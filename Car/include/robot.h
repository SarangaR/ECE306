#ifndef ROBOT_H
#define ROBOT_H

#include "msp430.h"
#include "include/ports.h"
#include "include/macros.h"
#include "include/motors.h"
#include "include/pid.h"

#define COMMAND_TICKS_PER_SECOND (50)
#define COMMAND_MAX_CHILDREN (8)
#define TRACK_WIDTH (120.0f) // mm
#define WHEEL_DIAMETER (40.0f) // mm
#define MAX_RPM (150.0f) // Maximum RPM of the motors
#define V_MAX ((MAX_RPM / 60.0) * PI * WHEEL_DIAMETER)

#define LF_BASE_SPEED (75)
#define LF_GAIN_SCALE (1024)

#define LF_KP (0.6f)
#define LF_KI (0.0f)
#define LF_KD (0.5f)

#define LF_KP_SCALED_L (LF_KP*LF_GAIN_SCALE)
#define LF_KI_SCALED_L (LF_KI*LF_GAIN_SCALE)
#define LF_KD_SCALED_L (LF_KD*LF_GAIN_SCALE)

#define LF_KP_SCALED_R (LF_KP*LF_GAIN_SCALE)
#define LF_KI_SCALED_R (LF_KI*LF_GAIN_SCALE)
#define LF_KD_SCALED_R (0)

#define LF_KP_SCALED_L (LF_KP*LF_GAIN_SCALE)
#define LF_KI_SCALED_L (LF_KI*LF_GAIN_SCALE)
#define LF_KD_SCALED_L (LF_KD*LF_GAIN_SCALE)

#define LF_KP_SCALED (LF_KP*LF_GAIN_SCALE)
#define LF_KI_SCALED (LF_KI*LF_GAIN_SCALE)
#define LF_KD_SCALED (LF_KD*LF_GAIN_SCALE)


#define LF_INTEGRAL_MAX (5000)
#define LF_INTEGRAL_MIN (-5000)
#define LF_CORRECTION_MAX (100)
#define LF_CORRECTION_MIN (-100)
#define LF_GAP_THRESHOLD (20)

static int lf_integral = 0;
static int lf_prev_error = 0;
static int lf_wasInGap = 0;

#define CHAIN_MAX_COMMANDS (8)

#define COMMAND_TICKS_FROM_MS(ms) ((unsigned int)((((unsigned long)(ms) * (unsigned long)COMMAND_TICKS_PER_SECOND) + 999UL) / 1000UL))

extern PIDController leftPIDLF;
extern PIDController rightPIDLF;

typedef enum
{
    NONE = 'N',
    CIRCLE = 'C',
    FIGURE_8 = '8',
    TRIANGLE = 'T'
} Event;

typedef enum
{
    STANDBY = 0,
    RUN = 1
} RobotState;

typedef enum
{
    CMD_NONE = 0,
    CMD_WAIT,
    CMD_FORWARD,
    CMD_REVERSE,
    CMD_SPIN,
    CMD_TURN_TO_ANGLE,
    CMD_DRIVE_STRAIGHT,
    CMD_REVERSE_STRAIGHT,
    CMD_DRIVE_TO_XY,
    CMD_DRIVE_UNTIL,
    CMD_DRIVE_TO_LINE,
    CMD_ALIGN_LEFT_TO_LINE,
    CMD_FOLLOW_LINE,
    CMD_SEQUENCE,
    CMD_PARALLEL
} CommandType;

typedef volatile unsigned int *DriveUntilFlag;

typedef enum
{
    SPIN_CLOCKWISE = 1,
    SPIN_COUNTERCLOCKWISE = 0
} SpinDirection;

typedef struct Command Command;

struct Command
{
    CommandType type;
    unsigned int duration_ticks;
    unsigned int elapsed_ticks;
    unsigned char started;
    unsigned char finished;
    Command *children[COMMAND_MAX_CHILDREN];
    unsigned char child_count;
    unsigned char active_child;
    float target_angle;
    float heading_start;
    float heading_error_prev;
    float heading_error_sum;
    SpinDirection spin_direction;
    float target_x;
    float target_y;
    unsigned char pwm_counter;
    unsigned char left_duty;
    unsigned char right_duty;
    DriveUntilFlag drive_until_flag;
    DriveUntilFlag drive_until_left_flag;
    DriveUntilFlag drive_until_right_flag;
    const char *display_message;
};

typedef struct
{
    RobotState state;
    Event event;
    Command *active_command;
} Robot;

typedef struct RobotCommandChain RobotCommandChain;

struct RobotCommandChain
{
    RobotCommandChain (*andThenForward)(int time_seconds);
    RobotCommandChain (*andThenReverse)(int time_seconds);
    RobotCommandChain (*andThenWait)(int time_seconds);
    RobotCommandChain (*andThenSpinCW)(int time_seconds);
    RobotCommandChain (*andThenSpinCCW)(int time_seconds);
    RobotCommandChain (*andThenTurnToAngle)(float target_angle_degrees);
    RobotCommandChain (*andThenDriveStraight)(int time_seconds);
    RobotCommandChain (*andThenReverseStraight)(int time_seconds);
    RobotCommandChain (*andThenDriveToLine)(void);
    RobotCommandChain (*andThenAlignLeftToLine)(void);
    RobotCommandChain (*andThenFollowLine)(int time_seconds);
    RobotCommandChain (*until)(DriveUntilFlag stop_flag);
    RobotCommandChain (*untilSelective)(DriveUntilFlag left_stop_flag, DriveUntilFlag right_stop_flag);
    RobotCommandChain (*withDisplay)(const char *msg);
    RobotCommandChain (*andThenDriveUntil)(DriveUntilFlag stop_flag);
    RobotCommandChain (*andThenDriveToXY)(float target_x_in, float target_y_in);
    void (*schedule)(void);
};

void initRobot(Robot *robot);
void resetRobot(Robot *robot);
void updateRobot(Robot *robot, int Time_Sequence);
void setRobotShape(Robot *robot, Event shape);

void Command_Forward(Command *command, int time_seconds);
void Command_Reverse(Command *command, int time_seconds);
void Command_Spin(Command *command, int time_seconds, SpinDirection direction);
void Command_Wait(Command *command, int time_seconds);
void Command_TurnToAngle(Command *command, float target_angle_degrees);
void Command_DriveStraight(Command *command, int time_seconds);
void Command_ReverseStraight(Command *command, int time_seconds);
void Command_DriveToXY(Command *command, float target_x_in, float target_y_in);
void Command_DriveUntil(Command *command, DriveUntilFlag stop_flag);
void Command_DriveToLine(Command *command);
void Command_AlignLeftToLine(Command *command);
void Command_FollowLine(Command *command, int time_seconds);
void Command_Sequence(Command *command, Command **children, unsigned char child_count);
void Command_Parallel(Command *command, Command **children, unsigned char child_count);

RobotCommandChain chainForward(int time_seconds);
RobotCommandChain chainReverse(int time_seconds);
RobotCommandChain chainWait(int time_seconds);
RobotCommandChain chainSpinCW(int time_seconds);
RobotCommandChain chainSpinCCW(int time_seconds);
RobotCommandChain chainTurnToAngle(float target_angle_degrees);
RobotCommandChain chainDriveStraight(int time_seconds);
RobotCommandChain chainReverseStraight(int time_seconds);
RobotCommandChain chainDriveToLine(void);
RobotCommandChain chainAlignLeftToLine(void);
RobotCommandChain chainFollowLine(int time_seconds);
RobotCommandChain chainDriveUntil(DriveUntilFlag stop_flag);
RobotCommandChain chainDriveToXY(float target_x_in, float target_y_in);

extern float turn_kp;
extern float turn_ki;
extern float turn_kd;
extern float turn_ff_rate_kp_dps_per_deg;
extern float turn_ff_max_rate_dps;
extern float turn_ff_ks_percent;
extern float turn_ff_kv_percent_per_dps;
extern float turn_max_pwm_percent;
extern float turn_integral_zone_deg;
extern float drive_straight_kp;
extern float drive_straight_kd;
extern float drive_straight_base_percent;
extern float drive_straight_max_correction_percent;
extern float drive_straight_deadband_deg;
extern float drive_straight_correction_sign;
extern float align_line_left_base_percent;
extern float align_line_right_base_percent;
extern float align_line_detector_kp;
extern float align_line_detector_kd;
extern float align_line_detector_ki;
extern float align_line_detector_tolerance_raw;
extern float drive_to_line_max_percent;
extern float drive_to_line_min_percent;
extern float drive_to_line_approach_gain;
extern float line_follow_base_percent;
extern float line_follow_kp;
extern float line_follow_ki;
extern float line_follow_kd;
extern float line_follow_integral_limit;
extern float line_follow_max_correction_percent;
extern float turn_angle_tolerance_deg;
extern float drive_to_xy_speed_percent;
extern float drive_to_xy_tolerance_in;
unsigned char isRobotBusy(void);

extern volatile unsigned int black_line_left;
extern volatile unsigned int black_line_right;
extern volatile unsigned int project7flag;
extern unsigned long project7count;

#endif
