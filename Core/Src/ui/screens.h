#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_MAIN_MENU = 1,
    SCREEN_ID_CHARGING_MENU = 2,
    SCREEN_ID_FAULT_MENU = 3,
    SCREEN_ID_SET_PARAMETERS__MAIN_ = 4,
    SCREEN_ID_VIEW_STATUS__MAIN_ = 5,
    SCREEN_ID_VIEW_LOGS__MAIN_ = 6,
    _SCREEN_ID_LAST = 6
};

typedef struct _objects_t {
    lv_obj_t *main_menu;
    lv_obj_t *charging_menu;
    lv_obj_t *fault_menu;
    lv_obj_t *set_parameters__main_;
    lv_obj_t *view_status__main_;
    lv_obj_t *view_logs__main_;
    lv_obj_t *title;
    lv_obj_t *start_charging_button;
    lv_obj_t *view_status_button;
    lv_obj_t *set_parameters_button;
    lv_obj_t *view_logs_button;
    lv_obj_t *title_1;
    lv_obj_t *back_button_4;
    lv_obj_t *output_voltage_chart;
    lv_obj_t *output_current_chart;
    lv_obj_t *output_voltage_scale;
    lv_obj_t *output_voltage_scale_1;
    lv_obj_t *output_voltage_label;
    lv_obj_t *output_current_label;
    lv_obj_t *title_2;
    lv_obj_t *title_3;
    lv_obj_t *target_battery_voltage_bar;
    lv_obj_t *target_battery_voltage_label;
    lv_obj_t *back_button_1;
    lv_obj_t *title_4;
    lv_obj_t *back_button_2;
    lv_obj_t *title_5;
    lv_obj_t *back_button_3;
    lv_obj_t *uart_logs;
    lv_obj_t *clear_button;
} objects_t;

extern objects_t objects;

void create_screen_main_menu();
void tick_screen_main_menu();

void create_screen_charging_menu();
void tick_screen_charging_menu();

void create_screen_fault_menu();
void tick_screen_fault_menu();

void create_screen_set_parameters__main_();
void tick_screen_set_parameters__main_();

void create_screen_view_status__main_();
void tick_screen_view_status__main_();

void create_screen_view_logs__main_();
void tick_screen_view_logs__main_();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/