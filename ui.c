// Home Panel UI - Placeholder
// Replace with SquareLine Studio generated files when available

#include "ui.h"

// Global for tracking previous screen (used by image_fetcher for navigation)
lv_obj_t* ui_previous_screen = NULL;

void ui_init(void)
{
    // Initialize Screen 1 (Home Screen)
    ui_Screen1_screen_init();

    // Initialize Screen 2 (Image Display) - created but not loaded
    ui_Screen2_screen_init();

    // Load Screen 1 as the initial screen
    lv_disp_load_scr(ui_Screen1);
}
