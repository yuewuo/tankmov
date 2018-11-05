#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "main.h"

#include "esp_attr.h"
#include "soc/rtc.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"

static const char *TAG = "motor";

void motor_gpio_initialize() {
    printf("initializing mcpwm gpio...\n");
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, M1P);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, M1N);
    mcpwm_gpio_init(MCPWM_UNIT_1, MCPWM0A, M1P);
    mcpwm_gpio_init(MCPWM_UNIT_1, MCPWM0B, M1N);

    printf("Configuring Initial Parameters of mcpwm...\n");
    mcpwm_config_t pwm_config;
    pwm_config.frequency = 4000;    //frequency = 2000Hz,
    pwm_config.cmpr_a = 0;    //duty cycle of PWMxA = 0
    pwm_config.cmpr_b = 0;    //duty cycle of PWMxb = 0
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);    //Configure PWM0A & PWM0B with above settings
    mcpwm_init(MCPWM_UNIT_1, MCPWM_TIMER_1, &pwm_config);
}

static void set_speed(mcpwm_unit_t mcpwm_num, mcpwm_timer_t timer_num, float speed) {  // speed from -1 ~ 1
    if (speed > 0) {
        mcpwm_set_signal_low(mcpwm_num, timer_num, MCPWM_OPR_B);
        mcpwm_set_duty(mcpwm_num, timer_num, MCPWM_OPR_A, speed);
        mcpwm_set_duty_type(mcpwm_num, timer_num, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
    } else if (speed < 0) {
        mcpwm_set_signal_low(mcpwm_num, timer_num, MCPWM_OPR_A);
        mcpwm_set_duty(mcpwm_num, timer_num, MCPWM_OPR_B, speed);
        mcpwm_set_duty_type(mcpwm_num, timer_num, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);
    } else {
        mcpwm_set_signal_low(mcpwm_num, timer_num, MCPWM_OPR_B);
        mcpwm_set_signal_low(mcpwm_num, timer_num, MCPWM_OPR_A);
    }
}

void motor_update_speed(float speed1, float speed2) {
    ESP_LOGI(TAG, "speed set to %f %f", speed1, speed2);
    set_speed(MCPWM_UNIT_0, MCPWM_TIMER_0, speed1);
    set_speed(MCPWM_UNIT_1, MCPWM_TIMER_1, speed2);
}
