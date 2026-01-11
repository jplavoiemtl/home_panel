#include <Arduino.h>
#include <lvgl.h>
#include "display.h"
#include "esp_bsp.h"
#include "lv_port.h"
#include "SD_MMC.h"
#include "FS.h"
#include "pincfg.h"
#include <vector>
#include <ESP32_JPEG_Library.h>

#define PIC_FOLDER "/pic"
#define DEMO_LVGL 1
#define DEMO_PIC 2
#define DEMO_MJPEG 3
#define DEMO_MP3 4
#define CURR_DEMO DEMO_PIC 

/** Set the rotation degree:
 *      - 0: 0 degree
 *      - 90: 90 degree
 *      - 180: 180 degree
 *      - 270: 270 degree  */

#define LVGL_PORT_ROTATION_DEGREE (90)

std::vector<char *> v_fileContent;
File dir;
void listDir(fs::FS &fs, const char *dirname, uint8_t levels);

int curr_show_index = 0;

#define MAX_PIC_FILE_SIZE 307200 //= 480 * 320 * 2

static uint8_t *img_read_data = NULL;

static lv_obj_t *img_obj = NULL;
static uint16_t *pic_img_data = NULL;
static uint32_t last_display_time = 0;
const uint32_t IMAGE_CYCLE_DELAY_MS = 8000;
static lv_img_dsc_t pic_img_dsc = {
    .header = {
        .cf = LV_IMG_CF_TRUE_COLOR,
        .always_zero = 0,
        .reserved = 0,
        .w = 480,
        .h = 320,
    },
    .data_size = 480 * 320 * 2,
    .data = NULL,
};

const char picDir[] = PIC_FOLDER;

//*****************************************************************************************
// Decodes a single JPEG image from the input buffer into the output RGB565 buffer.
static jpeg_error_t esp_jpeg_decoder_one_image(uint8_t *input_buf, int len, uint8_t *output_buf) {
  jpeg_error_t ret = JPEG_ERR_OK;
  int inbuf_consumed = 0;

  // Generate default configuration
  jpeg_dec_config_t config = {
      .output_type = JPEG_RAW_TYPE_RGB565_BE,
      .rotate = JPEG_ROTATE_0D,
  };

  // Empty handle to jpeg_decoder
  jpeg_dec_handle_t *jpeg_dec = NULL;

  // Create jpeg_dec
  jpeg_dec = jpeg_dec_open(&config);

  // Create io_callback handle
  jpeg_dec_io_t *jpeg_io = (jpeg_dec_io_t *)calloc(1, sizeof(jpeg_dec_io_t));
  if (jpeg_io == NULL)
  {
    return JPEG_ERR_MEM;
  }

  // Create out_info handle
  jpeg_dec_header_info_t *out_info = (jpeg_dec_header_info_t *)calloc(1, sizeof(jpeg_dec_header_info_t));
  if (out_info == NULL)
  {
    return JPEG_ERR_MEM;
  }
  // Set input buffer and buffer len to io_callback
  jpeg_io->inbuf = input_buf;
  jpeg_io->inbuf_len = len;

  // Parse jpeg picture header and get picture for user and decoder
  ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
  if (ret < 0) {
    esp_rom_printf("JPEG decode parse failed\n");
    goto _exit;
  }

  jpeg_io->outbuf = output_buf;
  inbuf_consumed = jpeg_io->inbuf_len - jpeg_io->inbuf_remain;
  jpeg_io->inbuf = input_buf + inbuf_consumed;
  jpeg_io->inbuf_len = jpeg_io->inbuf_remain;

  // Start decode jpeg raw data
  ret = jpeg_dec_process(jpeg_dec, jpeg_io);
  if (ret < 0) {
    esp_rom_printf("JPEG decode process failed\n");
    goto _exit;
  }

_exit:
  // Decoder deinitialize
  jpeg_dec_close(jpeg_dec);
  free(out_info);
  free(jpeg_io);
  return ret;
}

//*****************************************************************************************
// Loads the next image from the SD card, decodes it, and updates the display.
void show_new_pic() {
  if (pic_img_data == NULL || img_read_data == NULL) {
      esp_rom_printf("Skipping show_new_pic: memory not allocated\n");
      return;
  }
  if (v_fileContent.size() > 0)
  {
    const char *s = (const char *)v_fileContent[curr_show_index];
    esp_rom_printf("showing %s\n", s);
    memset(img_read_data, 0, MAX_PIC_FILE_SIZE);
    File loadFile;
    loadFile = SD_MMC.open(s, FILE_READ);
    if (!loadFile)
    {
      return;
    }

    char buff[1024];
    size_t bytesRead = 0;
    while (loadFile.available() && bytesRead < MAX_PIC_FILE_SIZE)
    {
      size_t len = min((size_t)loadFile.available(), min(sizeof(buff), MAX_PIC_FILE_SIZE - bytesRead));
      loadFile.read((uint8_t *)buff, len);
      memcpy(img_read_data + bytesRead, buff, len);
      bytesRead += len;
    }
    loadFile.close();

    jpeg_error_t ret = JPEG_ERR_OK;

    ret = esp_jpeg_decoder_one_image(img_read_data, bytesRead, (uint8_t *)pic_img_data);
    if (ret != JPEG_ERR_OK)
    {
      esp_rom_printf("JPEG decode failed - %d\n", (int)ret);
      return;
    }

    bsp_display_lock(0);
    lv_obj_invalidate(img_obj);
    bsp_display_unlock();

    curr_show_index--;

    if (curr_show_index < 0)
    {
      curr_show_index = v_fileContent.size() - 1;
    }
  }
}

//*****************************************************************************************
// Recursively searches for files in the specified directory and adds them to the list.
void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  esp_rom_printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root)
  {
    esp_rom_printf("Failed to open directory\n");
    return;
  }
  if (!root.isDirectory())
  {
    esp_rom_printf("Not a directory\n");
    return;
  }

  File file = root.openNextFile();
  while (file)
  {
    if (file.isDirectory())
    {
      if (levels)
      {
        listDir(fs, file.path(), levels - 1);
      }
    }
    else
    {
      v_fileContent.insert(v_fileContent.begin(), strdup(file.path()));
    }
    file = root.openNextFile();
  }
  esp_rom_printf("found files : %d\n", v_fileContent.size());
  root.close();
  file.close();
}

//*****************************************************************************************
// Initializes hardware components, display, SD card, and loads the initial image.
void setup() {
  String title = "LVGL porting example";

  Serial.begin(115200);
  
  delay(3000); // Give serial monitor time to catch up: 1000
  esp_rom_printf("\n\n--- System Info ---\n");
  esp_rom_printf("Model: %s\n", ESP.getChipModel());
  esp_rom_printf("Heap Size: %d, Free: %d\n", ESP.getHeapSize(), ESP.getFreeHeap());
  esp_rom_printf("PSRAM Size: %d, Free: %d\n", ESP.getPsramSize(), ESP.getFreePsram());
  esp_rom_printf("-------------------\n");

  esp_rom_printf("Initialize tf card\n");
  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
  if (!SD_MMC.begin("/sdmmc", true, false, 20000))
  {
    esp_rom_printf("Card Mount Failed\n");
    while (1)
    {
    };
  }

  esp_rom_printf("Initialize panel device\n");
  uint32_t buffer_size = EXAMPLE_LCD_QSPI_H_RES * EXAMPLE_LCD_QSPI_V_RES;
  if (!psramFound()) {
      buffer_size = EXAMPLE_LCD_QSPI_H_RES * EXAMPLE_LCD_QSPI_V_RES / 10;
  }

  bsp_display_cfg_t cfg = {
      .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
      .buffer_size = buffer_size,
#if LVGL_PORT_ROTATION_DEGREE == 90
      .rotate = LV_DISP_ROT_90,
#elif LVGL_PORT_ROTATION_DEGREE == 270
      .rotate = LV_DISP_ROT_270,
#elif LVGL_PORT_ROTATION_DEGREE == 180
      .rotate = LV_DISP_ROT_180,
#elif LVGL_PORT_ROTATION_DEGREE == 0
      .rotate = LV_DISP_ROT_NONE,
#endif
  };

  bsp_display_start_with_config(&cfg);
  bsp_display_backlight_on();

  pic_img_data = (uint16_t *)heap_caps_aligned_alloc(16, 480 * 320 * 2, MALLOC_CAP_SPIRAM);
  if (!pic_img_data) {
      pic_img_data = (uint16_t *)malloc(480 * 320 * 2);
  }
  if (pic_img_data) {
      memset(pic_img_data, 0, 480 * 320 * 2);
  } else {
      esp_rom_printf("Failed to allocate pic_img_data\n");
  }

  img_read_data = (uint8_t *)heap_caps_malloc(MAX_PIC_FILE_SIZE, MALLOC_CAP_SPIRAM);
  if (!img_read_data) {
      img_read_data = (uint8_t *)malloc(MAX_PIC_FILE_SIZE);
  }
  if (img_read_data) {
      memset(img_read_data, 0, MAX_PIC_FILE_SIZE);
  } else {
      esp_rom_printf("Failed to allocate img_read_data\n");
  }

  pic_img_dsc.data = (uint8_t *)pic_img_data;
  bsp_display_lock(0);
  img_obj = lv_img_create(lv_scr_act());
  lv_img_set_src(img_obj, &pic_img_dsc);
  lv_obj_center(img_obj);
  bsp_display_unlock();

  dir = SD_MMC.open(picDir);
  listDir(SD_MMC, picDir, 1);

  if (v_fileContent.size() > 0) {
    curr_show_index = v_fileContent.size() - 1;
  }
  show_new_pic();

}

//*****************************************************************************************
// Main execution loop for image cycling and yielding.
void loop() {
  if (millis() - last_display_time >= IMAGE_CYCLE_DELAY_MS) {
    last_display_time = millis();
    show_new_pic();
  }
  delay(1);
}
