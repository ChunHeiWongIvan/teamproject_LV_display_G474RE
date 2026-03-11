#include <string.h>

#include "screens.h"
#include "images.h"
#include "fonts.h"
#include "actions.h"
#include "vars.h"
#include "styles.h"
#include "ui.h"

#include <string.h>

objects_t objects;

//
// Event handlers
//

lv_obj_t *tick_value_change_obj;

//
// Screens
//

void create_screen_main_menu() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.main_menu = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 480, 320);
    {
        lv_obj_t *parent_obj = obj;
        {
            // Title
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.title = obj;
            lv_obj_set_pos(obj, 10, 10);
            lv_obj_set_size(obj, 361, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "MEng Project Team 6 \nWide output voltage range Li-ion battery charger");
        }
        {
            // start_charging_button
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.start_charging_button = obj;
            lv_obj_set_pos(obj, 50, 75);
            lv_obj_set_size(obj, 140, 80);
            lv_obj_add_event_cb(obj, action_start_charging_command, LV_EVENT_CLICKED, (void *)0);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xff00c853), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "Start \nCharging");
                }
            }
        }
        {
            // view_status_button
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.view_status_button = obj;
            lv_obj_set_pos(obj, 51, 204);
            lv_obj_set_size(obj, 140, 80);
            lv_obj_add_event_cb(obj, action_to_view_status, LV_EVENT_CLICKED, (void *)0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "View Status");
                }
            }
        }
        {
            // set_parameters_button
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.set_parameters_button = obj;
            lv_obj_set_pos(obj, 290, 75);
            lv_obj_set_size(obj, 140, 80);
            lv_obj_add_event_cb(obj, action_to_set_parameters, LV_EVENT_CLICKED, (void *)0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "Set Parameters");
                }
            }
        }
        {
            // view_logs_button
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.view_logs_button = obj;
            lv_obj_set_pos(obj, 290, 204);
            lv_obj_set_size(obj, 140, 80);
            lv_obj_add_event_cb(obj, action_to_view_logs, LV_EVENT_CLICKED, (void *)0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "View Logs");
                }
            }
        }
    }
    
    tick_screen_main_menu();
}

void tick_screen_main_menu() {
}

void create_screen_charging_menu() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.charging_menu = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 480, 320);
    {
        lv_obj_t *parent_obj = obj;
        {
            // Title_1
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.title_1 = obj;
            lv_obj_set_pos(obj, 183, 17);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "Charging Menu");
        }
        {
            // back_button_4
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.back_button_4 = obj;
            lv_obj_set_pos(obj, 10, 10);
            lv_obj_set_size(obj, 60, 30);
            lv_obj_add_event_cb(obj, action_to_main_menu, LV_EVENT_CLICKED, (void *)0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "Back");
                }
            }
        }
        {
            // output_voltage_chart
            lv_obj_t *obj = lv_chart_create(parent_obj);
            objects.output_voltage_chart = obj;
            lv_obj_set_pos(obj, 40, 68);
            lv_obj_set_size(obj, 340, 100);
        }
        {
            // output_current_chart
            lv_obj_t *obj = lv_chart_create(parent_obj);
            objects.output_current_chart = obj;
            lv_obj_set_pos(obj, 40, 199);
            lv_obj_set_size(obj, 340, 100);
        }
        {
            // output_voltage_scale
            lv_obj_t *obj = lv_scale_create(parent_obj);
            objects.output_voltage_scale = obj;
            lv_obj_set_pos(obj, 384, 82);
            lv_obj_set_size(obj, 30, 75);
            lv_scale_set_mode(obj, LV_SCALE_MODE_VERTICAL_RIGHT);
            lv_scale_set_range(obj, 0, 600);
            lv_scale_set_angle_range(obj, 270);
            lv_scale_set_rotation(obj, 135);
            lv_scale_set_total_tick_count(obj, 13);
            lv_scale_set_major_tick_every(obj, 2);
            lv_scale_set_label_show(obj, true);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_10, LV_PART_INDICATOR);
        }
        {
            // output_voltage_scale_1
            lv_obj_t *obj = lv_scale_create(parent_obj);
            objects.output_voltage_scale_1 = obj;
            lv_obj_set_pos(obj, 384, 213);
            lv_obj_set_size(obj, 30, 75);
            lv_scale_set_mode(obj, LV_SCALE_MODE_VERTICAL_RIGHT);
            lv_scale_set_range(obj, 0, 20);
            lv_scale_set_angle_range(obj, 270);
            lv_scale_set_rotation(obj, 135);
            lv_scale_set_total_tick_count(obj, 11);
            lv_scale_set_major_tick_every(obj, 2);
            lv_scale_set_label_show(obj, true);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_10, LV_PART_INDICATOR);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 422, 96);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Output\nVoltage\n(V)");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 422, 227);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Output\nCurrent\n(A)");
        }
        {
            // output_voltage_label
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.output_voltage_label = obj;
            lv_obj_set_pos(obj, 329, 52);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "350.0 V");
        }
        {
            // output_current_label
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.output_current_label = obj;
            lv_obj_set_pos(obj, 328, 183);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "20.00 A");
        }
    }
    
    tick_screen_charging_menu();
}

void tick_screen_charging_menu() {
}

void create_screen_fault_menu() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.fault_menu = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 480, 320);
    {
        lv_obj_t *parent_obj = obj;
        {
            // Title_2
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.title_2 = obj;
            lv_obj_set_pos(obj, 199, 17);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "Fault Menu");
        }
    }
    
    tick_screen_fault_menu();
}

void tick_screen_fault_menu() {
}

void create_screen_set_parameters__main_() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.set_parameters__main_ = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 480, 320);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER);
    {
        lv_obj_t *parent_obj = obj;
        {
            // Title_3
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.title_3 = obj;
            lv_obj_set_pos(obj, 184, 17);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "Set Parameters");
        }
        {
            lv_obj_t *obj = lv_buttonmatrix_create(parent_obj);
            lv_obj_set_pos(obj, 29, 57);
            lv_obj_set_size(obj, 240, 240);
            static const char *map[11] = {
                "+1 V",
                "+10 V",
                "+100 V",
                "\n",
                "-1 V",
                "-10 V",
                "-100 V",
                "\n",
                "Set min: \n200 V",
                "Set max: \n600 V",
                NULL,
            };
            lv_buttonmatrix_set_map(obj, map);
            lv_obj_add_event_cb(obj, action_set_parameters_keypad, LV_EVENT_VALUE_CHANGED, (void *)0);
        }
        {
            lv_obj_t *obj = lv_arc_create(parent_obj);
            lv_obj_set_pos(obj, 1285, 388);
            lv_obj_set_size(obj, 150, 150);
            lv_arc_set_value(obj, 25);
        }
        {
            // target_battery_voltage_bar
            lv_obj_t *obj = lv_bar_create(parent_obj);
            objects.target_battery_voltage_bar = obj;
            lv_obj_set_pos(obj, 301, 172);
            lv_obj_set_size(obj, 150, 10);
            lv_bar_set_value(obj, 50, LV_ANIM_OFF);
        }
        {
            // target_battery_voltage_label
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.target_battery_voltage_label = obj;
            lv_obj_set_pos(obj, 356, 194);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "400 V");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 306, 128);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "Full charge voltage");
        }
        {
            // back_button_1
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.back_button_1 = obj;
            lv_obj_set_pos(obj, 10, 10);
            lv_obj_set_size(obj, 60, 30);
            lv_obj_add_event_cb(obj, action_to_main_menu, LV_EVENT_CLICKED, (void *)0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "Back");
                }
            }
        }
    }
    
    tick_screen_set_parameters__main_();
}

void tick_screen_set_parameters__main_() {
}

void create_screen_view_status__main_() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.view_status__main_ = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 480, 320);
    {
        lv_obj_t *parent_obj = obj;
        {
            // Title_4
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.title_4 = obj;
            lv_obj_set_pos(obj, 198, 17);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "View Status");
        }
        {
            // back_button_2
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.back_button_2 = obj;
            lv_obj_set_pos(obj, 10, 10);
            lv_obj_set_size(obj, 60, 30);
            lv_obj_add_event_cb(obj, action_to_main_menu, LV_EVENT_CLICKED, (void *)0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "Back");
                }
            }
        }
    }
    
    tick_screen_view_status__main_();
}

void tick_screen_view_status__main_() {
}

void create_screen_view_logs__main_() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.view_logs__main_ = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 480, 320);
    {
        lv_obj_t *parent_obj = obj;
        {
            // Title_5
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.title_5 = obj;
            lv_obj_set_pos(obj, 204, 17);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text(obj, "View Logs");
        }
        {
            // back_button_3
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.back_button_3 = obj;
            lv_obj_set_pos(obj, 10, 10);
            lv_obj_set_size(obj, 60, 30);
            lv_obj_add_event_cb(obj, action_to_main_menu, LV_EVENT_CLICKED, (void *)0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "Back");
                }
            }
        }
        {
            // uart_logs
            lv_obj_t *obj = lv_textarea_create(parent_obj);
            objects.uart_logs = obj;
            lv_obj_set_pos(obj, 40, 59);
            lv_obj_set_size(obj, 400, 121);
            lv_textarea_set_max_length(obj, 257);
            lv_textarea_set_one_line(obj, false);
            lv_textarea_set_password_mode(obj, false);
        }
        {
            // clear_button
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.clear_button = obj;
            lv_obj_set_pos(obj, 166, 223);
            lv_obj_set_size(obj, 148, 52);
            lv_obj_add_event_cb(obj, action_clear_textarea, LV_EVENT_CLICKED, (void *)0);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xffff0000), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "Clear");
                }
            }
        }
    }
    
    tick_screen_view_logs__main_();
}

void tick_screen_view_logs__main_() {
}

typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_main_menu,
    tick_screen_charging_menu,
    tick_screen_fault_menu,
    tick_screen_set_parameters__main_,
    tick_screen_view_status__main_,
    tick_screen_view_logs__main_,
};
void tick_screen(int screen_index) {
    tick_screen_funcs[screen_index]();
}
void tick_screen_by_id(enum ScreensEnum screenId) {
    tick_screen_funcs[screenId - 1]();
}

//
// Fonts
//

ext_font_desc_t fonts[] = {
#if LV_FONT_MONTSERRAT_8
    { "MONTSERRAT_8", &lv_font_montserrat_8 },
#endif
#if LV_FONT_MONTSERRAT_10
    { "MONTSERRAT_10", &lv_font_montserrat_10 },
#endif
#if LV_FONT_MONTSERRAT_12
    { "MONTSERRAT_12", &lv_font_montserrat_12 },
#endif
#if LV_FONT_MONTSERRAT_14
    { "MONTSERRAT_14", &lv_font_montserrat_14 },
#endif
#if LV_FONT_MONTSERRAT_16
    { "MONTSERRAT_16", &lv_font_montserrat_16 },
#endif
#if LV_FONT_MONTSERRAT_18
    { "MONTSERRAT_18", &lv_font_montserrat_18 },
#endif
#if LV_FONT_MONTSERRAT_20
    { "MONTSERRAT_20", &lv_font_montserrat_20 },
#endif
#if LV_FONT_MONTSERRAT_22
    { "MONTSERRAT_22", &lv_font_montserrat_22 },
#endif
#if LV_FONT_MONTSERRAT_24
    { "MONTSERRAT_24", &lv_font_montserrat_24 },
#endif
#if LV_FONT_MONTSERRAT_26
    { "MONTSERRAT_26", &lv_font_montserrat_26 },
#endif
#if LV_FONT_MONTSERRAT_28
    { "MONTSERRAT_28", &lv_font_montserrat_28 },
#endif
#if LV_FONT_MONTSERRAT_30
    { "MONTSERRAT_30", &lv_font_montserrat_30 },
#endif
#if LV_FONT_MONTSERRAT_32
    { "MONTSERRAT_32", &lv_font_montserrat_32 },
#endif
#if LV_FONT_MONTSERRAT_34
    { "MONTSERRAT_34", &lv_font_montserrat_34 },
#endif
#if LV_FONT_MONTSERRAT_36
    { "MONTSERRAT_36", &lv_font_montserrat_36 },
#endif
#if LV_FONT_MONTSERRAT_38
    { "MONTSERRAT_38", &lv_font_montserrat_38 },
#endif
#if LV_FONT_MONTSERRAT_40
    { "MONTSERRAT_40", &lv_font_montserrat_40 },
#endif
#if LV_FONT_MONTSERRAT_42
    { "MONTSERRAT_42", &lv_font_montserrat_42 },
#endif
#if LV_FONT_MONTSERRAT_44
    { "MONTSERRAT_44", &lv_font_montserrat_44 },
#endif
#if LV_FONT_MONTSERRAT_46
    { "MONTSERRAT_46", &lv_font_montserrat_46 },
#endif
#if LV_FONT_MONTSERRAT_48
    { "MONTSERRAT_48", &lv_font_montserrat_48 },
#endif
};

//
// Color themes
//

uint32_t active_theme_index = 0;

//
//
//

void create_screens() {

// Set default LVGL theme
    lv_display_t *dispp = lv_display_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), false, LV_FONT_DEFAULT);
    lv_display_set_theme(dispp, theme);
    
    // Initialize screens
    // Create screens
    create_screen_main_menu();
    create_screen_charging_menu();
    create_screen_fault_menu();
    create_screen_set_parameters__main_();
    create_screen_view_status__main_();
    create_screen_view_logs__main_();
}