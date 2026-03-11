#include "ui/vars.h"
#include <stdio.h>

int32_t target_battery_voltage = 400;

int32_t get_var_target_battery_voltage() {
    return target_battery_voltage;
}

const char * get_var_target_battery_voltage_text(void)
{
    static char buf[16];
    snprintf(buf, sizeof(buf), "%ld V", (long)target_battery_voltage);
    return buf;
}

void set_var_target_battery_voltage(int32_t value) {
    target_battery_voltage = value;
}
