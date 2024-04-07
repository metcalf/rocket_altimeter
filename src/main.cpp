#include "assert.h"
#include "avr/interrupt.h"
#include "avr/io.h"

#include "bme280_client.h"
#include "recorder.h"

// TODO: Figure this out and hope it's ~linear near sea level
#define PA_PER_10FT (123 * 10)
#define START_DELTA_THRESHOLD_10FT 1 // Start launch tracking after this size delta

uint32_t last_pressure_pa_;
int8_t last_delta_10ft_;
bool running_;

uint32_t start_pressure_pa_;
int16_t last_altitude_10ft_;

// Returns whether there is more room to keep recording
bool record_delta(int8_t delta_10ft_from_last) {
    bool res = recorder_record(delta_10ft_from_last);
    last_altitude_10ft_ += delta_10ft_from_last;
    return res;
}

int8_t get_record_delta(uint32_t pressure_pa) {
    // TODO: check this math or, better, write tests.
    // Calculate all deltas relative to launch pressure so that we don't drift because
    // of repeated rounding to 10ft
    int16_t delta_10ft_from_launch = (start_pressure_pa_ - pressure_pa) / PA_PER_10FT;
    return delta_10ft_from_launch - last_altitude_10ft_;
}

int main(void) {
    CPU_CCP = CCP_IOREG_gc; /* Enable writing to protected register MCLKCTRLB */
    CLKCTRL.MCLKCTRLB = CLKCTRL_PEN_bm | CLKCTRL_PDIV_64X_gc; // Divide main clock by 64 = 312500hz

    // TODO: Implement some kind of "start button"
    // This code doesn't start until pressing it (to avoid overwriting data on power cycle)
    // Pressing it from any other state should cause software reset (maybe only read it during the measurement poll to avoid vibration triggering, require 2 consecutive loops to reset)

    // Get an initial pressure value
    bme280_init();
    assert(bme280_measure(&last_pressure_pa_) == BME280_OK);

    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
    TCA0.SINGLE.PER = F_CLK_PER / 8 / 2; // NB: This needs to match the divider set in CTRLA
    TCA0.SINGLE.CTRLA = TCA_SINGLE_RUNSTDBY_bm | TCA_SINGLE_ENABLE_bm | TCA_SINGLE_CLKSEL_DIV8_gc;

    sei();

    while (1) {
        // Enter standby until the timer wakes us
        SLPCTRL.CTRLA |= SLPCTRL_SMODE_STDBY_gc;

        if (!running_) {
            // TODO: LED on
        }

        uint32_t pressure_pa = last_pressure_pa_; // Ignore errors and use last value
        bme280_measure(&pressure_pa);

        if (running_) {
            if (!record_delta(get_record_delta(pressure_pa))) {
                // Stop recording until we get a reset
            };
        } else {
            int16_t delta_10ft = (last_pressure_pa_ - pressure_pa) / PA_PER_10FT;
            if (delta_10ft >= START_DELTA_THRESHOLD_10FT) {
                record_delta(get_record_delta(last_pressure_pa_));
                record_delta(get_record_delta(pressure_pa));
                running_ = true;
            } else {
                start_pressure_pa_ = last_pressure_pa_;
                last_pressure_pa_ = pressure_pa;
            }

            // TODO: LED off
        }
    }
}

ISR(TCA0_OVF_vect) { TCA0.SINGLE.INTFLAGS |= TCA_SINGLE_OVF_bm; }
