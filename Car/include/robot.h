#ifndef ROBOT_H
#define ROBOT_H

#include "msp430.h"
#include "include/ports.h"
#include "include/macros.h"
#include "include/motors.h"
#include "include/pid.h"

#define COMMAND_TICKS_PER_SECOND (50)
#define COMMAND_MAX_CHILDREN (12)
#define TRACK_WIDTH (120.0f) // mm
#define WHEEL_DIAMETER (40.0f) // mm
#define MAX_RPM (150.0f) // Maximum RPM of the motors
#define V_MAX ((MAX_RPM / 60.0) * PI * WHEEL_DIAMETER)
#define MAX_ANGULAR_VELOCITY_DEG_S (150.0f) // max angular velocity in degrees per second

#define LF_BASE_SPEED       (75)
#define LF_GAIN_SCALE       (1024)

#define LF_KP               (0.7f)
#define LF_KI               (0.0f)
#define LF_KD               (0.5f)

#define LF_KP_SCALED        (LF_KP * LF_GAIN_SCALE)
#define LF_KI_SCALED        (LF_KI * LF_GAIN_SCALE)
#define LF_KD_SCALED        (LF_KD * LF_GAIN_SCALE)

#define LF_TARGET_SUM       (138)
#define LF_KP_SUM_SCALED    (512)     // ~0.3 * 1024 — tune this

#define LF_INTEGRAL_MAX     (5000)
#define LF_INTEGRAL_MIN     (-5000)
#define LF_CORRECTION_MAX   (100)
#define LF_CORRECTION_MIN   (-100)
#define LF_LOSS_THRESHOLD   (30)
#define LF_FRICTION_FLOOR (45)

#define TURN_KP_UNSCALED (5.0f)
#define TURN_KI_UNSCALED (0.025f)
#define TURN_KD_UNSCALED (12.0f)

#define TURN_KP (TURN_KP_UNSCALED * LF_GAIN_SCALE)
#define TURN_KI (TURN_KI_UNSCALED * LF_GAIN_SCALE)
#define TURN_KD (TURN_KD_UNSCALED * LF_GAIN_SCALE)

static int lf_integral = 0;
static int lf_prev_error = 0;
static int lf_wasInGap = 0;
static float lf_prev_heading = 0;
static float lf_heading_acc = 0;
static float lf_last_lap_tick = 0;
static int lf_coastFrames = 0;
static int lf_coastMv     = 0;
static int lf_junction_frames = 0;  /* consecutive frames where both sensors see black */

/* ── Follow-line failsafe thresholds ─────────────────────────────────────── */
/* Detector value above which a sensor is considered "on black" */
#define LF_BOTH_BLACK_THRESHOLD   (70)
/* Detector value below which a sensor is considered "off the line" (white) */
#define LF_BOTH_WHITE_THRESHOLD   (LF_LOSS_THRESHOLD)
/* Sustained both-black frames required to declare a junction (~300 ms @ 50 Hz) */
#define LF_JUNCTION_CONFIRM_FRAMES (15)
/* Max frames to coast on last-known mv before amplifying the recovery turn */
#define LF_COAST_MAX_FRAMES       (25)
/*
 * Steering bias applied while confirmed at a junction.
 * Negative  → left turn  (counterclockwise around the circle).
 * Tune the magnitude if the robot overshoots / undershoots the circle entry.
 */
#define LF_CIRCLE_BIAS            (-45)

#define CHAIN_MAX_COMMANDS (COMMAND_MAX_CHILDREN)

#define COMMAND_TICKS_FROM_MS(ms) ((unsigned int)((((unsigned long)(ms) * (unsigned long)COMMAND_TICKS_PER_SECOND) + 999UL) / 1000UL))

extern PIDController leftPIDLF;
extern PIDController rightPIDLF;
extern PIDController turnPID;

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
    CMD_DRIVE_DISTANCE,
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
    int lf_lap_count;
    unsigned char pwm_counter;
    unsigned char left_duty;
    unsigned char right_duty;
    DriveUntilFlag drive_until_flag;
    DriveUntilFlag drive_until_left_flag;
    DriveUntilFlag drive_until_right_flag;
    const char *display_message;
    unsigned int timeout_ticks;  /* 0 = no timeout; DriveDistance uses this as a hard deadline */
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
    RobotCommandChain (*andThenDriveUntil)(DriveUntilFlag left_flag, DriveUntilFlag right_flag);
    RobotCommandChain (*andThenDriveToXY)(float target_x_in, float target_y_in);
    RobotCommandChain (*andThenDriveDistance)(float inches);
    RobotCommandChain (*andThenTurnToAbsoluteAngle)(float degrees);
    RobotCommandChain (*withTimeout)(unsigned int timeout_ms);
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
void Command_DriveDistance(Command *command, float inches);
void Command_TurnToAbsoluteAngle(Command *command, float degrees);
void Command_DriveUntil(Command *command, DriveUntilFlag stop_left_flag, DriveUntilFlag stop_right_flag);
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
RobotCommandChain chainAndThenDriveUntil(DriveUntilFlag left_flag, DriveUntilFlag right_flag);
RobotCommandChain chainDriveToXY(float target_x_in, float target_y_in);
RobotCommandChain chainDriveDistance(float inches);
RobotCommandChain chainTurnToAbsoluteAngle(float degrees);
RobotCommandChain chainDriveStraightMs(unsigned int ms);
RobotCommandChain chainReverseMs(unsigned int ms);
RobotCommandChain chainSpinCWMs(unsigned int ms, unsigned char duty_percent);
RobotCommandChain chainSpinCCWMs(unsigned int ms, unsigned char duty_percent);

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
extern float drive_to_xy_kv;
extern float drive_to_xy_komega;
unsigned char isRobotBusy(void);

/* Streaming curvature drive — preempts any active timed command and applies
   motor speeds immediately.  fwd_pct and turn_pct are in the range -100..100.
   Calling with (0, 0) stops the motors and disarms the watchdog. */
void applySpeedSet(float fwd_pct, float turn_pct);

/* Call once per 50 Hz tick (pass one_second_timer).  Stops motors if no
   applySpeedSet call has arrived within ~300 ms. */
void robotCurvatureWatchdog(unsigned long tick);

extern volatile unsigned int black_line_left;
extern volatile unsigned int black_line_right;
extern volatile unsigned int black_all;
extern volatile unsigned int project7flag;
extern unsigned long project7count;

#endif
