#include "recorder.h"

#include "avr/eeprom.h"
#include "avr/io.h"

// We expect to go up faster than down so allocate more bits to
// the positive side of the range
#define MAX_POSITIVE_VALUE 10
#define MIN_NEGATIVE_VALUE (MAX_POSITIVE_VALUE - 15)

static uint8_t *curr_addr_ = 0, curr_val_ = 0;
static bool partial_byte_ = false;

bool record_one(int8_t val) {
    // Shift the range
    char byte = (val - MIN_NEGATIVE_VALUE) & 0x0f;

    if (!partial_byte_) {
        // Store the low bits in memory
        curr_val_ = byte;
        partial_byte_ = true;
        return true;
    } else {
        // Flush the full value to EEPROM
        curr_val_ |= (byte << 4);
        partial_byte_ = false;
        eeprom_write_byte(curr_addr_, curr_val_);
        curr_addr_++;
        return (uint8_t)curr_addr_ < EEPROM_SIZE;
    }
}

bool recorder_record(int8_t val) {
    if (val > 0) {
        while (val >= MAX_POSITIVE_VALUE) {
            if (!record_one(MAX_POSITIVE_VALUE)) {
                return false;
            };
            val -= MAX_POSITIVE_VALUE;
        }
    } else {
        while (val <= MIN_NEGATIVE_VALUE) {
            if (!record_one(MIN_NEGATIVE_VALUE)) {
                return false;
            };
            val -= MIN_NEGATIVE_VALUE;
        }
    }
    return record_one(val);
}
