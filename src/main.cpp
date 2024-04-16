#include "assert.h"
#include "avr/interrupt.h"
#include "avr/io.h"
#include "avr/sleep.h"
#include "util/delay.h"

#include "bme280_client.h"
#include "recorder.h"

// This is not really linear but should be close enough to only introduce
// a few percent error assuming we're launching from near sea level, only
// going ~1000ft and operating around ambient temperature
#define PA_PER_10FT 35
#define START_DELTA_THRESHOLD_10FT 1 // Start launch tracking after this size delta
#define FAST_INTERVAL_INVERSE_SECS 2
#define SLOW_INTERVAL_SECS 3

uint32_t last_pressure_pa_;
int8_t last_delta_10ft_;
bool running_;

uint32_t start_pressure_pa_;
int16_t last_altitude_10ft_;
uint8_t n_records_ = 0;

// Returns whether there is more room to keep recording
bool record_delta(int8_t delta_10ft_from_last) {
    bool res = recorder_record(delta_10ft_from_last);
    last_altitude_10ft_ += delta_10ft_from_last;
    n_records_++;
    return res;
}

int8_t get_record_delta(uint32_t pressure_pa) {
    // TODO: check this math or, better, write tests.
    // Calculate all deltas relative to launch pressure so that we don't drift because
    // of repeated rounding to 10ft
    int16_t delta_10ft_from_launch = ((int32_t)start_pressure_pa_ - pressure_pa) / PA_PER_10FT;
    return delta_10ft_from_launch - last_altitude_10ft_;
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
    PORTA.INTFLAGS |= PIN4_bm;
}

int main(void) {
    CPU_CCP = CCP_IOREG_gc; /* Enable writing to protected register MCLKCTRLB */
    CLKCTRL.MCLKCTRLB = CLKCTRL_PEN_bm | CLKCTRL_PDIV_64X_gc; // Divide main clock by 64 = 312500hz

    VPORTB.DIR |= PIN2_bm; // Configure LED for output

    set_sleep_mode(SLEEP_MODE_STANDBY);

    wait_for_button(); // Must configure sleep first

    // Get an initial pressure value
    bme280_init();
    assert(bme280_measure(&last_pressure_pa_) == BME280_OK);

    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
    // NB: This needs to match the divider set in CTRLA and needs to support 3s intervals
    TCA0.SINGLE.PER = F_CLK_PER / FAST_INTERVAL_INVERSE_SECS / 16;
    TCA0.SINGLE.CTRLA = TCA_SINGLE_RUNSTDBY_bm | TCA_SINGLE_ENABLE_bm | TCA_SINGLE_CLKSEL_DIV16_gc;

    sei();

    while (1) {
        // Enter standby until the timer wakes us
        sleep_mode();

        //if (!running_) {
        led_on();
        _delay_ms(10);
        led_off();
        _delay_ms(100);
        //}

        // uint32_t pressure_pa = last_pressure_pa_; // Ignore errors and use last value
        // bme280_measure(&pressure_pa);

        // if (running_) {
        //     if (!record_delta(get_record_delta(pressure_pa))) {
        //         led_off();
        //         // Stop recording until power cycles once we fill EEPROM
        //         sleep_mode();
        //     }
        //     // Switch to 3s increments after 10s
        //     if (n_records_ == 20) {
        //         TCA0.SINGLE.PER = F_CLK_PER * SLOW_INTERVAL_SECS / 16;
        //     }
        //     led_off();
        // } else {
        //     int16_t delta_10ft = ((int32_t)last_pressure_pa_ - pressure_pa) / PA_PER_10FT;
        //     if (delta_10ft >= START_DELTA_THRESHOLD_10FT) {
        //         record_delta(get_record_delta(last_pressure_pa_));
        //         record_delta(get_record_delta(pressure_pa));
        //         running_ = true;
        //     } else {
        //         start_pressure_pa_ = last_pressure_pa_;
        //         last_pressure_pa_ = pressure_pa;
        //     }

        //     led_off();
        // }
    }
}

ISR(TCA0_OVF_vect) { TCA0.SINGLE.INTFLAGS |= TCA_SINGLE_OVF_bm; }
