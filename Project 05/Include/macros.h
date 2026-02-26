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
volatile float one_second_timer;
volatile int movement_started;
volatile int reset_movement;
unsigned int test_value;
char chosen_direction;
char change;

unsigned int wheel_move;
char forward;
