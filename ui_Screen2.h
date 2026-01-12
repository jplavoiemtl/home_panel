// Home Panel Screen 2 (Image Display) - Placeholder
// Replace with SquareLine Studio generated files when available

#ifndef UI_SCREEN2_H
#define UI_SCREEN2_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// SCREEN: ui_Screen2 (Image Display Screen)
void ui_Screen2_screen_init(void);

// Call to enable/disable back button (prevents accidental back during loading)
void ui_Screen2_setImageDisplayed(bool displayed);

extern lv_obj_t* ui_Screen2;
extern lv_obj_t* ui_screen2Text;
extern lv_obj_t* ui_Button2;
extern lv_obj_t* ui_imgScreen2Background;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
