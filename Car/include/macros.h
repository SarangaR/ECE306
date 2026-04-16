#ifndef MACROS_H
#define MACROS_H

#include "display.h"
#include <stdint.h>

#define ALWAYS (1)
#define RESET_STATE (0)
#define RED_LED (0x01)    // RED LED 0
#define GRN_LED (0x40)    // GREEN LED 1
#define TEST_PROBE (0x01) // 0 TEST PROBE
#define TRUE (0x01)       //

#define MCLK_FREQ_MHZ (8) // MCLK = 8MHz
#define CLEAR_REGISTER (0X0000)
#define P4PUD (P4OUT)
#define P2PUD (P2OUT)

#define PI (3.14159265f)

// Thumbwheel position defines
#define THUMBWHEEL_CALIBRATE (0)
#define THUMBWHEEL_HW06_MODE (10)

// Timer counter maximums
#define TIME_SEQUENCE_MAX    (251U)
#define DISPLAY_COUNT_MAX    (251U)

// Function Prototypes
void main(void);
void Init_Conditions(void);
void Display_Process(void);
void Init_LEDs(void);
void Carlson_StateMachine(void);

// Global Variables
volatile char slow_input_down;
extern char display_line[4][11];
extern char *display[4];
unsigned char display_mode;
extern volatile unsigned char display_changed;
extern volatile unsigned char update_display;
extern volatile unsigned int update_display_count;
volatile unsigned int Time_Sequence;
volatile unsigned int Last_Time_Sequence;
volatile unsigned int cycle_time;
volatile char time_change;
volatile char one_time;
volatile unsigned long one_second_timer;
volatile int movement_started;
volatile int reset_movement;
unsigned int test_value;
char chosen_direction;
char change;

unsigned int wheel_move;

// HW06 Debounce and Backlight globals
volatile unsigned int sw1_debounce_count;
volatile unsigned int sw2_debounce_count;
volatile unsigned char sw1_debounce_active;
volatile unsigned char sw2_debounce_active;
volatile unsigned char backlight_blink_enable;

extern volatile uint8_t baud_hi;

#endif //MACROS_H
