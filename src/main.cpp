#include "assert.h"
#include "avr/interrupt.h"
#include "avr/io.h"
#include "avr/sleep.h"
#include "util/delay.h"

#include "bme280_client.h"
#include "recorder.h"
#include "usart_debug.h"

// This is not really linear but should be close enough to only introduce
// a few percent error assuming we're launching from near sea level, only
// going ~1000ft and operating around ambient temperature

#define MODE_ROCKET 0
#define MODE_THROW 1
#define MODE_ELECTRIC 2
#define MODE_KITE 3

#define CURRENT_MODE MODE_ROCKET

#if CURRENT_MODE == MODE_ROCKET
#define PA_INTERVAL 18 // 5 feet interval
#define FAST_INTERVAL_INVERSE_SECS 8
#define SLOW_INTERVAL_SECS 1
#define FAST_INTERVAL_RECORDS 80
#define START_DELTA_THRESHOLD_INTERVALS 3 // Start launch tracking after this size delta
#elif CURRENT_MODE == MODE_THROW
#define PA_INTERVAL 5 // ~1.5'
#define FAST_INTERVAL_INVERSE_SECS 10
#define SLOW_INTERVAL_SECS 1
#define FAST_INTERVAL_RECORDS 200
#define START_DELTA_THRESHOLD_INTERVALS 1 // Start launch tracking after this size delta
#elif CURRENT_MODE == MODE_ELECTRIC
#define PA_INTERVAL 17 // ~5'
#define FAST_INTERVAL_INVERSE_SECS 4
#define SLOW_INTERVAL_SECS 1
#define FAST_INTERVAL_RECORDS 40
#define START_DELTA_THRESHOLD_INTERVALS 1 // Start launch tracking after this size delta
#elif CURRENT_MODE == MODE_KITE
#define PA_INTERVAL 11 // ~3'
#define FAST_INTERVAL_INVERSE_SECS 2
#define SLOW_INTERVAL_SECS 1
#define FAST_INTERVAL_RECORDS 256
#define START_DELTA_THRESHOLD_INTERVALS 1 // Start launch tracking after this size delta
#endif

int32_t last_pressure_pa_;
bool running_;

int32_t start_pressure_pa_;
int16_t last_altitude_intervals_;
uint8_t n_records_ = 0;

// Returns whether there is more room to keep recording
bool record_delta(int8_t delta_intervals_from_last) {
    usart_debug_send(delta_intervals_from_last);
    bool res = recorder_record(delta_intervals_from_last);
    last_altitude_intervals_ += delta_intervals_from_last;
    n_records_++;
    return res;
}

int8_t get_record_delta(int32_t pressure_pa) {
    // TODO: check this math or, better, write tests.
    // Calculate all deltas relative to launch pressure so that we don't drift because
    // of repeated rounding to intervals
    int16_t delta_intervals_from_launch = (start_pressure_pa_ - pressure_pa) / PA_INTERVAL;
    return delta_intervals_from_launch - last_altitude_intervals_;
}

void led_on() { VPORTB.OUT |= PIN2_bm; }

void led_off() { VPORTB.OUT &= ~PIN2_bm; }

void wait_for_button() {
    // Button is on PA4
    // Enable pull-up
    PORTA.PIN4CTRL |= PORT_PULLUPEN_bm;
    // Interrupt on low-level (needs to occur after pull-up)
    PORTA.PIN4CTRL |= PORT_ISC_LEVEL_gc;

    sei();
    // Power down until button press wakes us up, then disable pull-up and interrupt
    // This avoids accidentally overwriting data if the device restarts
    sleep_mode();

    cli();
    PORTA.PIN4CTRL = 0;
}

void error() {
    set_sleep_mode(SLEEP_MODE_STANDBY);
    TCA0.SINGLE.PER = F_CLK_PER / 20 / 16; // 20hz
    while (1) {
        led_on();
        sleep_mode();
        led_off();
        sleep_mode();
    }
}

void test_measuring_and_recording() {
    if (bme280_measure(&last_pressure_pa_) != BME280_OK) {
        error();
    };
    for (int i = 0; i < 30; i++) {
        sleep_mode();
        led_on();

        int32_t pressure_pa;
        if (bme280_measure(&pressure_pa) != BME280_OK) {
            error();
        };
        int32_t delta_pa = last_pressure_pa_ - pressure_pa;
        if (delta_pa > INT8_MAX) {
            delta_pa = INT8_MAX;
        } else if (delta_pa < INT8_MIN) {
            delta_pa = INT8_MIN;
        }

        usart_debug_send(delta_pa);
        if (!recorder_record_test_byte(delta_pa)) {
            break;
        }
        last_pressure_pa_ = pressure_pa;
        led_off();
    }

    led_on();
    while (1)
        ;
}

void test_measuring_and_printing() {
    while (1) {
        sleep_mode();
        led_on();

        int32_t pressure_pa;
        if (bme280_measure(&pressure_pa) != BME280_OK) {
            error();
        };

        usart_debug_send('+');
        usart_debug_send(pressure_pa & 0xff);
        usart_debug_send((pressure_pa >> 8) & 0xff);
        usart_debug_send((pressure_pa >> 16) & 0xff);

        led_off();
    }
}

int main(void) {
    CPU_CCP = CCP_IOREG_gc; /* Enable writing to protected register MCLKCTRLB */
    CLKCTRL.MCLKCTRLB = CLKCTRL_PEN_bm | CLKCTRL_PDIV_64X_gc; // Divide main clock by 64 = 312500hz

    VPORTB.DIR |= PIN2_bm; // Configure LED for output
    usart_debug_init();

    set_sleep_mode(SLEEP_MODE_STANDBY);
    wait_for_button(); // Must configure sleep first
    // Standby seems to screw things some things up and we won't be in this mode
    // for more than a few minutes
    set_sleep_mode(SLEEP_MODE_IDLE);

    // Get an initial pressure value
    if (bme280_init() != BME280_OK) {
        error();
    }

    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
    // NB: This needs to match the divider set in CTRLA and needs to support 3s intervals
    TCA0.SINGLE.PER = F_CLK_PER / FAST_INTERVAL_INVERSE_SECS / 16;
    TCA0.SINGLE.CTRLA = TCA_SINGLE_RUNSTDBY_bm | TCA_SINGLE_ENABLE_bm | TCA_SINGLE_CLKSEL_DIV16_gc;

    sei();

    //test_measuring_and_printing();
    //test_measuring_and_recording();

    // Get an initial reading
    if (bme280_measure(&last_pressure_pa_) != BME280_OK) {
        error();
    };

    while (1) {
        sleep_mode(); // Enter standby until the timer wakes us

        led_on();

        int32_t pressure_pa;
        if (bme280_measure(&pressure_pa) != BME280_OK) {
            error();
        };

        if (running_) {
            if (!record_delta(get_record_delta(pressure_pa))) {
                led_off();
                // Stop recording until power cycles once we fill EEPROM
                TCA0.SINGLE.CTRLA = 0;
                set_sleep_mode(SLEEP_MODE_STANDBY);
                sleep_mode();
            }
            // Switch to 3s increments after 10s
            if (n_records_ == FAST_INTERVAL_RECORDS) {
                TCA0.SINGLE.PER = F_CLK_PER * SLOW_INTERVAL_SECS / 16;
            }
        } else {
            int16_t delta_intervals = (last_pressure_pa_ - pressure_pa) / PA_INTERVAL;
            if (delta_intervals >= START_DELTA_THRESHOLD_INTERVALS) {
                record_delta(get_record_delta(last_pressure_pa_));
                record_delta(get_record_delta(pressure_pa));
                running_ = true;
            } else {
                start_pressure_pa_ = last_pressure_pa_;
                last_pressure_pa_ = pressure_pa;
            }

            led_off();
        }
    }
}

ISR(TCA0_OVF_vect) { TCA0.SINGLE.INTFLAGS |= TCA_SINGLE_OVF_bm; }

ISR(PORTA_PORT_vect) {}
