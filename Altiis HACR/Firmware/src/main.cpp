// main.cpp

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"

#define PWM_PIN 16
#define BUTTON_PIN 15
#define POT_ADC 0   // GPIO26

#define PWM_FREQ 50
#define PWM_MIN_US 1000
#define PWM_MAX_US 2000

#define LONG_PRESS_MS 1000
#define SOFT_START_MS 810
#define ADJUST_RAMP_MS 300
#define ESC_ARM_DELAY_MS 2000

uint pwm_slice;
uint16_t current_pwm = PWM_MIN_US;
bool system_on = false;

// ---------------- PWM ----------------
void set_pwm_us(uint16_t us) {
    uint32_t period_us = 1000000 / PWM_FREQ;
    uint16_t duty = (us * 65535) / period_us;
    pwm_set_gpio_level(PWM_PIN, duty);
}

uint16_t read_pot_us() {
    uint16_t raw = adc_read();
    return PWM_MIN_US + (raw * (PWM_MAX_US - PWM_MIN_US)) / 4095;
}

void ramp_pwm(uint16_t from, uint16_t to, uint32_t time_ms) {
    const int steps = 50;
    int32_t delta = (int32_t)to - from;

    for (int i = 0; i <= steps; i++) {
        set_pwm_us(from + delta * i / steps);
        sleep_ms(time_ms / steps);
    }
}

// ---------------- MAIN ----------------
int main() {
    stdio_init_all();

    // PWM
    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
    pwm_slice = pwm_gpio_to_slice_num(PWM_PIN);
    pwm_set_clkdiv(pwm_slice, 125.0f);
    pwm_set_wrap(pwm_slice, 65535);
    pwm_set_enabled(pwm_slice, true);

    // Button
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_down(BUTTON_PIN);

    // ADC
    adc_init();
    adc_gpio_init(26);
    adc_select_input(POT_ADC);

    // ESC safety arm
    set_pwm_us(PWM_MIN_US);
    sleep_ms(ESC_ARM_DELAY_MS);

    absolute_time_t press_start;
    bool pressed = false;

    while (true) {
        bool btn = gpio_get(BUTTON_PIN);

        if (btn && !pressed) {
            pressed = true;
            press_start = get_absolute_time();
        }

        if (!btn && pressed) {
            pressed = false;
            int held = absolute_time_diff_us(press_start, get_absolute_time()) / 1000;

            if (held >= LONG_PRESS_MS) {
                system_on = !system_on;

                if (system_on) {
                    uint16_t target = read_pot_us();
                    ramp_pwm(PWM_MIN_US, target, SOFT_START_MS);
                    current_pwm = target;
                } else {
                    set_pwm_us(PWM_MIN_US);
                    current_pwm = PWM_MIN_US;
                }
            }
        }

        if (system_on) {
            uint16_t target = read_pot_us();
            if (target != current_pwm) {
                ramp_pwm(current_pwm, target, ADJUST_RAMP_MS);
                current_pwm = target;
            }
        }

        sleep_ms(10);
    }
}
