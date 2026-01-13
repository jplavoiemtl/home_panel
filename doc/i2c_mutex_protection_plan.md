# I2C Mutex Protection for Touch Driver

## Overview
Add I2C mutex protection to the touch driver to prevent potential race conditions and add ghost touch filtering. This follows the proven pattern from the companion module.

## Risk Assessment: **Low Risk**

| Factor | Assessment |
|--------|------------|
| Single I2C device | Touch is the only I2C device (no IMU/PMIC like companion) |
| Single-threaded mode | LVGL runs from main loop, no threading conflicts |
| Non-blocking mutex | 10ms timeout prevents freezes if mutex unavailable |
| Fallback behavior | If mutex fails, touch read is skipped (graceful degradation) |

## Files Modified

### 1. `esp_bsp.h` - Declare extern mutex

Added FreeRTOS includes and extern declaration for `i2c_mutex`:

```c
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// I2C mutex for protecting touch I2C operations
extern SemaphoreHandle_t i2c_mutex;
```

### 2. `esp_bsp.c` - Create mutex variable and initialize

Added global mutex variable:
```c
// I2C mutex for protecting touch operations
SemaphoreHandle_t i2c_mutex = NULL;
```

Created mutex in `bsp_i2c_init()` after I2C driver installation:
```c
// Create I2C mutex for protecting touch operations
if (i2c_mutex == NULL) {
    i2c_mutex = xSemaphoreCreateRecursiveMutex();
    if (i2c_mutex == NULL) {
        ESP_LOGW(TAG, "Failed to create I2C mutex");
    }
}
```

### 3. `lv_port.c` - Protect touch reads with mutex

Added include for `esp_bsp.h` to access the mutex.

Modified `lvgl_port_touchpad_read()` to wrap I2C operations with mutex:
```c
if (touch_int) {
    /* START I2C MUTEX PROTECTION - prevents race conditions on I2C bus */
    if (i2c_mutex && xSemaphoreTakeRecursive(i2c_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        /* Read data from touch controller into memory */
        esp_lcd_touch_read_data(touch_ctx->handle);

        /* Get touch coordinates */
        bool touchpad_pressed = esp_lcd_touch_get_coordinates(...);

        xSemaphoreGiveRecursive(i2c_mutex);
        /* END I2C MUTEX PROTECTION */

        /* Ghost touch filter: only process if valid touch count */
        if (touchpad_pressed && touchpad_cnt > 0) {
            // Process touch
        }
    }
    /* If mutex acquisition fails, skip this touch read (non-blocking fallback) */
}
```

## Implementation Details

- **Recursive Mutex**: Using `xSemaphoreCreateRecursiveMutex()` allows the same task to acquire the mutex multiple times without deadlock
- **10ms Timeout**: Non-blocking with `pdMS_TO_TICKS(10)` - if mutex unavailable, touch read is skipped gracefully
- **Ghost Touch Filter**: Added check for `touchpad_cnt > 0` before processing coordinates
- **Null Check**: `if (i2c_mutex && ...)` ensures graceful fallback if mutex creation failed

## Testing Checklist

- [ ] Compile without errors
- [ ] Boot and display initializes normally
- [ ] Touch responsiveness on all buttons
- [ ] Long-term stability test
