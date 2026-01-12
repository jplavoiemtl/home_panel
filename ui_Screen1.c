// Home Panel Screen 1 (Home Screen) - Placeholder
// Replace with SquareLine Studio generated files when available
// Display: 480x320 (after 90° rotation)

#include "ui.h"

// Screen and widget objects
lv_obj_t* ui_Screen1 = NULL;
lv_obj_t* ui_labelConnectionStatus = NULL;
lv_obj_t* ui_labelPowerValue = NULL;
lv_obj_t* ui_labelEnergyValue = NULL;
lv_obj_t* ui_ActivitySpinner = NULL;
lv_obj_t* ui_ButtonLatest = NULL;
lv_obj_t* ui_ButtonBack = NULL;
lv_obj_t* ui_ButtonNew = NULL;

// Local labels for titles
static lv_obj_t* ui_labelPowerTitle = NULL;
static lv_obj_t* ui_labelEnergyTitle = NULL;

// Event wrapper for buttons
static void ui_event_ButtonLatest(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        buttonLatest_event_handler(e);
    }
}

static void ui_event_ButtonBack(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        buttonBack_event_handler(e);
    }
}

static void ui_event_ButtonNew(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        buttonNew_event_handler(e);
    }
}

void ui_Screen1_screen_init(void)
{
    // Create screen with black background
    ui_Screen1 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen1, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_Screen1, 255, LV_PART_MAIN);

    // Connection status label (top center)
    ui_labelConnectionStatus = lv_label_create(ui_Screen1);
    lv_obj_set_width(ui_labelConnectionStatus, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_labelConnectionStatus, LV_SIZE_CONTENT);
    lv_obj_align(ui_labelConnectionStatus, LV_ALIGN_TOP_MID, 0, 10);
    lv_label_set_text(ui_labelConnectionStatus, "Connecting...");
    lv_obj_set_style_text_color(ui_labelConnectionStatus, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_labelConnectionStatus, &lv_font_montserrat_16, LV_PART_MAIN);

    // Activity spinner (top left)
    ui_ActivitySpinner = lv_spinner_create(ui_Screen1, 1000, 90);
    lv_obj_set_width(ui_ActivitySpinner, 40);
    lv_obj_set_height(ui_ActivitySpinner, 40);
    lv_obj_align(ui_ActivitySpinner, LV_ALIGN_TOP_LEFT, 15, 10);
    lv_obj_clear_flag(ui_ActivitySpinner, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(ui_ActivitySpinner, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ui_ActivitySpinner, lv_color_hex(0x404040), LV_PART_MAIN);
    lv_obj_set_style_arc_color(ui_ActivitySpinner, lv_color_hex(0x00FF00), LV_PART_INDICATOR);
    lv_obj_add_flag(ui_ActivitySpinner, LV_OBJ_FLAG_HIDDEN);  // Hidden by default

    // Power section
    ui_labelPowerTitle = lv_label_create(ui_Screen1);
    lv_obj_align(ui_labelPowerTitle, LV_ALIGN_LEFT_MID, 20, -60);
    lv_label_set_text(ui_labelPowerTitle, "Power");
    lv_obj_set_style_text_color(ui_labelPowerTitle, lv_color_hex(0x808080), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_labelPowerTitle, &lv_font_montserrat_18, LV_PART_MAIN);

    ui_labelPowerValue = lv_label_create(ui_Screen1);
    lv_obj_align(ui_labelPowerValue, LV_ALIGN_LEFT_MID, 20, -25);
    lv_label_set_text(ui_labelPowerValue, "-- kW");
    lv_obj_set_style_text_color(ui_labelPowerValue, lv_color_hex(0xE9B804), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_labelPowerValue, &lv_font_montserrat_36, LV_PART_MAIN);

    // Energy section
    ui_labelEnergyTitle = lv_label_create(ui_Screen1);
    lv_obj_align(ui_labelEnergyTitle, LV_ALIGN_LEFT_MID, 20, 25);
    lv_label_set_text(ui_labelEnergyTitle, "Energy");
    lv_obj_set_style_text_color(ui_labelEnergyTitle, lv_color_hex(0x808080), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_labelEnergyTitle, &lv_font_montserrat_18, LV_PART_MAIN);

    ui_labelEnergyValue = lv_label_create(ui_Screen1);
    lv_obj_align(ui_labelEnergyValue, LV_ALIGN_LEFT_MID, 20, 60);
    lv_label_set_text(ui_labelEnergyValue, "-- kWh");
    lv_obj_set_style_text_color(ui_labelEnergyValue, lv_color_hex(0xE9B804), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_labelEnergyValue, &lv_font_montserrat_36, LV_PART_MAIN);

    // Camera buttons (bottom row)
    int btn_width = 100;
    int btn_height = 80;
    int btn_y = 120;  // Distance from center

    // Back button (left)
    ui_ButtonBack = lv_btn_create(ui_Screen1);
    lv_obj_set_size(ui_ButtonBack, btn_width, btn_height);
    lv_obj_align(ui_ButtonBack, LV_ALIGN_CENTER, -140, btn_y);
    lv_obj_set_style_bg_color(ui_ButtonBack, lv_color_hex(0x2196F3), LV_PART_MAIN);
    lv_obj_set_style_radius(ui_ButtonBack, 10, LV_PART_MAIN);

    lv_obj_t* label_back = lv_label_create(ui_ButtonBack);
    lv_label_set_text(label_back, LV_SYMBOL_LEFT "\nBack");
    lv_obj_set_style_text_align(label_back, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(label_back);

    // Latest button (center)
    ui_ButtonLatest = lv_btn_create(ui_Screen1);
    lv_obj_set_size(ui_ButtonLatest, btn_width, btn_height);
    lv_obj_align(ui_ButtonLatest, LV_ALIGN_CENTER, 0, btn_y);
    lv_obj_set_style_bg_color(ui_ButtonLatest, lv_color_hex(0x4CAF50), LV_PART_MAIN);
    lv_obj_set_style_radius(ui_ButtonLatest, 10, LV_PART_MAIN);

    lv_obj_t* label_latest = lv_label_create(ui_ButtonLatest);
    lv_label_set_text(label_latest, LV_SYMBOL_IMAGE "\nLatest");
    lv_obj_set_style_text_align(label_latest, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(label_latest);

    // New button (right)
    ui_ButtonNew = lv_btn_create(ui_Screen1);
    lv_obj_set_size(ui_ButtonNew, btn_width, btn_height);
    lv_obj_align(ui_ButtonNew, LV_ALIGN_CENTER, 140, btn_y);
    lv_obj_set_style_bg_color(ui_ButtonNew, lv_color_hex(0xFF9800), LV_PART_MAIN);
    lv_obj_set_style_radius(ui_ButtonNew, 10, LV_PART_MAIN);

    lv_obj_t* label_new = lv_label_create(ui_ButtonNew);
    lv_label_set_text(label_new, LV_SYMBOL_REFRESH "\nNew");
    lv_obj_set_style_text_align(label_new, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(label_new);

    // Add event callbacks
    lv_obj_add_event_cb(ui_ButtonLatest, ui_event_ButtonLatest, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_ButtonBack, ui_event_ButtonBack, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_ButtonNew, ui_event_ButtonNew, LV_EVENT_ALL, NULL);
}
