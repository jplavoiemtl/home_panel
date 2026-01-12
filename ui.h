// Home Panel UI - Placeholder
// Replace with SquareLine Studio generated files when available
// Display: 320x480 (rotated 90° = 480x320 visible)

#ifndef _HOME_PANEL_UI_H
#define _HOME_PANEL_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "ui_helpers.h"
#include "ui_events.h"

///////////////////// SCREENS ////////////////////

#include "ui_Screen1.h"
#include "ui_Screen2.h"

///////////////////// VARIABLES ////////////////////

// Global for tracking previous screen (used by image_fetcher for navigation)
extern lv_obj_t* ui_previous_screen;

// UI INIT
void ui_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
