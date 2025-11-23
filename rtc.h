#ifndef RTC_H
#define RTC_H

#include "types.h"
#include <time.h>

// RTC (Real-Time Clock) state for Pokemon games
// The RTC is accessed through GPIO pins on the cartridge

typedef struct RTCState {
    // Current time
    u8 seconds;
    u8 minutes;
    u8 hours;
    u8 days_low;
    u8 days_high;
    
    // Control/status
    u8 control;
    u8 status;
    
    // Serial communication state
    u8 command;
    u8 bit_index;
    u8 data_buffer[8];
    u8 buffer_pos;
    bool reading;
    bool writing;
    
    // GPIO state tracking
    u8 last_sck;  // Serial clock
    u8 last_cs;   // Chip select
    
    // Base time for calculating elapsed time
    time_t base_timestamp;
} RTCState;

void rtc_init(RTCState *rtc);
void rtc_update(RTCState *rtc);
u8 rtc_gpio_read(RTCState *rtc, u16 gpio_data, u16 gpio_direction);
void rtc_gpio_write(RTCState *rtc, u16 gpio_data, u16 gpio_direction);

#endif // RTC_H
