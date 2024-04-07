#include "recorder.h"

#include "avr/io.h"

#define EEPROM_PAGES EEPROM_SIZE / EEPROM_SIZE

static uint8_t curr_page_ = 0, curr_byte_ = 0;
static bool partial_byte_ = false;
static char page_val_[EEPROM_PAGE_SIZE];

bool flush_page() {
    // Wait for EEPROM to be ready
    while (NVMCTRL.STATUS & NVMCTRL_EEBUSY_bm)
        ;

    // TODO: confirm this seems right
    for (int i = 0; i < EEPROM_PAGE_SIZE; i++) {
        *(((char *)EEPROM_START) + EEPROM_PAGE_SIZE * curr_page_ + i) = page_val_[i];
    }
    CPU_CCP = CCP_SPM_gc;
    NVMCTRL.CTRLA |= NVMCTRL_CMD_PAGEWRITE_gc;

    curr_page_++;
    curr_byte_ = 0;
    return curr_page_ == EEPROM_PAGES;
}

bool record_one(int8_t val) {
    char byte = (val + 8) & 0x0f;

    // Store the low part
    if (!partial_byte_) {
        page_val_[curr_byte_] = byte;
        partial_byte_ = true;
        return true;
    } else {
        page_val_[curr_byte_] |= (byte << 4);
        partial_byte_ = false;
        curr_byte_++;
        if (curr_byte_ == EEPROM_PAGE_SIZE) {
            return flush_page();
        } else {
            return true;
        }
    }
}

bool recorder_record(int8_t val) {
    if (val > 0) {
        while (val >= 8) {
            if (!record_one(8)) {
                return false;
            };
            val -= 8;
        }
    } else {
        while (val <= -7) {
            if (!record_one(-7)) {
                return false;
            };
            val += 7;
        }
    }
    return record_one(val);
}
