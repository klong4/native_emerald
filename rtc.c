#include "rtc.h"
#include <string.h>
#include <stdio.h>

// GPIO pin mapping for RTC
#define RTC_SCK  0x01  // Serial Clock (bit 0)
#define RTC_SIO  0x02  // Serial I/O (bit 1)
#define RTC_CS   0x04  // Chip Select (bit 2)

// RTC commands
#define RTC_CMD_RESET      0x60
#define RTC_CMD_CONTROL    0x62
#define RTC_CMD_DATETIME   0x64
#define RTC_CMD_TIME       0x66
#define RTC_CMD_STATUS     0x62

void rtc_init(RTCState *rtc) {
    if (!rtc) return;
    
    memset(rtc, 0, sizeof(RTCState));
    
    // Initialize with current system time
    rtc->base_timestamp = time(NULL);
    
    // Set initial time values
    struct tm *current_time = localtime(&rtc->base_timestamp);
    rtc->seconds = current_time->tm_sec;
    rtc->minutes = current_time->tm_min;
    rtc->hours = current_time->tm_hour;
    
    // Calculate days since epoch (simplified)
    rtc->days_low = 0;
    rtc->days_high = 0;
    
    rtc->status = 0;
    rtc->control = 0;
    
    printf("[RTC] Initialized with time: %02d:%02d:%02d\n", 
           rtc->hours, rtc->minutes, rtc->seconds);
}

void rtc_update(RTCState *rtc) {
    if (!rtc) return;
    
    // Calculate elapsed time since base
    time_t now = time(NULL);
    time_t elapsed = now - rtc->base_timestamp;
    
    // Update time values
    rtc->seconds = (u8)(elapsed % 60);
    rtc->minutes = (u8)((elapsed / 60) % 60);
    rtc->hours = (u8)((elapsed / 3600) % 24);
    
    // Update days (simplified - doesn't handle day rollover perfectly)
    u32 days = (u32)(elapsed / 86400);
    rtc->days_low = days & 0xFF;
    rtc->days_high = (days >> 8) & 0xFF;
}

u8 rtc_gpio_read(RTCState *rtc, u16 gpio_data, u16 gpio_direction) {
    if (!rtc) return 0;
    
    // Update RTC time before reading
    rtc_update(rtc);
    
    // If reading data, return the current bit
    if (rtc->reading && rtc->bit_index < 64) {
        u8 byte_index = rtc->bit_index / 8;
        u8 bit_pos = rtc->bit_index % 8;
        
        if (byte_index < 8) {
            u8 bit_value = (rtc->data_buffer[byte_index] >> bit_pos) & 0x01;
            return bit_value ? RTC_SIO : 0;
        }
    }
    
    return 0;
}

void rtc_gpio_write(RTCState *rtc, u16 gpio_data, u16 gpio_direction) {
    if (!rtc) return;
    
    u8 sck = (gpio_data & RTC_SCK) ? 1 : 0;
    u8 sio = (gpio_data & RTC_SIO) ? 1 : 0;
    u8 cs = (gpio_data & RTC_CS) ? 1 : 0;
    
    // Detect CS rising edge (start of communication)
    if (cs && !rtc->last_cs) {
        rtc->bit_index = 0;
        rtc->buffer_pos = 0;
        rtc->reading = false;
        rtc->writing = true;
        memset(rtc->data_buffer, 0, sizeof(rtc->data_buffer));
        
        static int gpio_write_count = 0;
        if (gpio_write_count < 5) {
            printf("[RTC] CS rising edge - start communication\n");
            gpio_write_count++;
        }
    }
    
    // Detect CS falling edge (end of communication)
    if (!cs && rtc->last_cs) {
        rtc->reading = false;
        rtc->writing = false;
        
        static int gpio_end_count = 0;
        if (gpio_end_count < 5) {
            printf("[RTC] CS falling edge - end communication\n");
            gpio_end_count++;
        }
    }
    
    // Detect SCK rising edge (clock in data)
    if (sck && !rtc->last_sck && cs) {
        if (rtc->writing && rtc->bit_index < 64) {
            // Write mode - receive command/data from game
            u8 byte_index = rtc->bit_index / 8;
            u8 bit_pos = rtc->bit_index % 8;
            
            if (byte_index < 8) {
                if (sio) {
                    rtc->data_buffer[byte_index] |= (1 << bit_pos);
                } else {
                    rtc->data_buffer[byte_index] &= ~(1 << bit_pos);
                }
            }
            
            rtc->bit_index++;
            
            // After receiving command byte, process it
            if (rtc->bit_index == 8) {
                rtc->command = rtc->data_buffer[0];
                
                static int cmd_count = 0;
                if (cmd_count < 5) {
                    printf("[RTC] Received command: 0x%02X\n", rtc->command);
                    cmd_count++;
                }
                
                // Prepare response based on command
                if ((rtc->command & 0x0F) == 0x06) {  // Read time
                    rtc->reading = true;
                    rtc->writing = false;
                    
                    // Update time before sending
                    rtc_update(rtc);
                    
                    // Prepare time data buffer
                    rtc->data_buffer[0] = rtc->seconds;
                    rtc->data_buffer[1] = rtc->minutes;
                    rtc->data_buffer[2] = rtc->hours;
                    rtc->data_buffer[3] = rtc->days_low;
                    rtc->data_buffer[4] = rtc->days_high;
                    rtc->data_buffer[5] = 0; // Day of week
                    rtc->data_buffer[6] = rtc->control;
                    rtc->data_buffer[7] = rtc->status;
                    
                    static int time_read_count = 0;
                    if (time_read_count < 3) {
                        printf("[RTC] Sending time: %02d:%02d:%02d\n",
                               rtc->hours, rtc->minutes, rtc->seconds);
                        time_read_count++;
                    }
                }
                else if ((rtc->command & 0x0F) == 0x02) {  // Read status
                    rtc->reading = true;
                    rtc->writing = false;
                    rtc->data_buffer[0] = rtc->status;
                }
                else if ((rtc->command & 0x0F) == 0x00) {  // Reset
                    rtc_init(rtc);
                }
            }
        }
        else if (rtc->reading && rtc->bit_index < 64) {
            // Read mode - game is clocking out data
            rtc->bit_index++;
        }
    }
    
    // Save current state for edge detection
    rtc->last_sck = sck;
    rtc->last_cs = cs;
}
