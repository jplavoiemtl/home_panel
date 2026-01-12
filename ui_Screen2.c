// Home Panel Screen 2 (Image Display) - Placeholder
// Replace with SquareLine Studio generated files when available
// Display: 480x320 (after 90° rotation)

#include "ui.h"

// Screen and widget objects
lv_obj_t* ui_Screen2 = NULL;
lv_obj_t* ui_screen2Text = NULL;
lv_obj_t* ui_Button2 = NULL;
lv_obj_t* ui_imgScreen2Background = NULL;

// Track if image has been displayed (prevents accidental back during loading)
static bool imageDisplayed = false;

// Called by image_fetcher when image is ready
void ui_Screen2_setImageDisplayed(bool displayed) {
    imageDisplayed = displayed;
}

// Event wrapper for back button - only works after image is displayed
static void ui_event_Button2(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Only allow back navigation if image has been displayed
        if (imageDisplayed) {
            imageDisplayed = false;  // Reset for next time
            _ui_screen_change(&ui_Screen1, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Screen1_screen_init);
        }
    }
}

// Screen event handler (calls external handler from image_fetcher)
static void ui_event_Screen2(lv_event_t* e) {
    screen2_event_handler(e);
}

void ui_Screen2_screen_init(void)
{
    // Create screen with dark green background (visible while loading)
    ui_Screen2 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen2, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen2, lv_color_hex(0x1a1a2e), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_Screen2, 255, LV_PART_MAIN);

    // Loading text (centered, visible while fetching image)
    ui_screen2Text = lv_label_create(ui_Screen2);
    lv_obj_set_width(ui_screen2Text, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_screen2Text, LV_SIZE_CONTENT);
    lv_obj_align(ui_screen2Text, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(ui_screen2Text, "Loading image...");
    lv_obj_set_style_text_align(ui_screen2Text, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui_screen2Text, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_screen2Text, &lv_font_montserrat_24, LV_PART_MAIN);

    // Full-screen invisible button for tap-to-go-back
    ui_Button2 = lv_btn_create(ui_Screen2);
    lv_obj_set_size(ui_Button2, 480, 320);
    lv_obj_align(ui_Button2, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(ui_Button2, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_Button2, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(ui_Button2, 0, LV_PART_MAIN);
    lv_obj_clear_flag(ui_Button2, LV_OBJ_FLAG_SCROLLABLE);

    // Image widget (full screen, behind button for touch)
    ui_imgScreen2Background = lv_img_create(ui_Screen2);
    lv_obj_set_size(ui_imgScreen2Background, 480, 320);
    lv_obj_align(ui_imgScreen2Background, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(ui_imgScreen2Background, LV_OBJ_FLAG_SCROLLABLE);
    // Start transparent (image_fetcher will make visible when image loads)
    lv_obj_set_style_opa(ui_imgScreen2Background, LV_OPA_TRANSP, LV_PART_MAIN);

    // Move button to front so it receives touch events
    lv_obj_move_foreground(ui_Button2);

    // Add event callbacks
    lv_obj_add_event_cb(ui_Button2, ui_event_Button2, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_Screen2, ui_event_Screen2, LV_EVENT_ALL, NULL);
}
