#include "include/robot.h"
#include "include/functions.h"
#include <string.h>
#include <stdint.h>
#include "include/otos.h"
#include "include/detector.h"
#include "include/pid.h"
#include "include/fast_trig.h"

#pragma diag_suppress 1530
#pragma diag_suppress 1531
#pragma diag_suppress 1534
#pragma diag_suppress 1535
#pragma diag_suppress 1544
#pragma diag_suppress 1546

#pragma CODE_SECTION(wrapAngle, ".TI.ramfunc")
#pragma CODE_SECTION(absoluteFloat, ".TI.ramfunc")
#pragma CODE_SECTION(pwmCountsFromPercentFloat, ".TI.ramfunc")

static Robot *active_robot = 0;

PIDController leftPIDLF;
PIDController rightPIDLF;
PIDController turnPID;

float turn_kp = 0.87f;
float turn_ki = 0.03f;
float turn_kd = 0.41f;

float turn_ff_rate_kp_dps_per_deg = 5.0f;
float turn_ff_max_rate_dps = 180.0f;
float turn_ff_ks_percent = 0.0f;
float turn_ff_kv_percent_per_dps = 0.0f;
float turn_max_pwm_percent = 92.0f;
float turn_integral_zone_deg = 12.0f;
float turn_sysid_step_pwm_percent = 65.0f;
float turn_sysid_ramp_start_pwm_percent = 16.0f;
float turn_sysid_ramp_step_pwm_percent = 3.0f;
float turn_sysid_ramp_max_pwm_percent = 72.0f;
float turn_sysid_motion_delta_deg = 0.30f;
float drive_straight_kp = 1.8f;
float drive_straight_kd = 0.0f;
float drive_straight_base_percent = 90.0f;
float drive_straight_max_correction_percent = 60.0f;
float drive_straight_deadband_deg = 0.5f;
float drive_straight_correction_sign = -1.0f;
float turn_angle_tolerance_deg = 8.0f;
float drive_to_xy_speed_percent = 75.0f;
float drive_to_xy_tolerance_in = 1.5f;
float drive_to_xy_kv           = 10.0f;  /* % per inch   — scales forward speed with distance */
float drive_to_xy_komega       = 0.7f;   /* % per degree — matches LF_KP                      */
float align_line_left_base_percent = 30.0f;
float align_line_right_base_percent = 72.0f;
float align_line_detector_kp = 0.10f;
float align_line_detector_kd = 0.05f;
float align_line_detector_ki = 0.00f;
float align_line_heading_kp = 0.65f;
float align_line_target_delta_deg = 88.0f;
float align_line_heading_tolerance_deg = 10.0f;
float align_line_detector_tolerance_raw = 70.0f;
float drive_to_line_max_percent = 85.0f;
float drive_to_line_min_percent = 12.0f;
float drive_to_line_approach_gain = 1.5f;

typedef struct
{
    Command commands[CHAIN_MAX_COMMANDS];
    Command *children[CHAIN_MAX_COMMANDS];
    Command root;
    unsigned char count;
} InternalCommandChain;

static InternalCommandChain active_chain;

static void scheduleCommand(Robot *robot, Command *root);
static RobotCommandChain getChainApi(void);

static unsigned int secondsToTicks(int time_seconds)
{
    unsigned int ticks;

    if (time_seconds <= 0)
    {
        return 1;
    }

    ticks = (unsigned int)time_seconds * COMMAND_TICKS_PER_SECOND;
    if (ticks == 0)
    {
        ticks = 1;
    }

    return ticks;
}

static float wrapAngle(float angle)
{
    while (angle > 180.0f)
    {
        angle -= 360.0f;
    }
    while (angle < -180.0f)
    {
        angle += 360.0f;
    }
    return angle;
}

static float absoluteFloat(float value)
{
    if (value < 0.0f)
    {
        return -value;
    }
    return value;
}

static int pwmCountsFromPercent(unsigned int duty_percent)
{
    if (duty_percent >= 100)
    {
        return WHEEL_PERIOD;
    }

    return (int)(((long)WHEEL_PERIOD * (long)duty_percent) / 100L);
}

static unsigned int pwmCountsFromPercentFloat(float duty_percent)
{
    if (duty_percent <= 0.0f)
    {
        return 0U;
    }

    if (duty_percent >= 100.0f)
    {
        return WHEEL_PERIOD;
    }

    return (unsigned int)(((float)WHEEL_PERIOD * duty_percent) / 100.0f);
}

static int pwmCountsFromPercentFloatSigned(float duty_percent)
{
    if (duty_percent <= -100.0f)
    {
        return -WHEEL_PERIOD;
    }

    if (duty_percent >= 100.0f)
    {
        return WHEEL_PERIOD;
    }

    return (int)(((float)WHEEL_PERIOD * duty_percent) / 100.0f);
}

static int clampSignedPwm(int value)
{
    if (value > (int)WHEEL_PERIOD)
    {
        return (int)WHEEL_PERIOD;
    }
    if (value < -(int)WHEEL_PERIOD)
    {
        return -(int)WHEEL_PERIOD;
    }
    return value;
}

static void applyMixedDrive(int left_signed_pwm, int right_signed_pwm)
{
    unsigned int left_mag;
    unsigned int right_mag;

    left_signed_pwm = clampSignedPwm(left_signed_pwm);
    right_signed_pwm = clampSignedPwm(right_signed_pwm);
    left_mag = (unsigned int)((left_signed_pwm < 0) ? -left_signed_pwm : left_signed_pwm);
    right_mag = (unsigned int)((right_signed_pwm < 0) ? -right_signed_pwm : right_signed_pwm);

    if ((left_signed_pwm >= 0) && (right_signed_pwm >= 0))
    {
        Motors_DriveForwardPWM(left_mag, right_mag);
    }
    else if ((left_signed_pwm <= 0) && (right_signed_pwm <= 0))
    {
        Motors_DriveReversePWM(left_mag, right_mag);
    }
    else if ((left_signed_pwm >= 0) && (right_signed_pwm <= 0))
    {
        Motors_DriveSpinPWM(left_mag, right_mag);
    }
    else
    {
        Motors_DriveSpinReversePWM(left_mag, right_mag);
    }
}

static void commandTickParallelDriveChild(Command *child,
                                          int *throttle_sum,
                                          int *turn_sum,
                                          unsigned char *has_drive_output,
                                          unsigned char *has_throttle)
{
    unsigned int base_pwm;
    unsigned int turn_pwm;

    if ((child == 0) || child->finished)
    {
        return;
    }

    if (!child->started)
    {
        child->started = 1;
    }

    base_pwm = pwmCountsFromPercent(65U);
    turn_pwm = pwmCountsFromPercent(40U);

    if (child->type == CMD_FORWARD)
    {
        *throttle_sum += (int)base_pwm;
        *has_drive_output = 1;
        *has_throttle = 1;
    }
    else if (child->type == CMD_REVERSE)
    {
        *throttle_sum -= (int)base_pwm;
        *has_drive_output = 1;
        *has_throttle = 1;
    }
    else if (child->type == CMD_SPIN)
    {
        if (child->spin_direction == SPIN_COUNTERCLOCKWISE)
        {
            *turn_sum -= (int)turn_pwm;
        }
        else
        {
            *turn_sum += (int)turn_pwm;
        }
        *has_drive_output = 1;
    }

    child->elapsed_ticks++;
    if (child->elapsed_ticks >= child->duration_ticks)
    {
        child->finished = 1;
    }
}

static unsigned int clampPwmCounts(int duty_counts)
{
    if (duty_counts < 0)
    {
        return 0U;
    }

    if (duty_counts > (int)WHEEL_PERIOD)
    {
        return WHEEL_PERIOD;
    }

    return (unsigned int)duty_counts;
}

static void setCurrentCommandDisplay(const char *name)
{
    char line[11] = "          ";
    unsigned int index = 0;

    while ((name[index] != '\0') && (index < 10U))
    {
        line[index] = name[index];
        index++;
    }

    line[10] = '\0';
    if (strncmp(display_line[3], line, 10) != 0)
    {
        memcpy(display_line[3], line, 11);
        display_changed = TRUE;
    }
}

static void setFollowLineElapsedDisplay(const Command *command)
{
    char line[11] = "L00 000.0s";
    unsigned int elapsed_tenths;
    unsigned int seconds;
    unsigned int tenths;

    elapsed_tenths = ((unsigned int)((unsigned long)command->elapsed_ticks * 10UL)) / COMMAND_TICKS_PER_SECOND;
    seconds = elapsed_tenths / 10U;
    tenths = elapsed_tenths % 10U;

    if (seconds > 999U)
    {
        seconds = 999U;
    }

    line[1] = (char)('0' + project7flag);
    line[2] = (char)('0' + command->lf_lap_count);
    line[4] = (char)('0' + ((seconds / 100U) % 10U));
    line[5] = (char)('0' + ((seconds / 10U) % 10U));
    line[6] = (char)('0' + (seconds % 10U));
    line[8] = (char)('0' + (tenths % 10U));

    if (strncmp(display_line[3], line, 10) != 0)
    {
        memcpy(display_line[3], line, 11);
        display_changed = TRUE;
    }
}

static void setTurnToAngleDisplay(const Command *command, float error)
{
    char line[11] = "          ";

    format_float(error, line);

    if (strncmp(display_line[3], line, 10) != 0)
    {
        memcpy(display_line[3], line, 11);
        display_changed = TRUE;
    }
}

static const char *getDisplayName(Command *cmd, const char *default_name)
{
    return (cmd->display_message != 0) ? cmd->display_message : default_name;
}

static void chainReset(void)
{
    active_chain.count = 0;
}

static void chainAppend(Command *command)
{
    if ((command == 0) || (active_chain.count >= CHAIN_MAX_COMMANDS))
    {
        return;
    }

    active_chain.children[active_chain.count] = command;
    active_chain.count++;
}

static RobotCommandChain chainAndThenForward(int time_seconds)
{
    if (active_chain.count >= CHAIN_MAX_COMMANDS)
    {
        return getChainApi();
    }

    Command_Forward(&active_chain.commands[active_chain.count], time_seconds);
    chainAppend(&active_chain.commands[active_chain.count]);
    return getChainApi();
}

static RobotCommandChain chainAndThenReverse(int time_seconds)
{
    if (active_chain.count >= CHAIN_MAX_COMMANDS)
    {
        return getChainApi();
    }

    Command_Reverse(&active_chain.commands[active_chain.count], time_seconds);
    chainAppend(&active_chain.commands[active_chain.count]);
    return getChainApi();
}

static RobotCommandChain chainAndThenWait(int time_seconds)
{
    if (active_chain.count >= CHAIN_MAX_COMMANDS)
    {
        return getChainApi();
    }

    Command_Wait(&active_chain.commands[active_chain.count], time_seconds);
    chainAppend(&active_chain.commands[active_chain.count]);
    return getChainApi();
}

static RobotCommandChain chainAndThenSpinCW(int time_seconds)
{
    if (active_chain.count >= CHAIN_MAX_COMMANDS)
    {
        return getChainApi();
    }

    Command_Spin(&active_chain.commands[active_chain.count], time_seconds, SPIN_CLOCKWISE);
    chainAppend(&active_chain.commands[active_chain.count]);
    return getChainApi();
}

static RobotCommandChain chainAndThenSpinCCW(int time_seconds)
{
    if (active_chain.count >= CHAIN_MAX_COMMANDS)
    {
        return getChainApi();
    }

    Command_Spin(&active_chain.commands[active_chain.count], time_seconds, SPIN_COUNTERCLOCKWISE);
    chainAppend(&active_chain.commands[active_chain.count]);
    return getChainApi();
}

static RobotCommandChain chainAndThenDriveStraightMs(unsigned int ms)
{
    Command *cmd;

    if (active_chain.count >= CHAIN_MAX_COMMANDS)
    {
        return getChainApi();
    }

    cmd = &active_chain.commands[active_chain.count];
    Command_DriveStraight(cmd, 1);
    cmd->duration_ticks = COMMAND_TICKS_FROM_MS(ms);
    chainAppend(cmd);
    return getChainApi();
}

static RobotCommandChain chainAndThenReverseMs(unsigned int ms)
{
    Command *cmd;

    if (active_chain.count >= CHAIN_MAX_COMMANDS)
    {
        return getChainApi();
    }

    cmd = &active_chain.commands[active_chain.count];
    Command_Reverse(cmd, 1);
    cmd->duration_ticks = COMMAND_TICKS_FROM_MS(ms);
    chainAppend(cmd);
    return getChainApi();
}

static RobotCommandChain chainAndThenSpinCWMs(unsigned int ms, unsigned char duty_percent)
{
    Command *cmd;

    if (active_chain.count >= CHAIN_MAX_COMMANDS)
    {
        return getChainApi();
    }

    cmd = &active_chain.commands[active_chain.count];
    Command_Spin(cmd, 1, SPIN_CLOCKWISE);
    cmd->duration_ticks = COMMAND_TICKS_FROM_MS(ms);
    cmd->left_duty  = duty_percent;
    cmd->right_duty = duty_percent;
    chainAppend(cmd);
    return getChainApi();
}

static RobotCommandChain chainAndThenSpinCCWMs(unsigned int ms, unsigned char duty_percent)
{
    Command *cmd;

    if (active_chain.count >= CHAIN_MAX_COMMANDS)
    {
        return getChainApi();
    }

    cmd = &active_chain.commands[active_chain.count];
    Command_Spin(cmd, 1, SPIN_COUNTERCLOCKWISE);
    cmd->duration_ticks = COMMAND_TICKS_FROM_MS(ms);
    cmd->left_duty  = duty_percent;
    cmd->right_duty = duty_percent;
    chainAppend(cmd);
    return getChainApi();
}

static RobotCommandChain chainAndThenTurnToAngle(float target_angle_degrees)
{
    if (active_chain.count >= CHAIN_MAX_COMMANDS)
    {
        return getChainApi();
    }

    Command_TurnToAngle(&active_chain.commands[active_chain.count], target_angle_degrees);
    chainAppend(&active_chain.commands[active_chain.count]);
    return getChainApi();
}

static RobotCommandChain chainAndThenDriveStraight(int time_seconds)
{
    if (active_chain.count >= CHAIN_MAX_COMMANDS)
    {
        return getChainApi();
    }

    Command_DriveStraight(&active_chain.commands[active_chain.count], time_seconds);
    chainAppend(&active_chain.commands[active_chain.count]);
    return getChainApi();
}

static RobotCommandChain chainAndThenDriveToLine(void)
{
    if (active_chain.count >= CHAIN_MAX_COMMANDS)
    {
        return getChainApi();
    }

    Command_DriveToLine(&active_chain.commands[active_chain.count]);
    chainAppend(&active_chain.commands[active_chain.count]);
    return getChainApi();
}

static RobotCommandChain chainAndThenAlignLeftToLine(void)
{
    if (active_chain.count >= CHAIN_MAX_COMMANDS)
    {
        return getChainApi();
    }

    Command_AlignLeftToLine(&active_chain.commands[active_chain.count]);
    chainAppend(&active_chain.commands[active_chain.count]);
    return getChainApi();
}

static RobotCommandChain chainAndThenFollowLine(int time_seconds)
{
    if (active_chain.count >= CHAIN_MAX_COMMANDS)
    {
        return getChainApi();
    }

    Command_FollowLine(&active_chain.commands[active_chain.count], time_seconds);
    chainAppend(&active_chain.commands[active_chain.count]);
    return getChainApi();
}

static RobotCommandChain chainAndThenDriveUntil(DriveUntilFlag left_flag, DriveUntilFlag right_flag)
{
    if (active_chain.count >= CHAIN_MAX_COMMANDS)
    {
        return getChainApi();
    }

    Command_DriveUntil(&active_chain.commands[active_chain.count], left_flag, right_flag);
    chainAppend(&active_chain.commands[active_chain.count]);
    return getChainApi();
}

static RobotCommandChain chainUntil(DriveUntilFlag stop_flag)
{
    if (active_chain.count == 0)
    {
        return getChainApi();
    }

    active_chain.commands[active_chain.count - 1U].drive_until_flag = stop_flag;
    active_chain.commands[active_chain.count - 1U].drive_until_left_flag = 0;
    active_chain.commands[active_chain.count - 1U].drive_until_right_flag = 0;
    return getChainApi();
}

static RobotCommandChain chainUntilSelective(DriveUntilFlag left_stop_flag, DriveUntilFlag right_stop_flag)
{
    if (active_chain.count == 0)
    {
        return getChainApi();
    }

    active_chain.commands[active_chain.count - 1U].drive_until_flag = 0;
    active_chain.commands[active_chain.count - 1U].drive_until_left_flag = left_stop_flag;
    active_chain.commands[active_chain.count - 1U].drive_until_right_flag = right_stop_flag;
    return getChainApi();
}

static RobotCommandChain chainWithDisplay(const char *msg)
{
    if (active_chain.count == 0)
    {
        return getChainApi();
    }

    active_chain.commands[active_chain.count - 1U].display_message = msg;
    return getChainApi();
}

static RobotCommandChain chainAndThenReverseStraight(int time_seconds)
{
    if (active_chain.count >= CHAIN_MAX_COMMANDS)
    {
        return getChainApi();
    }

    Command_ReverseStraight(&active_chain.commands[active_chain.count], time_seconds);
    chainAppend(&active_chain.commands[active_chain.count]);
    return getChainApi();
}

static RobotCommandChain chainAndThenDriveToXY(float target_x_in, float target_y_in)
{
    if (active_chain.count >= CHAIN_MAX_COMMANDS)
    {
        return getChainApi();
    }

    Command_DriveToXY(&active_chain.commands[active_chain.count], target_x_in, target_y_in);
    chainAppend(&active_chain.commands[active_chain.count]);
    return getChainApi();
}

static RobotCommandChain chainAndThenDriveDistance(float inches)
{
    Command *cmd;

    if (active_chain.count >= CHAIN_MAX_COMMANDS)
    {
        return getChainApi();
    }

    cmd = &active_chain.commands[active_chain.count];
    Command_DriveDistance(cmd, inches);
    chainAppend(cmd);
    return getChainApi();
}

static RobotCommandChain chainAndThenTurnToAbsoluteAngle(float degrees)
{
    if (active_chain.count >= CHAIN_MAX_COMMANDS)
    {
        return getChainApi();
    }

    Command_TurnToAbsoluteAngle(&active_chain.commands[active_chain.count], degrees);
    chainAppend(&active_chain.commands[active_chain.count]);
    return getChainApi();
}

static RobotCommandChain chainWithTimeout(unsigned int timeout_ms)
{
    if (active_chain.count == 0)
    {
        return getChainApi();
    }

    active_chain.commands[active_chain.count - 1U].timeout_ticks =
        COMMAND_TICKS_FROM_MS(timeout_ms);
    return getChainApi();
}

static void chainSchedule(void)
{
    if ((active_robot == 0) || (active_chain.count == 0))
    {
        return;
    }

    Command_Sequence(&active_chain.root, active_chain.children, active_chain.count);
    scheduleCommand(active_robot, &active_chain.root);
}

static RobotCommandChain getChainApi(void)
{
    static RobotCommandChain chain_api_local;

    chain_api_local.andThenForward = chainAndThenForward;
    chain_api_local.andThenReverse = chainAndThenReverse;
    chain_api_local.andThenWait = chainAndThenWait;
    chain_api_local.andThenSpinCW = chainAndThenSpinCW;
    chain_api_local.andThenSpinCCW = chainAndThenSpinCCW;
    chain_api_local.andThenTurnToAngle = chainAndThenTurnToAngle;
    chain_api_local.andThenDriveStraight = chainAndThenDriveStraight;
    chain_api_local.andThenReverseStraight = chainAndThenReverseStraight;
    chain_api_local.andThenDriveToLine = chainAndThenDriveToLine;
    chain_api_local.andThenAlignLeftToLine = chainAndThenAlignLeftToLine;
    chain_api_local.andThenFollowLine      = chainAndThenFollowLine;
    chain_api_local.until = chainUntil;
    chain_api_local.untilSelective = chainUntilSelective;
    chain_api_local.withDisplay = chainWithDisplay;
    chain_api_local.andThenDriveUntil = chainAndThenDriveUntil;
    chain_api_local.andThenDriveToXY = chainAndThenDriveToXY;
    chain_api_local.andThenDriveDistance = chainAndThenDriveDistance;
    chain_api_local.andThenTurnToAbsoluteAngle = chainAndThenTurnToAbsoluteAngle;
    chain_api_local.withTimeout = chainWithTimeout;
    chain_api_local.schedule = chainSchedule;

    return chain_api_local;
}

static void commandResetRecursive(Command *command)
{
    unsigned char index;

    if (command == 0)
    {
        return;
    }

    command->elapsed_ticks = 0;
    command->started = 0;
    command->finished = 0;
    command->active_child = 0;

    for (index = 0; index < command->child_count; index++)
    {
        commandResetRecursive(command->children[index]);
    }
}

static void commandTick(Command *command)
{
    unsigned char index;
    unsigned char all_done;

    if ((command == 0) || command->finished)
    {
        return;
    }

    switch (command->type)
    {
    case CMD_WAIT:
        if (!command->started)
        {
            command->started = 1;
            setCurrentCommandDisplay(getDisplayName(command, "WAIT"));
        }

        command->elapsed_ticks++;
        if (command->elapsed_ticks >= command->duration_ticks)
        {
            command->finished = 1;
        }
        break;

    case CMD_FORWARD:
    case CMD_REVERSE:
    case CMD_SPIN:
        if (!command->started)
        {
            command->started = 1;
            if (command->type == CMD_FORWARD)
            {
                setCurrentCommandDisplay(getDisplayName(command, "FORWARD"));
                driveForward();
            }
            else if (command->type == CMD_REVERSE)
            {
                setCurrentCommandDisplay(getDisplayName(command, "REVERSE"));
                driveReverse();
            }
            else
            {
                setCurrentCommandDisplay(getDisplayName(command, "SPIN"));
                if (command->spin_direction == SPIN_COUNTERCLOCKWISE)
                {
                    if (command->left_duty > 0U)
                    {
                        Motors_DriveSpinReversePWM(
                            (unsigned int)pwmCountsFromPercent(command->left_duty),
                            (unsigned int)pwmCountsFromPercent(command->left_duty));
                    }
                    else
                    {
                        driveSpinReverse();
                    }
                }
                else
                {
                    if (command->left_duty > 0U)
                    {
                        Motors_DriveSpinPWM(
                            (unsigned int)pwmCountsFromPercent(command->left_duty),
                            (unsigned int)pwmCountsFromPercent(command->left_duty));
                    }
                    else
                    {
                        driveSpin();
                    }
                }
            }
        }

        if (Motors_IsSettled())
        {
            command->elapsed_ticks++;
        }

        if (command->drive_until_flag != 0)
        {
            if (*(command->drive_until_flag) != 0U)
            {
                driveStop();
                command->finished = 1;
            }
        }
        else if (command->elapsed_ticks >= command->duration_ticks)
        {
            driveStop();
            command->finished = 1;
        }
        break;

    case CMD_TURN_TO_ANGLE:
    {
        float heading;
        float error;
        float abs_error;
        float previous_error;
        float derivative;
        float feedback_control;
        float normalized_feedback;
        int signed_pwm;

        if (!command->started)
        {
            command->started = 1;
            command->heading_error_prev = 0.0f;
            command->heading_error_sum = 0.0f;
            command->pwm_counter = 0;
            setTurnToAngleDisplay(command, 0);
            if (command->left_duty == 0U)
            {
                command->target_angle = (-getHeading()) + command->target_angle;
            }
        }

        heading = -getHeading();
        previous_error = command->heading_error_prev;
        error = wrapAngle(command->target_angle - heading);
        abs_error = fabsf(error);
        
        derivative = wrapAngle(error - previous_error);
        setTurnToAngleDisplay(command, heading);

        command->heading_error_prev = error;
        if (abs_error <= turn_integral_zone_deg)
        {
            command->heading_error_sum += error;
            if (command->heading_error_sum > LF_INTEGRAL_MAX)
            {
                command->heading_error_sum = LF_INTEGRAL_MAX;
            }
            if (command->heading_error_sum < LF_INTEGRAL_MIN)
            {
                command->heading_error_sum = LF_INTEGRAL_MIN;
            }
        }
        else
        {
            command->heading_error_sum = 0.0f;
        }

        feedback_control = (TURN_KP_UNSCALED * error) + (TURN_KI_UNSCALED * command->heading_error_sum) + (TURN_KD_UNSCALED * derivative);
        normalized_feedback = feedback_control / MAX_ANGULAR_VELOCITY_DEG_S;

        signed_pwm = pwmCountsFromPercentFloatSigned(normalized_feedback*100.0f);
    
        applyMixedDrive(-signed_pwm, signed_pwm);

        command->elapsed_ticks++;
        if (abs_error <= turn_angle_tolerance_deg)
        {
            if (command->pwm_counter < 255U)
            {
                command->pwm_counter++;
            }
        }
        else
        {
            command->pwm_counter = 0U;
        }

        if (command->pwm_counter >= COMMAND_TICKS_FROM_MS(120U))
        {
            driveStop();
            command->finished = 1;
            break;
        }
        break;
    }

    case CMD_DRIVE_STRAIGHT:
    {
        float heading;
        float error;
        float derivative;
        float control;
        float normalized_control;
        int correction;
        int left_pwm;
        int right_pwm;
        int max_correction_pwm;
        unsigned int base_pwm;
        unsigned int min_pwm;

        if (!command->started)
        {
            command->started = 1;
            command->heading_start = getHeading();
            command->heading_error_prev = 0.0f;
            command->heading_error_sum = 0.0f;
            setCurrentCommandDisplay(getDisplayName(command, "STRAIGHT"));

            if (command->drive_until_flag != 0)
            {
                *(command->drive_until_flag) = 0;
            }
            if (command->drive_until_left_flag != 0)
            {
                *(command->drive_until_left_flag) = 0;
            }
            if (command->drive_until_right_flag != 0)
            {
                *(command->drive_until_right_flag) = 0;
            }
        }

        heading = getHeading();
        error = wrapAngle(command->heading_start - heading);
        derivative = error - command->heading_error_prev;
        command->heading_error_prev = error;
        command->heading_error_sum += error;

        control = (drive_straight_kp * error) + (drive_straight_kd * derivative);
        control *= drive_straight_correction_sign;

        if (absoluteFloat(error) <= drive_straight_deadband_deg)
        {
            control = 0.0f;
        }

        base_pwm = pwmCountsFromPercentFloat(drive_straight_base_percent);
        min_pwm = pwmCountsFromPercent(MOTOR_MIN_DUTY_PERCENT);
        max_correction_pwm = (int)pwmCountsFromPercentFloat(drive_straight_max_correction_percent);

        normalized_control = control / 90.0f;
        if (normalized_control > 1.0f)
        {
            normalized_control = 1.0f;
        }
        if (normalized_control < -1.0f)
        {
            normalized_control = -1.0f;
        }

        correction = (int)((float)max_correction_pwm * normalized_control);
        left_pwm = (int)base_pwm - correction;
        right_pwm = (int)base_pwm + correction;

        if ((left_pwm > 0) && (left_pwm < (int)min_pwm))
        {
            left_pwm = (int)min_pwm;
        }
        if ((right_pwm > 0) && (right_pwm < (int)min_pwm))
        {
            right_pwm = (int)min_pwm;
        }

        if ((command->drive_until_left_flag != 0) || (command->drive_until_right_flag != 0))
        {
            unsigned int left_hit = 0U;
            unsigned int right_hit = 0U;

            if (command->elapsed_ticks >= COMMAND_TICKS_FROM_MS(100U))
            {
                if ((command->drive_until_left_flag != 0) && (*(command->drive_until_left_flag) != 0U))
                {
                    left_pwm = 0;
                    left_hit = 1U;
                }
                if ((command->drive_until_right_flag != 0) && (*(command->drive_until_right_flag) != 0U))
                {
                    right_pwm = 0;
                    right_hit = 1U;
                }
            }

            Motors_DriveForwardPWM(clampPwmCounts(left_pwm), clampPwmCounts(right_pwm));
            command->elapsed_ticks++;

            if (left_hit && right_hit)
            {
                driveStop();
                command->finished = 1;
            }
        }
        else
        {
            Motors_DriveForwardPWM(clampPwmCounts(left_pwm), clampPwmCounts(right_pwm));

            command->elapsed_ticks++;
            if (command->drive_until_flag != 0)
            {
                if ((command->elapsed_ticks >= COMMAND_TICKS_FROM_MS(100U)) && (*(command->drive_until_flag) != 0U))
                {
                    driveStop();
                    command->finished = 1;
                }
            }
            else if (command->elapsed_ticks >= command->duration_ticks)
            {
                driveStop();
                command->finished = 1;
            }
        }
        break;
    }

    case CMD_DRIVE_UNTIL:
    {
        float heading;
        float error;
        float derivative;
        float control;
        float normalized_control;
        int correction;
        int left_pwm;
        int right_pwm;
        int max_correction_pwm;
        unsigned int base_pwm;
        unsigned int min_pwm;

        unsigned int left_stopped;
        unsigned int right_stopped;

        if (!command->started)
        {
            command->started = 1;
            command->heading_start = getHeading();
            command->heading_error_prev = 0.0f;
            command->heading_error_sum = 0.0f;
            setCurrentCommandDisplay(getDisplayName(command, "DRV UNTIL"));

            if (command->drive_until_left_flag != 0)
            {
                *(command->drive_until_left_flag) = 0U;
            }

            if (command->drive_until_right_flag != 0)
            {
                *(command->drive_until_right_flag) = 0U;
            }
        }

        heading = getHeading();
        error = wrapAngle(command->heading_start - heading);
        derivative = error - command->heading_error_prev;
        command->heading_error_prev = error;
        command->heading_error_sum += error;

        control = (drive_straight_kp * error) + (drive_straight_kd * derivative);
        control *= drive_straight_correction_sign;

        if (absoluteFloat(error) <= drive_straight_deadband_deg)
        {
            control = 0.0f;
        }

        base_pwm = pwmCountsFromPercentFloat(drive_straight_base_percent);
        min_pwm = pwmCountsFromPercent(MOTOR_MIN_DUTY_PERCENT);
        max_correction_pwm = (int)pwmCountsFromPercentFloat(drive_straight_max_correction_percent);

        normalized_control = control / 90.0f;
        if (normalized_control > 1.0f)
        {
            normalized_control = 1.0f;
        }
        if (normalized_control < -1.0f)
        {
            normalized_control = -1.0f;
        }

        correction = (int)((float)max_correction_pwm * normalized_control);
        left_pwm = (int)base_pwm + correction;
        right_pwm = (int)base_pwm - correction;

        if ((left_pwm > 0) && (left_pwm < (int)min_pwm))
        {
            left_pwm = (int)min_pwm;
        }
        if ((right_pwm > 0) && (right_pwm < (int)min_pwm))
        {
            right_pwm = (int)min_pwm;
        }

        if ((command->elapsed_ticks >= COMMAND_TICKS_FROM_MS(100U)))
        {

            if ((command->drive_until_left_flag != 0) && (*(command->drive_until_left_flag) != 0U))
            {
                right_pwm = 0;
                left_stopped = 1;
            }
            if ((command->drive_until_left_flag != 0) && (*(command->drive_until_left_flag) != 0U)) 
            {
                left_pwm = 0;
                right_stopped = 1;
            }
            if (left_stopped && right_stopped)
            {
                driveStop();
                command->finished = 1;
                break;
            }
        }

        applyMixedDrive(clampPwmCounts(left_pwm), clampPwmCounts(right_pwm));

        command->elapsed_ticks++;
        break;
    }

    case CMD_DRIVE_TO_LINE:
    {
        float left_white_level;
        float right_white_level;
        float left_percent;
        float right_percent;
        float avg_percent;
        float white_difference;
        float avg_white;
        float heading;
        float error;
        float derivative;
        float control;
        float normalized_control;
        float heading_correction_percent;
        unsigned int left_pwm;
        unsigned int right_pwm;
        const float drive_to_line_white_kp_percent = 90.0f;
        const float drive_to_line_black_lock_level = 0.14f;
        const float drive_to_line_equal_tolerance = 0.06f;

        if (!command->started)
        {
            command->started = 1;
            command->heading_start = getHeading();
            command->heading_error_prev = 0.0f;
            command->heading_error_sum = 0.0f;
            command->pwm_counter = 0U;
            setCurrentCommandDisplay(getDisplayName(command, "TO LINE"));
        }

        left_white_level = getDetectorWhiteLevel(DETECTOR_LEFT);
        right_white_level = getDetectorWhiteLevel(DETECTOR_RIGHT);

        left_percent = drive_to_line_white_kp_percent * left_white_level;
        right_percent = drive_to_line_white_kp_percent * right_white_level;

        white_difference = absoluteFloat(left_white_level - right_white_level);
        avg_white = 0.5f * (left_white_level + right_white_level);

        if ((white_difference <= drive_to_line_equal_tolerance) &&
            (avg_white > drive_to_line_black_lock_level))
        {
            heading = getHeading();
            error = wrapAngle(command->heading_start - heading);
            derivative = error - command->heading_error_prev;
            command->heading_error_prev = error;

            control = (drive_straight_kp * error) + (drive_straight_kd * derivative);
            control *= drive_straight_correction_sign;

            if (absoluteFloat(error) <= drive_straight_deadband_deg)
            {
                control = 0.0f;
            }

            normalized_control = control / 90.0f;
            if (normalized_control > 1.0f)
            {
                normalized_control = 1.0f;
            }
            if (normalized_control < -1.0f)
            {
                normalized_control = -1.0f;
            }

            avg_percent = 0.5f * (left_percent + right_percent);
            heading_correction_percent = drive_straight_max_correction_percent * normalized_control;
            left_percent = avg_percent - heading_correction_percent;
            right_percent = avg_percent + heading_correction_percent;
        }

        if (left_percent < 0.0f)
        {
            left_percent = 0.0f;
        }
        if (right_percent < 0.0f)
        {
            right_percent = 0.0f;
        }

        if (left_percent > 100.0f)
        {
            left_percent = 100.0f;
        }
        if (right_percent > 100.0f)
        {
            right_percent = 100.0f;
        }

        left_pwm = (left_percent <= 0.0f) ? 0U : pwmCountsFromPercentFloat(left_percent);
        right_pwm = (right_percent <= 0.0f) ? 0U : pwmCountsFromPercentFloat(right_percent);

        Motors_DriveForwardPWM(clampPwmCounts((int)left_pwm), clampPwmCounts((int)right_pwm));

        if ((left_white_level <= drive_to_line_black_lock_level) &&
            (right_white_level <= drive_to_line_black_lock_level))
        {
            if (command->pwm_counter < 255U)
            {
                command->pwm_counter++;
            }
        }
        else
        {
            command->pwm_counter = 0U;
        }

        command->elapsed_ticks++;
        if ((command->pwm_counter >= COMMAND_TICKS_FROM_MS(100U)) || (command->elapsed_ticks >= secondsToTicks(30)))
        {
            driveStop();
            command->finished = 1;
        }
        break;
    }

    case CMD_ALIGN_LEFT_TO_LINE:
    {

        int left_color;
        int right_color;
        unsigned int left_pwm;
        unsigned int right_pwm;

        if (!command->started)
        {
            command->started = 1;
            command->pwm_counter = 0U;
            setCurrentCommandDisplay(getDisplayName(command, "ALIGN LINE"));
        }

        left_color  = getDetectorValue(DETECTOR_RIGHT);
        right_color = getDetectorValue(DETECTOR_LEFT);

        left_pwm = pwmCountsFromPercentFloatSigned(60.0);
        right_pwm = pwmCountsFromPercentFloatSigned(-60.0);

        applyMixedDrive(left_pwm, right_pwm);

        if (right_color - left_color > 50) //right is greater by 50 means it is on black.
        {
            if (command->pwm_counter < 255U)
            {
                command->pwm_counter++;
            }
        }
        else {
            command->pwm_counter = 0U;
        }

        int BLACK_LEVEL = 70;

     
                                   
        if ((left_color > BLACK_LEVEL) && (right_color < left_color))
        {
            if (command->pwm_counter < 255U)
            {
                command->pwm_counter++;
            }
        }
        else
        {
            command->pwm_counter = 0U;
        }

        command->elapsed_ticks++;
        if ((command->pwm_counter >= COMMAND_TICKS_FROM_MS(100U)) ||
            (command->elapsed_ticks >= secondsToTicks(5)))
        {
            driveStop();
            command->finished = 1;
        }

        break;
    }

case CMD_FOLLOW_LINE:
    {
        if (!command->started)
        {
            command->started       = 1;
            command->elapsed_ticks = 0U;
            lf_integral            = 0;
            lf_prev_error          = 0;
            lf_wasInGap            = 0;
            lf_coastFrames         = 0;
            lf_coastMv             = 0;
            setFollowLineElapsedDisplay(command);
            zeroHeading();
            command->lf_lap_count = -1;
        }

        int irL = getDetectorValue(DETECTOR_LEFT);
        int irR = getDetectorValue(DETECTOR_RIGHT);

        int steerError = irL - irR;
        int sumError   = (irL + irR) - LF_TARGET_SUM;

        lf_integral += steerError;
        if (lf_integral >  LF_INTEGRAL_MAX) lf_integral =  LF_INTEGRAL_MAX;
        if (lf_integral <  LF_INTEGRAL_MIN) lf_integral =  LF_INTEGRAL_MIN;

        int derivative = steerError - lf_prev_error;
        lf_prev_error  = steerError;

        int mv_scaled = (LF_KP_SCALED * steerError)
                      + (LF_KI_SCALED * lf_integral)
                      + (LF_KD_SCALED * derivative);
        int mv = mv_scaled >> 10;

        if (mv >  LF_CORRECTION_MAX) mv =  LF_CORRECTION_MAX;
        if (mv <  LF_CORRECTION_MIN) mv =  LF_CORRECTION_MIN;

        int speedTrim_scaled = LF_KP_SUM_SCALED * sumError;
        int speedTrim        = speedTrim_scaled >> 10;
        if (speedTrim < 0) speedTrim = -speedTrim;

        int steerAbs = mv < 0 ? -mv : mv;
        int minBase  = steerAbs > 10 ? steerAbs : 10;

        int baseSpeed = LF_BASE_SPEED - speedTrim;
        if (baseSpeed < minBase) baseSpeed = minBase;

        int leftDuty  = LF_BASE_SPEED - mv;
        int rightDuty = LF_BASE_SPEED + mv;

        if (leftDuty  >  100) leftDuty  =  100;
        if (leftDuty  < -100) leftDuty  = -100;
        if (rightDuty >  100) rightDuty =  100;
        if (rightDuty < -100) rightDuty = -100;

        applyMixedDrive(pwmCountsFromPercentFloatSigned(leftDuty),
                        pwmCountsFromPercentFloatSigned(rightDuty));

        float heading = -getHeading();
        if (fabsf(heading) <= 5.0f && project7count == 0)
        {
            command->lf_lap_count++;
            project7count = 1;
        }
        else if (project7count == 1 && fabsf(heading) > 5.0f)
        {
            project7count = 0;
        }

        if (command->lf_lap_count >= 1) project7flag = 1;

        command->elapsed_ticks++;
        setFollowLineElapsedDisplay(command);

        if (((command->drive_until_flag != 0) && (*(command->drive_until_flag) != 0)) || command->elapsed_ticks >= command->duration_ticks)
        {
            driveStop();
            command->finished = 1;
            lf_integral    = 0;
            lf_prev_error  = 0;
            break;
        }

        break;
    }

    case CMD_REVERSE_STRAIGHT:
    {
        float heading;
        float error;
        float derivative;
        float control;
        float normalized_control;
        int correction;
        int left_pwm;
        int right_pwm;
        int max_correction_pwm;
        unsigned int base_pwm;
        unsigned int min_pwm;

        if (!command->started)
        {
            command->started = 1;
            command->heading_start = getHeading();
            command->heading_error_prev = 0.0f;
            command->heading_error_sum = 0.0f;
            setCurrentCommandDisplay(getDisplayName(command, "REV STR"));
        }

        heading = getHeading();
        error = wrapAngle(command->heading_start - heading);
        derivative = error - command->heading_error_prev;
        command->heading_error_prev = error;
        command->heading_error_sum += error;

        control = (drive_straight_kp * error) + (drive_straight_kd * derivative);
        control *= drive_straight_correction_sign;

        if (absoluteFloat(error) <= drive_straight_deadband_deg)
        {
            control = 0.0f;
        }

        base_pwm = pwmCountsFromPercentFloat(drive_straight_base_percent);
        min_pwm = pwmCountsFromPercent(MOTOR_MIN_DUTY_PERCENT);
        max_correction_pwm = (int)pwmCountsFromPercentFloat(drive_straight_max_correction_percent);

        normalized_control = control / 90.0f;
        if (normalized_control > 1.0f)
        {
            normalized_control = 1.0f;
        }
        if (normalized_control < -1.0f)
        {
            normalized_control = -1.0f;
        }

        correction = (int)((float)max_correction_pwm * normalized_control);
        left_pwm = (int)base_pwm - correction;
        right_pwm = (int)base_pwm + correction;

        if ((left_pwm > 0) && (left_pwm < (int)min_pwm))
        {
            left_pwm = (int)min_pwm;
        }
        if ((right_pwm > 0) && (right_pwm < (int)min_pwm))
        {
            right_pwm = (int)min_pwm;
        }

        if ((command->drive_until_left_flag != 0) || (command->drive_until_right_flag != 0))
        {
            unsigned int left_hit = 0U;
            unsigned int right_hit = 0U;

            if ((command->drive_until_left_flag != 0) && (*(command->drive_until_left_flag) != 0U))
            {
                left_pwm = 0;
                left_hit = 1U;
            }
            if ((command->drive_until_right_flag != 0) && (*(command->drive_until_right_flag) != 0U))
            {
                right_pwm = 0;
                right_hit = 1U;
            }

            Motors_DriveReversePWM(clampPwmCounts(left_pwm), clampPwmCounts(right_pwm));
            command->elapsed_ticks++;

            if (left_hit && right_hit)
            {
                driveStop();
                command->finished = 1;
            }
        }
        else
        {
            Motors_DriveReversePWM(clampPwmCounts(left_pwm), clampPwmCounts(right_pwm));

            command->elapsed_ticks++;
            if (command->drive_until_flag != 0)
            {
                if (*(command->drive_until_flag) != 0U)
                {
                    driveStop();
                    command->finished = 1;
                }
            }
            else if (command->elapsed_ticks >= command->duration_ticks)
            {
                driveStop();
                command->finished = 1;
            }
        }
        break;
    }

    case CMD_DRIVE_TO_XY:
    {
        float dx, dy, dist;
        float angle_to_target, heading_err;
        float v, omega, left_pct, right_pct;

        if (!command->started)
        {
            command->started       = 1;
            command->elapsed_ticks = 0;
            setCurrentCommandDisplay(getDisplayName(command, "GO TO XY"));
        }

        dx = command->target_x - getPositionX();
        dy = command->target_y - getPositionY();
        dist = sqrtf(dx * dx + dy * dy);

        if (dist <= drive_to_xy_tolerance_in)
        {
            driveStop();
            command->finished = 1;
            break;
        }

        angle_to_target = atan2f(dx, dy) * (180.0f / FT_PI);
        heading_err     = wrapAngle(angle_to_target - getHeading());

        /* Linear velocity: proportional to distance, capped */
        v = drive_to_xy_kv * dist;
        if (v > drive_to_xy_speed_percent) { v = drive_to_xy_speed_percent; }

        /* Angular velocity: proportional to heading error */
        omega = drive_to_xy_komega * heading_err;
        if (omega >  drive_to_xy_speed_percent) { omega =  drive_to_xy_speed_percent; }
        if (omega < -drive_to_xy_speed_percent) { omega = -drive_to_xy_speed_percent; }

        left_pct  = v - omega;
        right_pct = v + omega;

        if (left_pct  >  100.0f) { left_pct  =  100.0f; }
        if (left_pct  < -100.0f) { left_pct  = -100.0f; }
        if (right_pct >  100.0f) { right_pct =  100.0f; }
        if (right_pct < -100.0f) { right_pct = -100.0f; }

        applyMixedDrive(pwmCountsFromPercentFloatSigned(left_pct),
                        pwmCountsFromPercentFloatSigned(right_pct));

        command->elapsed_ticks++;
        if (command->elapsed_ticks >= secondsToTicks(30))
        {
            driveStop();
            command->finished = 1;
        }
        break;
    }

    case CMD_DRIVE_DISTANCE:
    {
        float dx, dy, dist_traveled;
        float heading, error, derivative, control, normalized_control;
        int correction, left_pwm, right_pwm, max_correction_pwm;
        unsigned int base_pwm, min_pwm;

        if (!command->started)
        {
            command->started = 1;
            command->target_x = getPositionX();
            command->target_y = getPositionY();
            command->heading_start = getHeading();
            command->heading_error_prev = 0.0f;
            setCurrentCommandDisplay(getDisplayName(command, "DRIVE DIST"));
        }

        dx = getPositionX() - command->target_x;
        dy = getPositionY() - command->target_y;
        dist_traveled = sqrtf(dx * dx + dy * dy);

        if (dist_traveled >= command->target_angle)
        {
            driveStop();
            command->finished = 1;
            break;
        }

        heading = getHeading();
        error = wrapAngle(command->heading_start - heading);
        derivative = error - command->heading_error_prev;
        command->heading_error_prev = error;

        control = (drive_straight_kp * error) + (drive_straight_kd * derivative);
        control *= drive_straight_correction_sign;

        if (absoluteFloat(error) <= drive_straight_deadband_deg)
        {
            control = 0.0f;
        }

        base_pwm = pwmCountsFromPercentFloat(drive_straight_base_percent);
        min_pwm = pwmCountsFromPercent(MOTOR_MIN_DUTY_PERCENT);
        max_correction_pwm = (int)pwmCountsFromPercentFloat(drive_straight_max_correction_percent);

        normalized_control = control / 90.0f;
        if (normalized_control >  1.0f) { normalized_control =  1.0f; }
        if (normalized_control < -1.0f) { normalized_control = -1.0f; }

        correction = (int)((float)max_correction_pwm * normalized_control);
        left_pwm  = (int)base_pwm - correction;
        right_pwm = (int)base_pwm + correction;

        if ((left_pwm  > 0) && (left_pwm  < (int)min_pwm)) { left_pwm  = (int)min_pwm; }
        if ((right_pwm > 0) && (right_pwm < (int)min_pwm)) { right_pwm = (int)min_pwm; }

        Motors_DriveForwardPWM(clampPwmCounts(left_pwm), clampPwmCounts(right_pwm));

        command->elapsed_ticks++;
        {
            /* Hard deadline: user-supplied timeout, or fallback 60-second limit */
            unsigned int deadline = (command->timeout_ticks > 0U)
                                    ? command->timeout_ticks
                                    : (unsigned int)secondsToTicks(60);
            if (command->elapsed_ticks >= deadline)
            {
                driveStop();
                command->finished = 1;
            }
        }
        break;
    }

    case CMD_SEQUENCE:
        if (command->child_count == 0)
        {
            command->finished = 1;
            break;
        }

        if (command->active_child < command->child_count)
        {
            commandTick(command->children[command->active_child]);
            if (command->children[command->active_child]->finished)
            {
                command->active_child++;
            }
        }

        if (command->active_child >= command->child_count)
        {
            command->finished = 1;
        }
        break;

    case CMD_PARALLEL:
    {
        int throttle_sum = 0;
        int turn_sum = 0;
        int left_sum = 0;
        int right_sum = 0;
        int min_pwm = 0;
        unsigned char has_drive_output = 0;
        unsigned char has_throttle = 0;

        if (command->child_count == 0)
        {
            command->finished = 1;
            break;
        }

        if (!command->started)
        {
            command->started = 1;
            setCurrentCommandDisplay(getDisplayName(command, "PARALLEL"));
        }

        all_done = 1;
        for (index = 0; index < command->child_count; index++)
        {
            if ((command->children[index] != 0) &&
                ((command->children[index]->type == CMD_FORWARD) ||
                 (command->children[index]->type == CMD_REVERSE) ||
                 (command->children[index]->type == CMD_SPIN)))
            {
                commandTickParallelDriveChild(command->children[index],
                                              &throttle_sum,
                                              &turn_sum,
                                              &has_drive_output,
                                              &has_throttle);
            }
            else
            {
                commandTick(command->children[index]);
            }

            if (!command->children[index]->finished)
            {
                all_done = 0;
            }
        }

        if (has_drive_output)
        {
            left_sum = throttle_sum + turn_sum;
            right_sum = throttle_sum - turn_sum;

            if (has_throttle)
            {
                min_pwm = (int)pwmCountsFromPercent(MOTOR_MIN_DUTY_PERCENT);

                if (throttle_sum > 0)
                {
                    if ((left_sum > 0) && (left_sum < min_pwm))
                    {
                        left_sum = min_pwm;
                    }
                    if ((right_sum > 0) && (right_sum < min_pwm))
                    {
                        right_sum = min_pwm;
                    }
                    if (left_sum < 0)
                    {
                        left_sum = min_pwm;
                    }
                    if (right_sum < 0)
                    {
                        right_sum = min_pwm;
                    }
                }
                else if (throttle_sum < 0)
                {
                    if ((left_sum < 0) && ((-left_sum) < min_pwm))
                    {
                        left_sum = -min_pwm;
                    }
                    if ((right_sum < 0) && ((-right_sum) < min_pwm))
                    {
                        right_sum = -min_pwm;
                    }
                    if (left_sum > 0)
                    {
                        left_sum = -min_pwm;
                    }
                    if (right_sum > 0)
                    {
                        right_sum = -min_pwm;
                    }
                }
            }

            applyMixedDrive(left_sum, right_sum);
        }
        else
        {
            driveStop();
        }

        if (all_done)
        {
            command->finished = 1;
        }
        break;
    }

    default:
        command->finished = 1;
        break;
    }
}

static void scheduleCommand(Robot *robot, Command *root)
{
    if ((robot == 0) || (root == 0))
    {
        return;
    }

    commandResetRecursive(root);
    robot->active_command = root;
    robot->state = RUN;
}

unsigned char isRobotBusy(void)
{
    if ((active_robot != 0) && (active_robot->active_command != 0))
    {
        return 1U;
    }
    return 0U;
}

/* ── Streaming curvature drive ─────────────────────────────────────────────
 *
 *  fwd_pct  : forward speed percent  (-100 = full reverse, +100 = full fwd)
 *  turn_pct : turn rate percent      (-100 = hard left,    +100 = hard right)
 *
 *  Curvature model:
 *    left  = fwd_pct + turn_pct     (clamped to ±100)
 *    right = fwd_pct - turn_pct     (clamped to ±100)
 *
 *  Any active timed command is preempted so the robot responds immediately.
 * ────────────────────────────────────────────────────────────────────────── */

static unsigned long  s_curvature_last_tick = 0UL;
static unsigned char  s_curvature_active    = 0U;

#define CURVATURE_WATCHDOG_TICKS (15U)   /* 15 × 20 ms = 300 ms */

void applySpeedSet(float fwd_pct, float turn_pct)
{
    float left_pct;
    float right_pct;

    /* Preempt any running timed command */
    if (active_robot != 0)
    {
        active_robot->active_command = 0;
        active_robot->state          = STANDBY;
    }

    if ((fwd_pct == 0.0f) && (turn_pct == 0.0f))
    {
        driveStop();
        s_curvature_active = 0U;
        return;
    }

    left_pct  = fwd_pct + turn_pct;
    right_pct = fwd_pct - turn_pct;

    if (left_pct  >  100.0f) { left_pct  =  100.0f; }
    if (left_pct  < -100.0f) { left_pct  = -100.0f; }
    if (right_pct >  100.0f) { right_pct =  100.0f; }
    if (right_pct < -100.0f) { right_pct = -100.0f; }

    s_curvature_last_tick = one_second_timer;
    s_curvature_active    = 1U;

    applyMixedDrive(pwmCountsFromPercentFloatSigned(right_pct),
                    pwmCountsFromPercentFloatSigned(left_pct));
}

void robotCurvatureWatchdog(unsigned long tick)
{
    if (!s_curvature_active) { return; }

    if ((tick - s_curvature_last_tick) >= (unsigned long)CURVATURE_WATCHDOG_TICKS)
    {
        driveStop();
        s_curvature_active = 0U;
    }
}

static void switchProcess(Robot *robot)
{
    /* Switch handling is done by interrupt-based switches.c */
    (void)robot;
}

void Command_Forward(Command *command, int time_seconds)
{
    if (command == 0)
    {
        return;
    }

    command->type = CMD_FORWARD;
    command->duration_ticks = secondsToTicks(time_seconds);
    command->elapsed_ticks = 0;
    command->started = 0;
    command->finished = 0;
    command->child_count = 0;
    command->active_child = 0;
    command->drive_until_flag = 0;
    command->drive_until_left_flag = 0;
    command->drive_until_right_flag = 0;
    command->display_message = 0;
}

void Command_Wait(Command *command, int time_seconds)
{
    if (command == 0)
    {
        return;
    }

    command->type = CMD_WAIT;
    command->duration_ticks = secondsToTicks(time_seconds);
    command->elapsed_ticks = 0;
    command->started = 0;
    command->finished = 0;
    command->child_count = 0;
    command->active_child = 0;
    command->drive_until_flag = 0;
    command->drive_until_left_flag = 0;
    command->drive_until_right_flag = 0;
    command->display_message = 0;
}

void Command_TurnToAngle(Command *command, float target_angle_degrees)
{
    if (command == 0)
    {
        return;
    }

    command->type = CMD_TURN_TO_ANGLE;
    /* TurnToAngle is untimed; complete when angle reached */
    command->duration_ticks = 0;
    command->elapsed_ticks = 0;
    command->started = 0;
    command->finished = 0;
    command->child_count = 0;
    command->active_child = 0;
    command->target_angle = wrapAngle(target_angle_degrees);
    command->heading_start = 0.0f;
    command->heading_error_prev = 0.0f;
    command->heading_error_sum = 0.0f;
    command->drive_until_flag = 0;
    command->drive_until_left_flag = 0;
    command->drive_until_right_flag = 0;
    command->display_message = 0;
}

void Command_DriveStraight(Command *command, int time_seconds)
{
    if (command == 0)
    {
        return;
    }

    command->type = CMD_DRIVE_STRAIGHT;
    command->duration_ticks = secondsToTicks(time_seconds);
    command->elapsed_ticks = 0;
    command->started = 0;
    command->finished = 0;
    command->child_count = 0;
    command->active_child = 0;
    command->target_angle = 0.0f;
    command->heading_start = 0.0f;
    command->heading_error_prev = 0.0f;
    command->heading_error_sum = 0.0f;
    command->drive_until_flag = 0;
    command->drive_until_left_flag = 0;
    command->drive_until_right_flag = 0;
    command->display_message = 0;
}

void Command_DriveUntil(Command *command, DriveUntilFlag stop_left_flag, DriveUntilFlag stop_right_flag)
{
    if (command == 0)
    {
        return;
    }

    command->type = CMD_DRIVE_UNTIL;
    command->duration_ticks = 0;
    command->elapsed_ticks = 0;
    command->started = 0;
    command->finished = 0;
    command->child_count = 0;
    command->active_child = 0;
    command->target_angle = 0.0f;
    command->heading_start = 0.0f;
    command->heading_error_prev = 0.0f;
    command->heading_error_sum = 0.0f;
    command->drive_until_flag = 0;
    command->drive_until_left_flag = stop_left_flag;
    command->drive_until_right_flag = stop_right_flag;
    command->display_message = 0;
}

void Command_DriveToLine(Command *command)
{
    if (command == 0)
    {
        return;
    }

    command->type = CMD_DRIVE_TO_LINE;
    command->duration_ticks = 0;
    command->elapsed_ticks = 0;
    command->started = 0;
    command->finished = 0;
    command->child_count = 0;
    command->active_child = 0;
    command->target_angle = 0.0f;
    command->heading_start = 0.0f;
    command->heading_error_prev = 0.0f;
    command->heading_error_sum = 0.0f;
    command->pwm_counter = 0;
    command->left_duty = 0;
    command->right_duty = 0;
    command->drive_until_flag = 0;
    command->drive_until_left_flag = 0;
    command->drive_until_right_flag = 0;
    command->display_message = 0;
}

void Command_AlignLeftToLine(Command *command)
{
    if (command == 0)
    {
        return;
    }

    command->type = CMD_ALIGN_LEFT_TO_LINE;
    command->duration_ticks = 0;
    command->elapsed_ticks = 0;
    command->started = 0;
    command->finished = 0;
    command->child_count = 0;
    command->active_child = 0;
    command->target_angle = 0.0f;
    command->heading_start = 0.0f;
    command->heading_error_prev = 0.0f;
    command->heading_error_sum = 0.0f;
    command->pwm_counter = 0;
    command->left_duty = 0;
    command->right_duty = 0;
    command->drive_until_flag = 0;
    command->drive_until_left_flag = 0;
    command->drive_until_right_flag = 0;
    command->display_message = 0;
}

void Command_FollowLine(Command *command, int time_seconds)
{
    if (command == 0)
    {
        return;
    }

    command->type = CMD_FOLLOW_LINE;
    command->duration_ticks = secondsToTicks(time_seconds);
    command->elapsed_ticks = 0;
    command->started = 0;
    command->finished = 0;
    command->child_count = 0;
    command->active_child = 0;
    command->target_angle = 0.0f;
    command->heading_start = 0.0f;
    command->heading_error_prev = 0.0f;
    command->heading_error_sum = 0.0f;
    command->pwm_counter = 0;
    command->left_duty = 0;
    command->right_duty = 0;
    command->drive_until_flag = 0;
    command->drive_until_left_flag = 0;
    command->drive_until_right_flag = 0;
    command->display_message = 0;
    lf_heading_acc     = 0;
    lf_junction_frames = 0;
    lf_prev_heading    = getHeading();
}

void Command_ReverseStraight(Command *command, int time_seconds)
{
    if (command == 0)
    {
        return;
    }

    command->type = CMD_REVERSE_STRAIGHT;
    command->duration_ticks = secondsToTicks(time_seconds);
    command->elapsed_ticks = 0;
    command->started = 0;
    command->finished = 0;
    command->child_count = 0;
    command->active_child = 0;
    command->target_angle = 0.0f;
    command->heading_start = 0.0f;
    command->heading_error_prev = 0.0f;
    command->heading_error_sum = 0.0f;
    command->drive_until_flag = 0;
    command->drive_until_left_flag = 0;
    command->drive_until_right_flag = 0;
    command->display_message = 0;
}

void Command_DriveToXY(Command *command, float target_x_in, float target_y_in)
{
    if (command == 0)
    {
        return;
    }

    command->type = CMD_DRIVE_TO_XY;
    command->duration_ticks = 0;
    command->elapsed_ticks = 0;
    command->started = 0;
    command->finished = 0;
    command->child_count = 0;
    command->active_child = 0;
    command->target_x = target_x_in;
    command->target_y = target_y_in;
    command->target_angle = 0.0f;
    command->heading_start = 0.0f;
    command->heading_error_prev = 0.0f;
    command->heading_error_sum = 0.0f;
    command->pwm_counter = 0;
    command->left_duty = 0;
    command->right_duty = 0;
    command->spin_direction = SPIN_CLOCKWISE;
    command->drive_until_flag = 0;
    command->drive_until_left_flag = 0;
    command->drive_until_right_flag = 0;
    command->display_message = 0;
}

void Command_DriveDistance(Command *command, float inches)
{
    if (command == 0)
    {
        return;
    }

    command->type = CMD_DRIVE_DISTANCE;
    command->target_angle = inches;
    command->target_x = 0.0f;
    command->target_y = 0.0f;
    command->duration_ticks = 0;
    command->elapsed_ticks = 0;
    command->started = 0;
    command->finished = 0;
    command->child_count = 0;
    command->active_child = 0;
    command->heading_start = 0.0f;
    command->heading_error_prev = 0.0f;
    command->heading_error_sum = 0.0f;
    command->left_duty = 0;
    command->right_duty = 0;
    command->drive_until_flag = 0;
    command->drive_until_left_flag = 0;
    command->drive_until_right_flag = 0;
    command->display_message = 0;
    command->timeout_ticks = 0;   /* 0 = use the built-in 60-second hard limit */
}

void Command_TurnToAbsoluteAngle(Command *command, float degrees)
{
    if (command == 0)
    {
        return;
    }

    command->type = CMD_TURN_TO_ANGLE;
    command->duration_ticks = 0;
    command->elapsed_ticks = 0;
    command->started = 0;
    command->finished = 0;
    command->child_count = 0;
    command->active_child = 0;
    command->target_angle = wrapAngle(degrees);
    command->heading_start = 0.0f;
    command->heading_error_prev = 0.0f;
    command->heading_error_sum = 0.0f;
    command->left_duty = 1U;
    command->right_duty = 0;
    command->drive_until_flag = 0;
    command->drive_until_left_flag = 0;
    command->drive_until_right_flag = 0;
    command->pwm_counter = 0;
    command->display_message = 0;
}

void Command_Reverse(Command *command, int time_seconds)
{
    if (command == 0)
    {
        return;
    }

    command->type = CMD_REVERSE;
    command->duration_ticks = secondsToTicks(time_seconds);
    command->elapsed_ticks = 0;
    command->started = 0;
    command->finished = 0;
    command->child_count = 0;
    command->active_child = 0;
    command->drive_until_flag = 0;
    command->drive_until_left_flag = 0;
    command->drive_until_right_flag = 0;
    command->display_message = 0;
}

void Command_Spin(Command *command, int time_seconds, SpinDirection direction)
{
    if (command == 0)
    {
        return;
    }

    command->type = CMD_SPIN;
    command->duration_ticks = secondsToTicks(time_seconds);
    command->elapsed_ticks = 0;
    command->started = 0;
    command->finished = 0;
    command->child_count = 0;
    command->active_child = 0;
    command->spin_direction = direction;
    command->left_duty = 0;
    command->right_duty = 0;
    command->drive_until_flag = 0;
    command->drive_until_left_flag = 0;
    command->drive_until_right_flag = 0;
    command->display_message = 0;
}

void Command_Sequence(Command *command, Command **children, unsigned char child_count)
{
    unsigned char index;

    if (command == 0)
    {
        return;
    }

    command->type = CMD_SEQUENCE;
    command->duration_ticks = 0;
    command->elapsed_ticks = 0;
    command->started = 0;
    command->finished = 0;
    command->active_child = 0;
    command->child_count = (child_count > COMMAND_MAX_CHILDREN) ? COMMAND_MAX_CHILDREN : child_count;
    command->drive_until_flag = 0;
    command->drive_until_left_flag = 0;
    command->drive_until_right_flag = 0;
    command->display_message = 0;

    for (index = 0; index < command->child_count; index++)
    {
        command->children[index] = children[index];
    }
}

void Command_Parallel(Command *command, Command **children, unsigned char child_count)
{
    unsigned char index;

    if (command == 0)
    {
        return;
    }

    command->type = CMD_PARALLEL;
    command->duration_ticks = 0;
    command->elapsed_ticks = 0;
    command->started = 0;
    command->finished = 0;
    command->active_child = 0;
    command->child_count = (child_count > COMMAND_MAX_CHILDREN) ? COMMAND_MAX_CHILDREN : child_count;
    command->drive_until_flag = 0;
    command->drive_until_left_flag = 0;
    command->drive_until_right_flag = 0;
    command->display_message = 0;

    for (index = 0; index < command->child_count; index++)
    {
        command->children[index] = children[index];
    }
}

void initRobot(Robot *robot)
{
    PID_Init(&turnPID, TURN_KP, TURN_KI, TURN_KD, LF_INTEGRAL_MAX, LF_INTEGRAL_MIN, LF_CORRECTION_MAX, LF_CORRECTION_MIN, 2.0f);
    if (robot == 0)
    {
        return;
    }

    robot->state = STANDBY;
    robot->event = NONE;
    robot->active_command = 0;

    active_robot = robot;
}

void resetRobot(Robot *robot)
{
    if (robot == 0)
    {
        return;
    }

    robot->state = STANDBY;
    robot->active_command = 0;
    driveStop();
}

void setRobotShape(Robot *robot, Event shape)
{
    if (robot == 0)
    {
        return;
    }

    robot->event = shape;
    resetRobot(robot);
}

void updateRobot(Robot *robot, int Time_Sequence)
{
    static unsigned long last_command_tick = 0;
    unsigned long current_tick;

    (void)Time_Sequence;

    if (robot == 0)
    {
        return;
    }

    switchProcess(robot);

    current_tick = one_second_timer;
    if (current_tick == last_command_tick)
    {
        return;
    }

    if (robot->active_command != 0)
    {
        commandTick(robot->active_command);
        last_command_tick = current_tick;

        if (robot->active_command->finished)
        {
            robot->active_command = 0;
            robot->state = STANDBY;
            driveStop();
        }
    }
    else
    {
        last_command_tick = current_tick;
    }
}

RobotCommandChain chainForward(int time_seconds)
{
    chainReset();
    return chainAndThenForward(time_seconds);
}

RobotCommandChain chainReverse(int time_seconds)
{
    chainReset();
    return chainAndThenReverse(time_seconds);
}

RobotCommandChain chainWait(int time_seconds)
{
    chainReset();
    return chainAndThenWait(time_seconds);
}

RobotCommandChain chainSpinCW(int time_seconds)
{
    chainReset();
    return chainAndThenSpinCW(time_seconds);
}

RobotCommandChain chainSpinCCW(int time_seconds)
{
    chainReset();
    return chainAndThenSpinCCW(time_seconds);
}

RobotCommandChain chainTurnToAngle(float target_angle_degrees)
{
    chainReset();
    return chainAndThenTurnToAngle(target_angle_degrees);
}

RobotCommandChain chainDriveStraight(int time_seconds)
{
    chainReset();
    return chainAndThenDriveStraight(time_seconds);
}

RobotCommandChain chainReverseStraight(int time_seconds)
{
    chainReset();
    return chainAndThenReverseStraight(time_seconds);
}

RobotCommandChain chainDriveToLine(void)
{
    chainReset();
    return chainAndThenDriveToLine();
}

RobotCommandChain chainAlignLeftToLine(void)
{
    chainReset();
    return chainAndThenAlignLeftToLine();
}

RobotCommandChain chainFollowLine(int time_seconds)
{
    chainReset();
    return chainAndThenFollowLine(time_seconds);
}

RobotCommandChain chainDriveUntil(DriveUntilFlag left_flag, DriveUntilFlag right_flag)
{
    chainReset();
    return chainAndThenDriveUntil(left_flag, right_flag);
}

RobotCommandChain chainDriveToXY(float target_x_in, float target_y_in)
{
    chainReset();
    return chainAndThenDriveToXY(target_x_in, target_y_in);
}

RobotCommandChain chainDriveStraightMs(unsigned int ms)
{
    chainReset();
    return chainAndThenDriveStraightMs(ms);
}

RobotCommandChain chainReverseMs(unsigned int ms)
{
    chainReset();
    return chainAndThenReverseMs(ms);
}

RobotCommandChain chainSpinCWMs(unsigned int ms, unsigned char duty_percent)
{
    chainReset();
    return chainAndThenSpinCWMs(ms, duty_percent);
}

RobotCommandChain chainSpinCCWMs(unsigned int ms, unsigned char duty_percent)
{
    chainReset();
    return chainAndThenSpinCCWMs(ms, duty_percent);
}

RobotCommandChain chainDriveDistance(float inches)
{
    chainReset();
    return chainAndThenDriveDistance(inches);
}

RobotCommandChain chainTurnToAbsoluteAngle(float degrees)
{
    chainReset();
    return chainAndThenTurnToAbsoluteAngle(degrees);
}
