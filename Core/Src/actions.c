#include "ui/actions.h"
#include "ui/screens.h"
#include "ui/vars.h"
#include "ui/ui.h"

extern const char * get_var_target_battery_voltage_text(void);

static int32_t clamp_i32(int32_t x, int32_t lo, int32_t hi) // Helper clamp function
{
    if(x < lo) return lo;
    if(x > hi) return hi;
    return x;
}

static int32_t voltage_to_bar(int32_t v) // Helper to convert voltage into bar percentage
{
    const int32_t V_MIN = 200;
    const int32_t V_MAX = 600;

    v = clamp_i32(v, V_MIN, V_MAX);

    // 0 to 100
    return (int32_t)((v - V_MIN) * 100 / (V_MAX - V_MIN));
}

void action_start_charging_command(lv_event_t * e)
{
    lv_screen_load(objects.charging_menu);
}

void action_to_set_parameters(lv_event_t * e)
{
    lv_screen_load(objects.set_parameters__main_);
}

void action_to_view_logs(lv_event_t * e)
{
    lv_screen_load(objects.view_logs__main_);
}

void action_to_view_status(lv_event_t * e)
{
    lv_screen_load(objects.view_status__main_);
}

void action_to_main_menu(lv_event_t * e)
{
    lv_screen_load(objects.main_menu);
}

void action_set_parameters_keypad(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code != LV_EVENT_VALUE_CHANGED) return;

    lv_obj_t * btnm = lv_event_get_target_obj(e);

    // 0-based ID, not counting "\n" in the map
    uint32_t id = lv_buttonmatrix_get_selected_button(btnm);
    if(id == LV_BUTTONMATRIX_BUTTON_NONE) return;

    int32_t v = get_var_target_battery_voltage();

    const int32_t V_MIN = 200; // Minimum full charge voltage of 200 V
    const int32_t V_MAX = 600; // Maximum full charge voltage of 600 V

    switch(id) {
        case 0:  v += 1;    break;     // +1 V
        case 1:  v += 10;   break;     // +10 V
        case 2:  v += 100;  break;     // +100 V
        case 3:  v -= 1;    break;     // -1 V
        case 4:  v -= 10;   break;     // -10 V
        case 5:  v -= 100;  break;     // -100 V

        case 6:  v = V_MIN; break;     // "Set min: 200 V"
        case 7:  v = V_MAX; break;     // "Set max: 600 V"

        default: break;
    }

    // Clamp
    v = clamp_i32(v, V_MIN, V_MAX);

    set_var_target_battery_voltage(v);

    // Update label
    lv_label_set_text(objects.target_battery_voltage_label,
                      get_var_target_battery_voltage_text());

    // Update bar (0 to 100)
    lv_bar_set_value(objects.target_battery_voltage_bar,
                     voltage_to_bar(v),
                     LV_ANIM_ON);
}

void action_clear_textarea(lv_event_t * e) {
	lv_textarea_set_text(objects.uart_logs, "");
}

