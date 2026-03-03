#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "pico/time.h"

#define ADC_GPIO   26
#define ADC_CHAN    0
#define PWM_GPIO   16
#define ADC_MAX_COUNT 4095U
#define DESIRED_LEN 2000
#define CAPTURE_LEN 200

static const uint16_t PWM_WRAP = 6249;

volatile unsigned int desired_voltage[DESIRED_LEN];
volatile unsigned int captured_counts[CAPTURE_LEN];
volatile unsigned int desired_decimated[CAPTURE_LEN];
volatile bool save_data_flag = false;
volatile uint16_t capture_index = 0;
volatile uint8_t decimation_count = 0;
volatile uint16_t desired_idx = 0;
volatile float eint = 0.0f;
volatile float kp = 0.0f;
volatile float ki = 0.0f;

static uint pwm_slice;
static uint pwm_chan;

static void pwm_init_20khz(uint gpio) {
    gpio_set_function(gpio, GPIO_FUNC_PWM);

    pwm_slice = pwm_gpio_to_slice_num(gpio);
    pwm_chan  = pwm_gpio_to_channel(gpio);

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg, 1.0f);
    pwm_config_set_wrap(&cfg, PWM_WRAP);

    pwm_init(pwm_slice, &cfg, true);
}

static void desired_voltage_init(void) {
    const unsigned int level25 = (ADC_MAX_COUNT * 25U) / 100U;
    const unsigned int level75 = (ADC_MAX_COUNT * 75U) / 100U;

    for (int i = 0; i < 500; i++) desired_voltage[i] = level25;
    for (int i = 500; i < 1000; i++) desired_voltage[i] = level75;
    for (int i = 1000; i < 1500; i++) desired_voltage[i] = level25;
    for (int i = 1500; i < DESIRED_LEN; i++) desired_voltage[i] = level75;
}

static bool timer_1khz_callback(struct repeating_timer *t) {
    uint16_t raw = adc_read();
    unsigned int desired_now = desired_voltage[desired_idx];
    const float dt_s = 0.001f;

    float error = (float)desired_now - (float)raw;
    float eint_candidate = eint + (error * dt_s);
    float u_candidate = 50.0f + (kp * error) + (ki * eint_candidate);

    if (!((u_candidate > 100.0f && error > 0.0f) || (u_candidate < 0.0f && error < 0.0f))) {
        eint = eint_candidate;
    }

    float u = 50.0f + (kp * error) + (ki * eint);
    if (u < 0.0f) u = 0.0f;
    if (u > 100.0f) u = 100.0f;

    uint16_t level = (uint16_t)((u * (float)(PWM_WRAP + 1)) / 100.0f);
    if (level > PWM_WRAP) level = PWM_WRAP;
    pwm_set_chan_level(pwm_slice, pwm_chan, level);

    if (save_data_flag) {
        decimation_count++;
        if (decimation_count >= 10) {
            decimation_count = 0;
            captured_counts[capture_index] = raw;
            desired_decimated[capture_index] = desired_now;
            capture_index++;

            if (capture_index >= CAPTURE_LEN) {
                save_data_flag = false;
            }
        }
    }

    desired_idx++;
    if (desired_idx >= DESIRED_LEN) desired_idx = 0;

    return true; 
}

int main() {
    stdio_init_all();
    while (!stdio_usb_connected()) {
        sleep_ms(10);
    }
    setvbuf(stdout, NULL, _IONBF, 0);
    sleep_ms(200);

    adc_init();
    adc_gpio_init(ADC_GPIO);
    adc_select_input(ADC_CHAN);

    pwm_init_20khz(PWM_GPIO);
    desired_voltage_init();

    pwm_set_chan_level(pwm_slice, pwm_chan, (PWM_WRAP + 1) / 2);

    struct repeating_timer timer;
    add_repeating_timer_ms(-1, timer_1khz_callback, NULL, &timer);

    printf("Enter kp ki to capture 200 ADC counts.\n");

    while (true) {
        float in_kp, in_ki;
        if (scanf("%f %f", &in_kp, &in_ki) == 2) {
            kp = in_kp;
            ki = in_ki;

            capture_index = 0;
            decimation_count = 0;
            desired_idx = 0;
            eint = 0.0f;
            save_data_flag = true;

            while (save_data_flag) {
                sleep_ms(1);
            }

            for (int i = CAPTURE_LEN - 1; i >= 0; i--) {
                printf("%d %u %u\n", i, captured_counts[i], desired_decimated[i]);
            }
        }
    }
}
