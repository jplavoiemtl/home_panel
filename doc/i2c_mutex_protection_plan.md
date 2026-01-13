# Plan: I2C Mutex Protection for Touch Driver

## Overview

Add I2C mutex protection to the touch driver to prevent potential race conditions and add ghost touch filtering. This follows the proven pattern from the companion module.

## Risk Assessment: **Low Risk**

| Factor | Assessment |
|--------|------------|
| Single I2C device | Touch is the only I2C device (no IMU/PMIC like companion) |
| Single-threaded mode | LVGL runs from main loop, no threading conflicts |
| Non-blocking mutex | 10ms timeout prevents freezes if mutex unavailable |
| Fallback behavior | If mutex fails, touch read is skipped (graceful degradation) |

## Files to Modify

### 1. `esp_bsp.c` - Create mutex and add protection

**Changes:**

- Add global `i2c_mutex` variable
- Create mutex in `bsp_i2c_init()` after I2C driver init
- Export mutex for use in `lv_port.c`

```c
// Add near top of file (after includes)
SemaphoreHandle_t i2c_mutex = NULL;

// In bsp_i2c_init(), after i2c_driver_install():
i2c_mutex = xSemaphoreCreateRecursiveMutex();
if (i2c_mutex == NULL) {
    ESP_LOGW(TAG, "Failed to create I2C mutex");
}
```

### 2. `esp_bsp.h` - Declare extern mutex

**Changes:**

- Add extern declaration for `i2c_mutex`

```c
extern SemaphoreHandle_t i2c_mutex;
```

### 3. `lv_port.c` - Protect touch reads with mutex

**Changes:**

- Include `esp_bsp.h` to access `i2c_mutex`
- Wrap touch I2C operations in `lvgl_port_touchpad_read()` with mutex
- Add ghost touch filtering (check touch count before using coordinates)

```c
static void lvgl_port_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    // ... existing setup code ...

    if (touch_int) {
        // START MUTEX PROTECTION
        if (i2c_mutex && xSemaphoreTakeRecursive(i2c_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            esp_lcd_touch_read_data(touch_ctx->handle);

            bool touchpad_pressed = esp_lcd_touch_get_coordinates(
                touch_ctx->handle, touchpad_x, touchpad_y, NULL, &touchpad_cnt, 1);

            xSemaphoreGiveRecursive(i2c_mutex);
            // END MUTEX PROTECTION

            // Ghost touch filter: only process if valid touch count
            if (touchpad_pressed && touchpad_cnt > 0) {
                data->point.x = touchpad_x[0];
                data->point.y = touchpad_y[0];
                data->state = LV_INDEV_STATE_PRESSED;
            } else {
                data->state = LV_INDEV_STATE_RELEASED;
            }
        }
        // If mutex acquisition fails, skip this touch read (non-blocking)
    }
}
```

## Implementation Order

1. **esp_bsp.h**: Add extern declaration for `i2c_mutex`
2. **esp_bsp.c**: Add mutex variable and create it in `bsp_i2c_init()`
3. **lv_port.c**: Add mutex protection around touch reads

## Testing Plan

1. **Compile**: Verify no build errors
2. **Boot test**: Verify display initializes normally
3. **Touch test**: Test touch responsiveness on all buttons
4. **Long-term test**: Run for extended period to verify no regressions

## Implementation Date

January 2026
