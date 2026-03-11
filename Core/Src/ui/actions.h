#ifndef EEZ_LVGL_UI_EVENTS_H
#define EEZ_LVGL_UI_EVENTS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void action_to_set_parameters(lv_event_t * e);
extern void action_set_parameters_keypad(lv_event_t * e);
extern void action_to_view_logs(lv_event_t * e);
extern void action_start_charging_command(lv_event_t * e);
extern void action_to_view_status(lv_event_t * e);
extern void action_to_main_menu(lv_event_t * e);
extern void action_clear_textarea(lv_event_t * e);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_EVENTS_H*/