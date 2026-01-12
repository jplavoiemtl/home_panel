#pragma once

#include <Arduino.h>
#include <lvgl.h>

struct ImageFetcherConfig {
  uint16_t screenWidth;
  uint16_t screenHeight;
  lv_obj_t* screen1;              // Home screen
  lv_obj_t* screen2;              // Image display screen
  lv_obj_t* imgScreen2Background; // Image widget on screen2
};

void imageFetcherInit(const ImageFetcherConfig& config);
void imageFetcherLoop();
bool requestLatestImage();

#ifdef __cplusplus
extern "C" {
#endif

void buttonLatest_event_handler(lv_event_t* e);
void buttonNew_event_handler(lv_event_t* e);
void buttonBack_event_handler(lv_event_t* e);
void screen2_event_handler(lv_event_t* e);

#ifdef __cplusplus
}
#endif
