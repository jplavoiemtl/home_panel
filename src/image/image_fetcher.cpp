#include "image_fetcher.h"

#include <HTTPClient.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "secrets_private.h"
#include "../net/net_module.h"
#include "ui.h"
#include "../ui_custom.h"  // Custom UI extensions (not overwritten by SquareLine Studio)
#include "../screen/screen_power.h"  // Screen power management
#include "../time/time_service.h"  // For pausing timer during image display

// Use Serial for debug output
#define USBSerial Serial

namespace {

enum ImageRequestState {
  HTTP_IDLE,
  HTTP_REQUESTING,
  HTTP_RECEIVING,
  HTTP_DECODING,
  HTTP_COMPLETE,
  HTTP_ERROR
};

// --- HTTP/S configuration ---
constexpr unsigned long HTTP_TIMEOUT_MS = 30000;  // 30 seconds for camera capture
constexpr size_t MAX_JPEG_SIZE = 60000;  // 60 KB

// --- Screen 2 timeout management ---
constexpr unsigned long SCREEN2_LOADING_TIMEOUT = 30000;  // 30 seconds (allow time for download)
constexpr unsigned long SCREEN2_DISPLAY_TIMEOUT = 60000;  // 1 minute

// State
volatile bool cleanupInProgress = false;  // Flag to stop buffer access during cleanup
ImageRequestState httpState = HTTP_IDLE;
HTTPClient httpClient;
WiFiClientSecure httpsClient;

uint16_t* image_buffer_psram = nullptr;
lv_img_dsc_t img_dsc{};
uint8_t* jpeg_buffer_psram = nullptr;
size_t jpeg_buffer_size = 0;
size_t jpeg_bytes_received = 0;
unsigned long httpRequestStartTime = 0;
bool requestInProgress = false;

unsigned long screenTransitionTime = 0;
bool screen2TimeoutActive = false;
bool imageDisplayTimeoutActive = false;
unsigned long imageDisplayStartTime = 0;

ImageFetcherConfig cfg{};

// Asynchronous request tracking
const char* pendingEndpoint = nullptr;

}  // namespace

// Forward declarations
static void cleanupImageRequest();
static void prepareForRequest();
static bool requestImage(const char* endpoint_type);
static void processHTTPResponse();
static bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
static void button2_pressed_handler(lv_event_t* e);


// External dependencies
// (USBSerial defined as Serial macro above)
extern bool isWifiAvailable();  // Defined in home_panel.ino - checks WiFi recovery state

// Handler for Button2 press - disables touch to prevent carryover to Screen1
static void button2_pressed_handler(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_PRESSED) return;

  // Disable touch input immediately when Button2 is pressed
  lv_indev_t* indev = lv_indev_get_next(NULL);
  if (indev) lv_indev_enable(indev, false);

  // Re-enable touch input after debounce period (300ms)
  lv_timer_create([](lv_timer_t* timer) {
    lv_indev_t* indev = lv_indev_get_next(NULL);
    if (indev) lv_indev_enable(indev, true);
    lv_timer_del(timer);  // One-shot timer
  }, 300, NULL);
}

//***************************************************************************************************
void imageFetcherInit(const ImageFetcherConfig& config) {
  cfg = config;
  httpState = HTTP_IDLE;
  requestInProgress = false;
  screen2TimeoutActive = false;
  imageDisplayTimeoutActive = false;
  pendingEndpoint = nullptr;

  // Swap bytes to match LVGL's expected RGB565 format
  TJpgDec.setSwapBytes(true);

  // Ensure descriptor is zeroed
  memset(&img_dsc, 0, sizeof(img_dsc));

  // Attach screen2 event handler for SCREEN_LOADED and SCREEN_UNLOAD_START events
  if (cfg.screen2) {
    lv_obj_add_event_cb(cfg.screen2, screen2_event_handler, LV_EVENT_ALL, NULL);
  }

  // Attach Button2 press handler for touch debounce when exiting Screen2
  extern lv_obj_t* ui_Button2;
  if (ui_Button2) {
    lv_obj_add_event_cb(ui_Button2, button2_pressed_handler, LV_EVENT_PRESSED, NULL);
  }
}

//***************************************************************************************************
static void cleanupImageRequest() {

  // 0. Set flag FIRST to stop any buffer access in processHTTPResponse
  cleanupInProgress = true;
  httpState = HTTP_IDLE;  // Set state early to prevent further processing

  // Reset back button state
  ui_Screen2_setImageDisplayed(false);

  // 1. Stop HTTP connections
  httpClient.end();
  httpsClient.stop();
  delay(100);  // Give WiFi stack time to properly close connections

  // 2. Hide image if it exists to avoid LVGL accessing freed memory
  if (cfg.imgScreen2Background) {
    lv_obj_set_style_opa(cfg.imgScreen2Background, LV_OPA_TRANSP, LV_PART_MAIN);
  }

  // 3. Free buffers
  if (jpeg_buffer_psram) {
    free(jpeg_buffer_psram);
    jpeg_buffer_psram = nullptr;
  }
  if (image_buffer_psram) {
    free(image_buffer_psram);
    image_buffer_psram = nullptr;
  }

  // 4. Reset descriptors and states
  memset(&img_dsc, 0, sizeof(img_dsc));
  screen2TimeoutActive = false;
  imageDisplayTimeoutActive = false;

  // 5. Clear cleanup flag
  cleanupInProgress = false;
}

//***************************************************************************************************
// Helper to return to Screen 1 on error or timeout
static void returnToScreen1(const char* reason) {
  USBSerial.printf("Returning to Screen 1: %s\n", reason);

  if (httpState != HTTP_IDLE && httpState != HTTP_COMPLETE) {
    cleanupImageRequest();
    httpState = HTTP_ERROR;
  }

  requestInProgress = false;
  screen2TimeoutActive = false;
  imageDisplayTimeoutActive = false;

  time_service_resume();
  if (ui_previous_screen) {
    lv_disp_load_scr(ui_previous_screen);
  } else if (cfg.screen1) {
    lv_disp_load_scr(cfg.screen1);
  }
}

//***************************************************************************************************
static void prepareForRequest() {
  cleanupImageRequest();

  // Pause time service timer to prevent LVGL conflicts during image display
  time_service_pause();

  // Disable Button2 clicks temporarily to prevent touch carryover from triggering
  // an unwanted screen change (user's finger may still be down from pressing buttons)
  extern lv_obj_t* ui_Button2;
  if (ui_Button2) {
    lv_obj_clear_flag(ui_Button2, LV_OBJ_FLAG_CLICKABLE);
  }

  // Save the current screen and transition to Screen2
  lv_obj_t* current = lv_scr_act();
  if (current != cfg.screen2 && cfg.screen2) {
      ui_previous_screen = current;
      lv_disp_load_scr(cfg.screen2);
  } else if (current == cfg.screen2 && ui_previous_screen == NULL && cfg.screen1) {
       // Safety fallback if already on Screen2 without previous screen set
       ui_previous_screen = cfg.screen1;
  }

  // NOTE: Display rotation stays at 90° throughout - do not change it.
  // Toggling rotation during screen transitions causes display corruption.

  // Reset loading timeout state
  screenTransitionTime = millis();
  screen2TimeoutActive = true;
  imageDisplayTimeoutActive = false;
  requestInProgress = true;

  // Force an immediate UI refresh so the rotation and "Loading" state are visible
  // BEFORE we potentially block on the network request in the next loop.
  lv_refr_now(NULL);
}

//***************************************************************************************************
void imageFetcherLoop() {
  // Handle asynchronous request triggering
  if (pendingEndpoint != nullptr) {
    const char* endpoint = pendingEndpoint;
    pendingEndpoint = nullptr; // Clear it immediately
    if (!requestImage(endpoint)) {
      returnToScreen1("HTTP request failed to initiate");
    }
    return;
  }

  processHTTPResponse();

  // Handle Screen 2 timeouts
  if (cfg.screen2 && lv_scr_act() == cfg.screen2) {
    // Loading timeout
    if (screen2TimeoutActive && millis() - screenTransitionTime > SCREEN2_LOADING_TIMEOUT) {
      returnToScreen1("image loading took too long");
    }

    // Display timeout (after successful load)
    if (imageDisplayTimeoutActive) {
      unsigned long elapsed = millis() - imageDisplayStartTime;
      if (elapsed > SCREEN2_DISPLAY_TIMEOUT) {
        returnToScreen1("display timeout (1 minute elapsed)");
      }
    }
  }
}

//***************************************************************************************************
static bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  // Return 1 (success) even for out-of-bounds to allow decode to continue
  // This handles mismatched image/screen resolutions gracefully
  if (!image_buffer_psram) return 1;  // Still return success to continue decode

  // Skip blocks entirely outside screen bounds
  if (y >= cfg.screenHeight || x >= cfg.screenWidth) return 1;

  for (uint16_t row = 0; row < h; row++) {
    if ((y + row) >= cfg.screenHeight) break;
    for (uint16_t col = 0; col < w; col++) {
      if ((x + col) >= cfg.screenWidth) break;
      uint32_t dstIndex = static_cast<uint32_t>(y + row) * cfg.screenWidth + (x + col);
      image_buffer_psram[dstIndex] = bitmap[static_cast<uint32_t>(row) * w + col];
    }
  }
  return 1;
}

//***************************************************************************************************
static bool requestImage(const char* endpoint_type) {
  USBSerial.printf("=== requestImage('%s') START ===\n", endpoint_type);

  if (WiFi.status() != WL_CONNECTED) {
    USBSerial.println("WiFi not connected, cannot make HTTP request.");
    return false;
  }

  String url;
  // Use MQTT server selection to determine image server (LOCAL=HTTP, REMOTE=HTTPS)
  bool useRemoteServer = (netGetCurrentMqttServer() == MQTT_SERVER_REMOTE);

  if (useRemoteServer) {
    url = String(IMAGE_SERVER_REMOTE) + String(endpoint_type) + "?token=" + String(API_TOKEN);
    USBSerial.println("Initiating HTTPS GET: " + url);
    httpsClient.setCACert(remote_server_ca_cert);
    bool beginResult = httpClient.begin(httpsClient, url);
    if (!beginResult) {
      USBSerial.println("FATAL: httpClient.begin() failed for HTTPS!");
      httpState = HTTP_ERROR;
      return false;
    }
  } else {
    url = String(IMAGE_SERVER_BASE) + String(endpoint_type) + "?token=" + String(API_TOKEN);
    // USBSerial.println("Initiating HTTP GET: " + url);
    bool beginResult = httpClient.begin(url);
    if (!beginResult) {
      USBSerial.println("FATAL: httpClient.begin() failed for HTTP!");
      httpState = HTTP_ERROR;
      return false;
    }
  }

  httpClient.setTimeout(HTTP_TIMEOUT_MS);
  httpClient.setConnectTimeout(8000);

  httpState = HTTP_REQUESTING;
  httpRequestStartTime = millis();
  USBSerial.println("Sending HTTP GET...");

  int httpCode = httpClient.GET();

  if (httpCode != HTTP_CODE_OK) {
    USBSerial.printf("FATAL: HTTP GET failed with code: %d\n", httpCode);
    httpClient.end();
    httpState = HTTP_ERROR;
    return false;
  }

  int contentLength = httpClient.getSize();
  USBSerial.print("Response received, Content-Length: ");
  USBSerial.println(contentLength);

  if (contentLength <= 0 || contentLength > static_cast<int>(MAX_JPEG_SIZE)) {
    USBSerial.println("Invalid or too large content length");
    httpClient.end();
    httpState = HTTP_ERROR;
    return false;
  }

  jpeg_buffer_psram = static_cast<uint8_t*>(ps_malloc(contentLength));
  if (!jpeg_buffer_psram) {
    USBSerial.println("FATAL: Failed to allocate PSRAM for JPEG buffer");
    httpClient.end();
    httpState = HTTP_ERROR;
    return false;
  }

  jpeg_buffer_size = contentLength;
  jpeg_bytes_received = 0;
  httpState = HTTP_RECEIVING;

  USBSerial.println("Starting to receive image data...");
  return true;
}

//***************************************************************************************************
static void processHTTPResponse() {
  static bool timeoutMessageShown = false;

  // Safety check: abort if cleanup is in progress or state is idle
  if (cleanupInProgress || httpState == HTTP_IDLE || httpState == HTTP_COMPLETE) {
    timeoutMessageShown = false;
    return;
  }

  if (millis() - httpRequestStartTime > HTTP_TIMEOUT_MS) {
    if (!timeoutMessageShown) {
      USBSerial.println("HTTP request timed out!");
      timeoutMessageShown = true;
    }
    cleanupImageRequest();
    httpState = HTTP_ERROR;
    return;
  }

  if (httpState == HTTP_RECEIVING) {
    // Safety check: ensure buffer is still valid
    if (!jpeg_buffer_psram || cleanupInProgress) {
      USBSerial.println("Buffer invalid during receive, aborting.");
      httpState = HTTP_IDLE;
      return;
    }

    WiFiClient* stream = httpClient.getStreamPtr();
    if (!stream) {
      USBSerial.println("Stream invalid, aborting.");
      httpState = HTTP_IDLE;
      return;
    }

    while (stream->available() && jpeg_bytes_received < jpeg_buffer_size) {
      // Check again inside loop in case cleanup happened
      if (!jpeg_buffer_psram || cleanupInProgress) {
        USBSerial.println("Buffer freed during receive, aborting.");
        httpState = HTTP_IDLE;
        return;
      }

      size_t bytesToRead =
          min(static_cast<size_t>(stream->available()), jpeg_buffer_size - jpeg_bytes_received);
      size_t bytesRead = stream->readBytes(jpeg_buffer_psram + jpeg_bytes_received, bytesToRead);
      jpeg_bytes_received += bytesRead;

      // Yield to LVGL but DON'T call lv_timer_handler here - it can cause reentrancy issues
      delay(1);
    }

    if (jpeg_bytes_received >= jpeg_buffer_size) {
      USBSerial.println("Image download complete. Starting decode...");
      httpClient.end();
      httpState = HTTP_DECODING;
    }
    return;
  }

  if (httpState == HTTP_DECODING) {
    // Safety check before decoding
    if (cleanupInProgress || !jpeg_buffer_psram) {
      USBSerial.println("Cleanup in progress or buffer invalid, skipping decode.");
      httpState = HTTP_IDLE;
      return;
    }

    if (image_buffer_psram) {
      free(image_buffer_psram);
      image_buffer_psram = nullptr;
    }

    size_t imageBufferSize =
        static_cast<size_t>(cfg.screenWidth) * cfg.screenHeight * sizeof(uint16_t);

    image_buffer_psram = static_cast<uint16_t*>(ps_malloc(imageBufferSize));

    if (!image_buffer_psram) {
      USBSerial.println("FATAL: PSRAM allocation failed for decoded image buffer");
      cleanupImageRequest();
      httpState = HTTP_ERROR;
      return;
    }

    // Clear buffer to black before decoding (prevents garbage if image doesn't fill screen)
    memset(image_buffer_psram, 0, imageBufferSize);

    TJpgDec.setJpgScale(1);
    TJpgDec.setCallback(tft_output);

    // Get JPEG dimensions for debugging
    uint16_t jpgWidth = 0, jpgHeight = 0;
    TJpgDec.getJpgSize(&jpgWidth, &jpgHeight, jpeg_buffer_psram, jpeg_buffer_size);
    USBSerial.printf("JPEG dimensions: %dx%d, Screen: %dx%d\n",
                     jpgWidth, jpgHeight, cfg.screenWidth, cfg.screenHeight);

    uint8_t result = TJpgDec.drawJpg(0, 0, jpeg_buffer_psram, jpeg_buffer_size);

    free(jpeg_buffer_psram);
    jpeg_buffer_psram = nullptr;

    if (result != 0) {
      USBSerial.println("TJpgDec error code: " + String(result));
      if (image_buffer_psram) { free(image_buffer_psram); image_buffer_psram = nullptr; }
      httpState = HTTP_ERROR;
      return;
    }

    USBSerial.println("JPEG decoded successfully into PSRAM.");

    // NOTE: Display rotation stays at 90 degrees throughout (set in setup).
    // The raw image buffer (480x320) displays correctly with LVGL's 90° rotation.
    // Do NOT toggle rotation here - it causes screen transition corruption.

    img_dsc.header.always_zero = 0;
    img_dsc.header.w = cfg.screenWidth;
    img_dsc.header.h = cfg.screenHeight;
    img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    img_dsc.data_size = cfg.screenWidth * cfg.screenHeight * LV_COLOR_DEPTH / 8;
    img_dsc.data = reinterpret_cast<const uint8_t*>(image_buffer_psram);

    // Update LVGL image (single-threaded mode, no lock needed)
    if (cfg.imgScreen2Background) {
      lv_img_set_src(cfg.imgScreen2Background, &img_dsc);
      lv_obj_set_style_opa(cfg.imgScreen2Background, LV_OPA_COVER, LV_PART_MAIN);
    }
    USBSerial.println("LVGL image source updated.");

    // Enable back button now that image is displayed
    ui_Screen2_setImageDisplayed(true);

    // Re-enable Button2 clicks after a delay to let any queued touch events clear.
    // This prevents touch carryover from the original button press.
    extern lv_obj_t* ui_Button2;
    if (ui_Button2) {
      lv_timer_create([](lv_timer_t* timer) {
        extern lv_obj_t* ui_Button2;
        if (ui_Button2) {
          lv_obj_add_flag(ui_Button2, LV_OBJ_FLAG_CLICKABLE);
        }
        lv_timer_del(timer);  // One-shot timer
      }, 500, NULL);  // 500ms delay
    }

    httpState = HTTP_COMPLETE;
    requestInProgress = false;
    screen2TimeoutActive = false;

    imageDisplayTimeoutActive = true;
    imageDisplayStartTime = millis();
    return;
  }

  if (httpState == HTTP_ERROR) {
    httpState = HTTP_IDLE;
    returnToScreen1("HTTP error during request");
    return;
  }
}

//***************************************************************************************************
bool requestLatestImage() {
  // Block request if WiFi is recovering
  if (!isWifiAvailable()) {
    USBSerial.println("WiFi not available (recovering), ignoring image request");
    return false;
  }

  lv_obj_t* current_screen = lv_scr_act();
  // Home Panel only has screen1 and screen2
  if (current_screen != cfg.screen1 && current_screen != cfg.screen2) {
    USBSerial.println("On unsupported screen, ignoring image request");
    return false;
  }

  // Wake screen for incoming image (handles MQTT-triggered requests)
  screenPowerActivity();

  USBSerial.println("Initiating async latest image request...");
  prepareForRequest();
  pendingEndpoint = "latest";
  return true;
}

//***************************************************************************************************
void buttonLatest_event_handler(lv_event_t* e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    USBSerial.println("Latest button clicked");
    requestLatestImage();
  }
}

//***************************************************************************************************
void buttonNew_event_handler(lv_event_t* e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    if (!isWifiAvailable()) {
      USBSerial.println("New button clicked but WiFi not available (recovering)");
      return;
    }
    USBSerial.println("New button clicked, initiating async request...");
    prepareForRequest();
    pendingEndpoint = "new";
  }
}

//***************************************************************************************************
void buttonBack_event_handler(lv_event_t* e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    if (!isWifiAvailable()) {
      USBSerial.println("Back button clicked but WiFi not available (recovering)");
      return;
    }
    USBSerial.println("Back button clicked, initiating async request...");
    prepareForRequest();
    pendingEndpoint = "back";
  }
}

//***************************************************************************************************
void screen2_event_handler(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_SCREEN_LOADED) {
    if (httpState != HTTP_COMPLETE) {
      // Hide image while loading
      if (cfg.imgScreen2Background) {
        lv_obj_set_style_opa(cfg.imgScreen2Background, LV_OPA_TRANSP, LV_PART_MAIN);
      }
    } else {
      // Image already loaded - show it (rotation stays at 90°)
      if (cfg.imgScreen2Background) {
        lv_obj_set_style_opa(cfg.imgScreen2Background, LV_OPA_COVER, LV_PART_MAIN);
      }
    }
    // Only reset timeout if no request is in progress
    // (prevents extending timeout when screen loads after blocking HTTP call)
    if (!requestInProgress) {
      screenTransitionTime = millis();
      screen2TimeoutActive = true;
    }
    if (httpState != HTTP_COMPLETE) {
      imageDisplayTimeoutActive = false;
    }
  } else if (code == LV_EVENT_SCREEN_UNLOAD_START) {
    USBSerial.println("Screen 2 Unloading: Stopping HTTP and freeing buffers.");

    // Set cleanup flag FIRST to stop any buffer access
    cleanupInProgress = true;
    httpState = HTTP_IDLE;
    screen2TimeoutActive = false;
    imageDisplayTimeoutActive = false;

    // Reset back button state
    ui_Screen2_setImageDisplayed(false);

    // Re-enable Button2 clicks (was disabled to prevent touch carryover)
    extern lv_obj_t* ui_Button2;
    if (ui_Button2) {
      lv_obj_add_flag(ui_Button2, LV_OBJ_FLAG_CLICKABLE);
    }

    // Stop HTTP connections
    httpClient.end();
    httpsClient.stop();

    if (cfg.imgScreen2Background) {
      lv_obj_set_style_opa(cfg.imgScreen2Background, LV_OPA_TRANSP, LV_PART_MAIN);
    }
    if (image_buffer_psram != nullptr) {
      free(image_buffer_psram);
      image_buffer_psram = nullptr;
    }
    if (jpeg_buffer_psram != nullptr) {
      free(jpeg_buffer_psram);
      jpeg_buffer_psram = nullptr;
    }
    memset(&img_dsc, 0, sizeof(lv_img_dsc_t));
    requestInProgress = false;

    // NOTE: No rotation change needed - display stays at 90° throughout.
    // Toggling rotation during screen transitions causes display corruption.

    // Resume time service timer now that we're returning to Screen1
    time_service_resume();

    // Clear cleanup flag
    cleanupInProgress = false;
  }
}
