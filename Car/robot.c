#include "Include\robot.h"
#include "Include\functions.h"
#include <string.h>
#include <stdint.h>
#include "Include\imu.h"

static Robot *active_robot = 0;

float turn_kp = 0.87f;
float turn_ki = 0.03f;
float turn_kd = 0.41f;
// float turn_kp = 18.462f;
// float turn_ki =923.077f;
// float turn_kd = 0.092f;
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
float drive_straight_kp = 1.2f;
float drive_straight_kd = 0.00f;
float drive_straight_base_percent = 85.0f;
float drive_straight_max_correction_percent = 60.0f;
float drive_straight_deadband_deg = 0.5f;
float drive_straight_correction_sign = -1.0f;
float turn_angle_tolerance_deg = 2.0f;

static volatile int sw1_position = 0;
static volatile int sw2_position = 0;
static volatile int switch1_lock = 1;
static volatile int switch2_lock = 1;
static volatile int count_debounce_sw1 = 0;
static volatile int count_debounce_sw2 = 0;

#define CHAIN_MAX_COMMANDS (24)

typedef struct
{
    Command commands[CHAIN_MAX_COMMANDS];
    Command *children[CHAIN_MAX_COMMANDS];
    Command root;
    unsigned char count;
} InternalCommandChain;

static InternalCommandChain active_chain;
static RobotCommandChain chain_api;

#define TURN_SYSID_MAX_SAMPLES (40U)
#define TURN_SYSID_STEP_TICKS (25U)
#define TURN_AUTOTUNE_MAX_TRIALS (6U)
#define TURN_AUTOTUNE_TIMEOUT_TICKS (35U)
#define TURN_AUTOTUNE_SETTLE_TICKS (2U)
#define TURN_AUTOTUNE_TARGET_DEG (90.0f)
#define TURN_AUTOTUNE_OVERSHOOT_WEIGHT (5.0f)

typedef struct
{
    unsigned char sample_count;
    unsigned char breakaway_pwm_percent;
    unsigned char step_pwm_percent;
    unsigned char delay_ticks;
    unsigned char tau_ticks;
    float steady_rate_dps;
    float max_rate_dps;
    float gain_dps_per_percent;
    int16_t peak_accel_raw;
    float rate_samples[TURN_SYSID_MAX_SAMPLES];
} TurnSysIdData;

static TurnSysIdData turn_sysid_data;

typedef struct
{
    unsigned char trial_index;
    unsigned char stage;
    unsigned char settle_ticks;
    unsigned char best_found;
    unsigned int trial_ticks;
    int8_t initial_error_sign;
    float peak_overshoot_deg;
    float best_score;
    float saved_kp;
    float saved_ki;
    float saved_kd;
    float best_kp;
    float best_ki;
    float best_kd;
} TurnAutoTuneData;

static TurnAutoTuneData turn_autotune_data;

static void scheduleCommand(Robot *robot, Command *root);
static RobotCommandChain getChainApi(void);

static unsigned int roundPositiveToUInt(float value)
{
    if (value <= 0.0f)
    {
        return 0U;
    }

    return (unsigned int)(value + 0.5f);
}

static unsigned int absoluteInt16(int16_t value)
{
    return (unsigned int)((value < 0) ? -value : value);
}

static void setTurnSysIdDisplay(void)
{
    char line0[11] = "          ";
    char line1[11] = "          ";
    char line2[11] = "          ";
    unsigned int bw = turn_sysid_data.breakaway_pwm_percent;
    unsigned int u = turn_sysid_data.step_pwm_percent;
    unsigned int r = roundPositiveToUInt(turn_sysid_data.steady_rate_dps);
    unsigned int k10 = roundPositiveToUInt(turn_sysid_data.gain_dps_per_percent * 10.0f);
    unsigned int l = turn_sysid_data.delay_ticks;
    unsigned int t = turn_sysid_data.tau_ticks;
    unsigned int a = absoluteInt16(turn_sysid_data.peak_accel_raw) / 100U;

    if (r > 999U)
    {
        r = 999U;
    }
    if (k10 > 99U)
    {
        k10 = 99U;
    }
    if (l > 99U)
    {
        l = 99U;
    }
    if (t > 99U)
    {
        t = 99U;
    }
    if (a > 99U)
    {
        a = 99U;
    }

    line0[0] = 'B';
    line0[1] = 'W';
    line0[2] = (char)('0' + ((bw / 10U) % 10U));
    line0[3] = (char)('0' + (bw % 10U));
    line0[4] = ' ';
    line0[5] = 'U';
    line0[6] = (char)('0' + ((u / 10U) % 10U));
    line0[7] = (char)('0' + (u % 10U));

    line1[0] = 'R';
    line1[1] = (char)('0' + ((r / 100U) % 10U));
    line1[2] = (char)('0' + ((r / 10U) % 10U));
    line1[3] = (char)('0' + (r % 10U));
    line1[4] = ' ';
    line1[5] = 'K';
    line1[6] = (char)('0' + ((k10 / 10U) % 10U));
    line1[7] = (char)('0' + (k10 % 10U));

    line2[0] = 'L';
    line2[1] = (char)('0' + ((l / 10U) % 10U));
    line2[2] = (char)('0' + (l % 10U));
    line2[3] = 'T';
    line2[4] = (char)('0' + ((t / 10U) % 10U));
    line2[5] = (char)('0' + (t % 10U));
    line2[6] = 'A';
    line2[7] = (char)('0' + ((a / 10U) % 10U));
    line2[8] = (char)('0' + (a % 10U));

    strcpy(display_line[0], line0);
    strcpy(display_line[1], line1);
    strcpy(display_line[2], line2);
    display_changed = TRUE;
}

static void formatGainLine(const char *label, float gain, char *line_out)
{
    float gain_abs = (gain < 0.0f) ? -gain : gain;
    unsigned int scaled = roundPositiveToUInt(gain_abs * 100.0f);
    unsigned int whole = scaled / 100U;
    unsigned int frac = scaled % 100U;
    char sign = (gain < 0.0f) ? '-' : '+';

    if (whole > 9U)
    {
        whole = 9U;
    }

    strcpy(line_out, "          ");
    line_out[0] = label[0];
    line_out[1] = label[1];
    line_out[2] = sign;
    line_out[3] = (char)('0' + whole);
    line_out[4] = '.';
    line_out[5] = (char)('0' + ((frac / 10U) % 10U));
    line_out[6] = (char)('0' + (frac % 10U));
}

static void setTurnAutoTuneDisplay(void)
{
    char line_kp[11];
    char line_ki[11];
    char line_kd[11];

    formatGainLine("KP", turn_kp, line_kp);
    formatGainLine("KI", turn_ki, line_ki);
    formatGainLine("KD", turn_kd, line_kd);

    strcpy(display_line[0], line_kp);
    strcpy(display_line[1], line_ki);
    strcpy(display_line[2], line_kd);
    display_changed = TRUE;
}

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

static unsigned int pwmCountsFromPercent(unsigned int duty_percent)
{
    if (duty_percent >= 100U)
    {
        return WHEEL_PERIOD;
    }

    return (unsigned int)(((unsigned long)WHEEL_PERIOD * (unsigned long)duty_percent) / 100UL);
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

    strcpy(display_line[0], "          ");
    strcpy(display_line[1], "   CMD:   ");

    while ((name[index] != '\0') && (index < 10U))
    {
        line[index] = name[index];
        index++;
    }

    line[10] = '\0';
    strcpy(display_line[2], line);
    display_changed = TRUE;
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

static RobotCommandChain chainAndThenTurnSysId(void)
{
    if (active_chain.count >= CHAIN_MAX_COMMANDS)
    {
        return getChainApi();
    }

    Command_TurnSysId(&active_chain.commands[active_chain.count]);
    chainAppend(&active_chain.commands[active_chain.count]);
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
    chain_api.andThenForward = chainAndThenForward;
    chain_api.andThenReverse = chainAndThenReverse;
    chain_api.andThenWait = chainAndThenWait;
    chain_api.andThenSpinCW = chainAndThenSpinCW;
    chain_api.andThenSpinCCW = chainAndThenSpinCCW;
    chain_api.andThenTurnToAngle = chainAndThenTurnToAngle;
    chain_api.andThenDriveStraight = chainAndThenDriveStraight;
    chain_api.andThenReverseStraight = chainAndThenReverseStraight;
    chain_api.andThenTurnSysId = chainAndThenTurnSysId;
    chain_api.schedule = chainSchedule;

    return chain_api;
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
            setCurrentCommandDisplay("WAIT");
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
                setCurrentCommandDisplay("FORWARD");
                driveForward();
            }
            else if (command->type == CMD_REVERSE)
            {
                setCurrentCommandDisplay("REVERSE");
                driveReverse();
            }
            else
            {
                setCurrentCommandDisplay("SPIN");
                if (command->spin_direction == SPIN_COUNTERCLOCKWISE)
                {
                    driveSpinReverse();
                }
                else
                {
                    driveSpin();
                }
            }
        }

        if (Motors_IsSettled())
        {
            command->elapsed_ticks++;
        }
        if (command->elapsed_ticks >= command->duration_ticks)
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
        float cmd_rate_dps;
        float ff_percent;
        int feedback_pwm;
        int ff_pwm;
        const float feedback_full_scale_deg = 45.0f;
        const float integral_limit = 720.0f;
        unsigned int max_turn_pwm;
        int signed_pwm;
        int abs_signed_pwm;

        if (!command->started)
        {
            command->started = 1;
            command->heading_error_prev = 0.0f;
            command->heading_error_sum = 0.0f;
            command->pwm_counter = 0;
            setCurrentCommandDisplay("TURN ANGLE");
        }

        heading = getHeading();
        previous_error = command->heading_error_prev;
        error = wrapAngle(command->target_angle - heading);
        abs_error = absoluteFloat(error);
        derivative = error - previous_error;

        command->heading_error_prev = error;
        if (abs_error <= turn_integral_zone_deg)
        {
            command->heading_error_sum += error;
            if (command->heading_error_sum > integral_limit)
            {
                command->heading_error_sum = integral_limit;
            }
            if (command->heading_error_sum < -integral_limit)
            {
                command->heading_error_sum = -integral_limit;
            }
        }
        else
        {
            command->heading_error_sum = 0.0f;
        }

        feedback_control = (turn_kp * error) + (turn_ki * command->heading_error_sum) + (turn_kd * derivative);
        normalized_feedback = feedback_control / feedback_full_scale_deg;
        if (normalized_feedback > 1.0f)
        {
            normalized_feedback = 1.0f;
        }
        if (normalized_feedback < -1.0f)
        {
            normalized_feedback = -1.0f;
        }

        cmd_rate_dps = turn_ff_rate_kp_dps_per_deg * error;
        if (cmd_rate_dps > turn_ff_max_rate_dps)
        {
            cmd_rate_dps = turn_ff_max_rate_dps;
        }
        if (cmd_rate_dps < -turn_ff_max_rate_dps)
        {
            cmd_rate_dps = -turn_ff_max_rate_dps;
        }

        ff_percent = turn_ff_ks_percent + (turn_ff_kv_percent_per_dps * absoluteFloat(cmd_rate_dps));
        if (ff_percent > turn_max_pwm_percent)
        {
            ff_percent = turn_max_pwm_percent;
        }

        max_turn_pwm = pwmCountsFromPercentFloat(turn_max_pwm_percent);
        feedback_pwm = (int)((float)max_turn_pwm * normalized_feedback);
        ff_pwm = (int)pwmCountsFromPercentFloat(ff_percent);
        if (cmd_rate_dps < 0.0f)
        {
            ff_pwm = -ff_pwm;
        }

        signed_pwm = feedback_pwm + ff_pwm;
        if (signed_pwm > (int)max_turn_pwm)
        {
            signed_pwm = (int)max_turn_pwm;
        }
        if (signed_pwm < -(int)max_turn_pwm)
        {
            signed_pwm = -(int)max_turn_pwm;
        }

        abs_signed_pwm = (signed_pwm < 0) ? -signed_pwm : signed_pwm;
        if ((abs_signed_pwm == 0) && (abs_error > turn_angle_tolerance_deg))
        {
            signed_pwm = (error >= 0.0f) ? (int)pwmCountsFromPercent(MOTOR_MIN_DUTY_PERCENT)
                                          : -(int)pwmCountsFromPercent(MOTOR_MIN_DUTY_PERCENT);
        }

        applyMixedDrive(-signed_pwm, signed_pwm);

        command->elapsed_ticks++;
        if (abs_error <= turn_angle_tolerance_deg || command->elapsed_ticks >= secondsToTicks(5))
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
        unsigned int packet_now;
        unsigned int packet_delta;

        if (!command->started)
        {
            unsigned int packet_start;

            command->started = 1;
            command->heading_start = 0.0f;
            command->heading_error_prev = 0.0f;
            command->heading_error_sum = 0.0f;
            command->pwm_counter = 0;
            packet_start = IMU_GetPacketCount();
            command->left_duty = (unsigned char)(packet_start & 0xFFU);
            command->right_duty = (unsigned char)((packet_start >> 8) & 0xFFU);
            setCurrentCommandDisplay("STRAIGHT");
        }

        if (command->pwm_counter == 0)
        {
            getHeading();
            packet_now = IMU_GetPacketCount();
            packet_delta = packet_now - ((unsigned int)command->left_duty | ((unsigned int)command->right_duty << 8));

            if (packet_delta < 3U)
            {
                driveStop();
                return;
            }

            command->heading_start = getHeading();
            command->heading_error_prev = 0.0f;
            command->pwm_counter = 1;
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

        Motors_DriveForwardPWM(clampPwmCounts(left_pwm), clampPwmCounts(right_pwm));

        command->elapsed_ticks++;
        if (command->elapsed_ticks >= command->duration_ticks)
        {
            driveStop();
            command->finished = 1;
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
        unsigned int packet_now;
        unsigned int packet_delta;

        if (!command->started)
        {
            unsigned int packet_start;

            command->started = 1;
            command->heading_start = 0.0f;
            command->heading_error_prev = 0.0f;
            command->heading_error_sum = 0.0f;
            command->pwm_counter = 0;
            packet_start = IMU_GetPacketCount();
            command->left_duty = (unsigned char)(packet_start & 0xFFU);
            command->right_duty = (unsigned char)((packet_start >> 8) & 0xFFU);
            setCurrentCommandDisplay("REV STR");
        }

        if (command->pwm_counter == 0)
        {
            getHeading();
            packet_now = IMU_GetPacketCount();
            packet_delta = packet_now - ((unsigned int)command->left_duty | ((unsigned int)command->right_duty << 8));

            if (packet_delta < 3U)
            {
                driveStop();
                return;
            }

            command->heading_start = getHeading();
            command->heading_error_prev = 0.0f;
            command->pwm_counter = 1;
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

        Motors_DriveReversePWM(clampPwmCounts(left_pwm), clampPwmCounts(right_pwm));

        command->elapsed_ticks++;
        if (command->elapsed_ticks >= command->duration_ticks)
        {
            driveStop();
            command->finished = 1;
        }
        break;
    }

    case CMD_TURN_SYSID:
    {
        float heading;
        float delta_heading;
        float yaw_rate_dps;
        float steady_target_10;
        float steady_target_63;
        unsigned int ramp_pwm_percent;
        unsigned int step_pwm_percent;
        unsigned int ramp_start_pwm_percent;
        unsigned int ramp_step_pwm_percent;
        unsigned int ramp_max_pwm_percent;
        unsigned int index_start;
        unsigned int index;
        int16_t ax;
        int16_t ay;
        int16_t az;
        int16_t accel_peak;

        if (!command->started)
        {
            command->started = 1;
            command->pwm_counter = 0;
            command->elapsed_ticks = 0;
            command->left_duty = 0;
            command->right_duty = 0;
            command->heading_start = getHeading();
            command->heading_error_prev = command->heading_start;

            turn_sysid_data.sample_count = 0;
            turn_sysid_data.breakaway_pwm_percent = 0;
            turn_sysid_data.step_pwm_percent = 0;
            turn_sysid_data.delay_ticks = 0;
            turn_sysid_data.tau_ticks = 0;
            turn_sysid_data.steady_rate_dps = 0.0f;
            turn_sysid_data.max_rate_dps = 0.0f;
            turn_sysid_data.gain_dps_per_percent = 0.0f;
            turn_sysid_data.peak_accel_raw = 0;

            setCurrentCommandDisplay("TURN SYSID");
        }

        if (command->pwm_counter == 0U)
        {
            ramp_start_pwm_percent = roundPositiveToUInt(turn_sysid_ramp_start_pwm_percent);
            ramp_step_pwm_percent = roundPositiveToUInt(turn_sysid_ramp_step_pwm_percent);
            ramp_max_pwm_percent = roundPositiveToUInt(turn_sysid_ramp_max_pwm_percent);
            if (ramp_start_pwm_percent > 100U)
            {
                ramp_start_pwm_percent = 100U;
            }
            if (ramp_step_pwm_percent == 0U)
            {
                ramp_step_pwm_percent = 1U;
            }
            if (ramp_max_pwm_percent > 100U)
            {
                ramp_max_pwm_percent = 100U;
            }
            if (ramp_max_pwm_percent < ramp_start_pwm_percent)
            {
                ramp_max_pwm_percent = ramp_start_pwm_percent;
            }

            ramp_pwm_percent = ramp_start_pwm_percent + ((unsigned int)command->elapsed_ticks * ramp_step_pwm_percent);
            if (ramp_pwm_percent > ramp_max_pwm_percent)
            {
                ramp_pwm_percent = ramp_max_pwm_percent;
            }

            command->left_duty = (unsigned char)ramp_pwm_percent;
            Motors_DriveSpinPWM(pwmCountsFromPercent(ramp_pwm_percent), pwmCountsFromPercent(ramp_pwm_percent));

            heading = getHeading();
            delta_heading = wrapAngle(heading - command->heading_error_prev);
            command->heading_error_prev = heading;

            if (absoluteFloat(delta_heading) >= turn_sysid_motion_delta_deg)
            {
                if (command->right_duty < 255U)
                {
                    command->right_duty++;
                }
            }
            else
            {
                command->right_duty = 0;
            }

            command->elapsed_ticks++;
            if ((command->right_duty >= 2U) || (ramp_pwm_percent >= ramp_max_pwm_percent))
            {
                turn_sysid_data.breakaway_pwm_percent = (unsigned char)ramp_pwm_percent;
                command->pwm_counter = 1U;
                command->elapsed_ticks = 0;
                command->right_duty = 0;
                command->heading_error_prev = heading;
            }
            break;
        }

        if (command->pwm_counter == 1U)
        {
            step_pwm_percent = roundPositiveToUInt(turn_sysid_step_pwm_percent);
            if (step_pwm_percent > 100U)
            {
                step_pwm_percent = 100U;
            }
            turn_sysid_data.step_pwm_percent = (unsigned char)step_pwm_percent;

            Motors_DriveSpinPWM(pwmCountsFromPercent(step_pwm_percent), pwmCountsFromPercent(step_pwm_percent));

            heading = getHeading();
            delta_heading = wrapAngle(heading - command->heading_error_prev);
            command->heading_error_prev = heading;
            yaw_rate_dps = absoluteFloat(delta_heading) * (float)COMMAND_TICKS_PER_SECOND;

            if (turn_sysid_data.sample_count < TURN_SYSID_MAX_SAMPLES)
            {
                turn_sysid_data.rate_samples[turn_sysid_data.sample_count] = yaw_rate_dps;
                turn_sysid_data.sample_count++;
            }

            if (yaw_rate_dps > turn_sysid_data.max_rate_dps)
            {
                turn_sysid_data.max_rate_dps = yaw_rate_dps;
            }

            ax = IMU_GetAccelXRaw();
            ay = IMU_GetAccelYRaw();
            az = IMU_GetAccelZRaw();
            accel_peak = ax;
            if (absoluteInt16(ay) > absoluteInt16(accel_peak))
            {
                accel_peak = ay;
            }
            if (absoluteInt16(az) > absoluteInt16(accel_peak))
            {
                accel_peak = az;
            }
            if (absoluteInt16(accel_peak) > absoluteInt16(turn_sysid_data.peak_accel_raw))
            {
                turn_sysid_data.peak_accel_raw = accel_peak;
            }

            command->elapsed_ticks++;
            if (command->elapsed_ticks >= TURN_SYSID_STEP_TICKS)
            {
                command->pwm_counter = 2U;
            }
            break;
        }

        driveStop();

        if (turn_sysid_data.sample_count > 0U)
        {
            index_start = 0U;
            if (turn_sysid_data.sample_count > 5U)
            {
                index_start = (unsigned int)turn_sysid_data.sample_count - 5U;
            }

            turn_sysid_data.steady_rate_dps = 0.0f;
            for (index = index_start; index < turn_sysid_data.sample_count; index++)
            {
                turn_sysid_data.steady_rate_dps += turn_sysid_data.rate_samples[index];
            }
            turn_sysid_data.steady_rate_dps /= (float)(turn_sysid_data.sample_count - index_start);

            if (turn_sysid_data.step_pwm_percent > 0U)
            {
                turn_sysid_data.gain_dps_per_percent =
                    turn_sysid_data.steady_rate_dps / (float)turn_sysid_data.step_pwm_percent;
            }
            else
            {
                turn_sysid_data.gain_dps_per_percent = 0.0f;
            }

            steady_target_10 = 0.10f * turn_sysid_data.steady_rate_dps;
            steady_target_63 = 0.632f * turn_sysid_data.steady_rate_dps;
            turn_sysid_data.delay_ticks = turn_sysid_data.sample_count;
            turn_sysid_data.tau_ticks = turn_sysid_data.sample_count;

            for (index = 0U; index < turn_sysid_data.sample_count; index++)
            {
                if ((turn_sysid_data.delay_ticks == turn_sysid_data.sample_count) &&
                    (turn_sysid_data.rate_samples[index] >= steady_target_10))
                {
                    turn_sysid_data.delay_ticks = (unsigned char)index;
                }
                if ((turn_sysid_data.tau_ticks == turn_sysid_data.sample_count) &&
                    (turn_sysid_data.rate_samples[index] >= steady_target_63))
                {
                    turn_sysid_data.tau_ticks = (unsigned char)index;
                }
            }
        }

        setTurnSysIdDisplay();
        command->finished = 1;
        break;
    }

    case CMD_AUTO_TUNE:
    {
        float heading;
        float error;
        float abs_error;
        float derivative;
        float feedback_control;
        float normalized_feedback;
        float cmd_rate_dps;
        float ff_percent;
        float score;
        const float feedback_full_scale_deg = 45.0f;
        const float integral_limit = 720.0f;
        unsigned int max_turn_pwm;
        int feedback_pwm;
        int ff_pwm;
        int signed_pwm;
        int abs_signed_pwm;

        if (!command->started)
        {
            command->started = 1;
            command->heading_error_prev = 0.0f;
            command->heading_error_sum = 0.0f;
            command->pwm_counter = 0;
            setCurrentCommandDisplay("AUTO TUNE");

            turn_autotune_data.trial_index = 0U;
            turn_autotune_data.stage = 0U;
            turn_autotune_data.settle_ticks = 0U;
            turn_autotune_data.best_found = 0U;
            turn_autotune_data.trial_ticks = 0U;
            turn_autotune_data.initial_error_sign = 1;
            turn_autotune_data.peak_overshoot_deg = 0.0f;
            turn_autotune_data.best_score = 0.0f;
            turn_autotune_data.saved_kp = turn_kp;
            turn_autotune_data.saved_ki = turn_ki;
            turn_autotune_data.saved_kd = turn_kd;
            turn_autotune_data.best_kp = turn_kp;
            turn_autotune_data.best_ki = turn_ki;
            turn_autotune_data.best_kd = turn_kd;
        }

        if (turn_autotune_data.stage == 0U)
        {
            if (turn_autotune_data.trial_index >= TURN_AUTOTUNE_MAX_TRIALS)
            {
                driveStop();
                if (turn_autotune_data.best_found)
                {
                    turn_kp = turn_autotune_data.best_kp;
                    turn_ki = turn_autotune_data.best_ki;
                    turn_kd = turn_autotune_data.best_kd;
                }
                else
                {
                    turn_kp = turn_autotune_data.saved_kp;
                    turn_ki = turn_autotune_data.saved_ki;
                    turn_kd = turn_autotune_data.saved_kd;
                }

                setTurnAutoTuneDisplay();
                command->finished = 1;
                break;
            }

            turn_kp = 0.4f + (0.35f * (float)turn_autotune_data.trial_index);
            turn_kd = 0.05f + (0.08f * (float)turn_autotune_data.trial_index);
            if (turn_autotune_data.trial_index >= 3U)
            {
                turn_ki = 0.01f * (float)(turn_autotune_data.trial_index - 2U);
            }
            else
            {
                turn_ki = 0.0f;
            }

            heading = getHeading();
            if ((turn_autotune_data.trial_index & 0x01U) == 0U)
            {
                command->target_angle = wrapAngle(heading + TURN_AUTOTUNE_TARGET_DEG);
            }
            else
            {
                command->target_angle = wrapAngle(heading - TURN_AUTOTUNE_TARGET_DEG);
            }

            error = wrapAngle(command->target_angle - heading);
            turn_autotune_data.initial_error_sign = (error >= 0.0f) ? 1 : -1;
            turn_autotune_data.trial_ticks = 0U;
            turn_autotune_data.settle_ticks = 0U;
            turn_autotune_data.peak_overshoot_deg = 0.0f;
            command->heading_error_prev = error;
            command->heading_error_sum = 0.0f;
            turn_autotune_data.stage = 1U;
            break;
        }

        heading = getHeading();
        error = wrapAngle(command->target_angle - heading);
        abs_error = absoluteFloat(error);
        derivative = error - command->heading_error_prev;
        command->heading_error_prev = error;

        if (abs_error <= turn_integral_zone_deg)
        {
            command->heading_error_sum += error;
            if (command->heading_error_sum > integral_limit)
            {
                command->heading_error_sum = integral_limit;
            }
            if (command->heading_error_sum < -integral_limit)
            {
                command->heading_error_sum = -integral_limit;
            }
        }
        else
        {
            command->heading_error_sum = 0.0f;
        }

        feedback_control = (turn_kp * error) + (turn_ki * command->heading_error_sum) + (turn_kd * derivative);
        normalized_feedback = feedback_control / feedback_full_scale_deg;
        if (normalized_feedback > 1.0f)
        {
            normalized_feedback = 1.0f;
        }
        if (normalized_feedback < -1.0f)
        {
            normalized_feedback = -1.0f;
        }

        cmd_rate_dps = turn_ff_rate_kp_dps_per_deg * error;
        if (cmd_rate_dps > turn_ff_max_rate_dps)
        {
            cmd_rate_dps = turn_ff_max_rate_dps;
        }
        if (cmd_rate_dps < -turn_ff_max_rate_dps)
        {
            cmd_rate_dps = -turn_ff_max_rate_dps;
        }

        ff_percent = turn_ff_ks_percent + (turn_ff_kv_percent_per_dps * absoluteFloat(cmd_rate_dps));
        if (ff_percent > turn_max_pwm_percent)
        {
            ff_percent = turn_max_pwm_percent;
        }

        max_turn_pwm = pwmCountsFromPercentFloat(turn_max_pwm_percent);
        feedback_pwm = (int)((float)max_turn_pwm * normalized_feedback);
        ff_pwm = (int)pwmCountsFromPercentFloat(ff_percent);
        if (cmd_rate_dps < 0.0f)
        {
            ff_pwm = -ff_pwm;
        }

        signed_pwm = feedback_pwm + ff_pwm;
        if (signed_pwm > (int)max_turn_pwm)
        {
            signed_pwm = (int)max_turn_pwm;
        }
        if (signed_pwm < -(int)max_turn_pwm)
        {
            signed_pwm = -(int)max_turn_pwm;
        }

        abs_signed_pwm = (signed_pwm < 0) ? -signed_pwm : signed_pwm;
        if ((abs_signed_pwm == 0) && (abs_error > turn_angle_tolerance_deg))
        {
            signed_pwm = (error >= 0.0f) ? (int)pwmCountsFromPercent(MOTOR_MIN_DUTY_PERCENT)
                                          : -(int)pwmCountsFromPercent(MOTOR_MIN_DUTY_PERCENT);
        }

        applyMixedDrive(-signed_pwm, signed_pwm);

        if (((float)turn_autotune_data.initial_error_sign * error) < 0.0f)
        {
            if (abs_error > turn_autotune_data.peak_overshoot_deg)
            {
                turn_autotune_data.peak_overshoot_deg = abs_error;
            }
        }

        if (abs_error <= turn_angle_tolerance_deg)
        {
            if (turn_autotune_data.settle_ticks < 255U)
            {
                turn_autotune_data.settle_ticks++;
            }
        }
        else
        {
            turn_autotune_data.settle_ticks = 0U;
        }

        if (turn_autotune_data.trial_ticks < 65535U)
        {
            turn_autotune_data.trial_ticks++;
        }

        if (turn_autotune_data.settle_ticks >= TURN_AUTOTUNE_SETTLE_TICKS)
        {
            score = (float)turn_autotune_data.trial_ticks +
                    (TURN_AUTOTUNE_OVERSHOOT_WEIGHT * turn_autotune_data.peak_overshoot_deg);
            if ((!turn_autotune_data.best_found) || (score < turn_autotune_data.best_score))
            {
                turn_autotune_data.best_found = 1U;
                turn_autotune_data.best_score = score;
                turn_autotune_data.best_kp = turn_kp;
                turn_autotune_data.best_ki = turn_ki;
                turn_autotune_data.best_kd = turn_kd;
            }

            driveStop();
            turn_autotune_data.trial_index++;
            turn_autotune_data.stage = 0U;
            break;
        }

        if (turn_autotune_data.trial_ticks >= TURN_AUTOTUNE_TIMEOUT_TICKS)
        {
            driveStop();
            turn_autotune_data.trial_index++;
            turn_autotune_data.stage = 0U;
            break;
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
            setCurrentCommandDisplay("PARALLEL");
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

static void commandDisplayName(Event event)
{
    (void)event;
    setCurrentCommandDisplay("IDLE");
}

static void cycleShapeRight(Robot *robot)
{
    switch (robot->event)
    {
    case NONE:
        setRobotShape(robot, TRIANGLE);
        break;
    case TRIANGLE:
        setRobotShape(robot, CIRCLE);
        break;
    case CIRCLE:
        setRobotShape(robot, FIGURE_8);
        break;
    case FIGURE_8:
    default:
        setRobotShape(robot, NONE);
        break;
    }
}

static void cycleShapeLeft(Robot *robot)
{
    switch (robot->event)
    {
    case TRIANGLE:
        setRobotShape(robot, NONE);
        break;
    case NONE:
        setRobotShape(robot, FIGURE_8);
        break;
    case CIRCLE:
        setRobotShape(robot, TRIANGLE);
        break;
    case FIGURE_8:
    default:
        setRobotShape(robot, CIRCLE);
        break;
    }
}

static void switchRightProcess(Robot *robot)
{
    if ((switch1_lock == 1) && (sw1_position == 0))
    {
        if (!(P4IN & SW1))
        {
            sw1_position = 1;
            switch1_lock = 0;
            count_debounce_sw1 = 0;
            cycleShapeLeft(robot);
        }
    }

    if (count_debounce_sw1 <= 5)
    {
        count_debounce_sw1++;
    }
    else
    {
        switch1_lock = 1;
        if (P4IN & SW1)
        {
            sw1_position = 0;
        }
    }
}

static void switchLeftProcess(Robot *robot)
{
    if ((switch2_lock == 1) && (sw2_position == 0))
    {
        if (!(P2IN & SW2))
        {
            sw2_position = 1;
            switch2_lock = 0;
            count_debounce_sw2 = 0;
            cycleShapeRight(robot);
        }
    }

    if (count_debounce_sw2 <= 5)
    {
        count_debounce_sw2++;
    }
    else
    {
        switch2_lock = 1;
        if (P2IN & SW2)
        {
            sw2_position = 0;
        }
    }
}

static void switchProcess(Robot *robot)
{
    switchLeftProcess(robot);
    switchRightProcess(robot);
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
}

void Command_TurnSysId(Command *command)
{
    if (command == 0)
    {
        return;
    }

    command->type = CMD_TURN_SYSID;
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
}

void Command_AutoTune(Command *command)
{
    if (command == 0)
    {
        return;
    }

    command->type = CMD_AUTO_TUNE;
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

    for (index = 0; index < command->child_count; index++)
    {
        command->children[index] = children[index];
    }
}

void Robot_RunSequentialGroup(Robot *robot, Command **commands, unsigned char count)
{
    static Command root_sequence;

    Command_Sequence(&root_sequence, commands, count);
    scheduleCommand(robot, &root_sequence);
}

void Robot_RunParallelGroup(Robot *robot, Command **commands, unsigned char count)
{
    static Command root_parallel;

    Command_Parallel(&root_parallel, commands, count);
    scheduleCommand(robot, &root_parallel);
}

void initRobot(Robot *robot)
{
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
    static Command triangle_steps[6];
    static Command *triangle_children[6];
    static Command triangle_sequence;

    static Command circle_spin;

    static Command figure8_forward;
    static Command figure8_spin;
    static Command *figure8_parallel_children[2];
    static Command figure8_parallel;

    if (robot == 0)
    {
        return;
    }

    robot->event = shape;
    resetRobot(robot);

    if (shape == TRIANGLE)
    {
        Command_Forward(&triangle_steps[0], 1);
        Command_Spin(&triangle_steps[1], 1, SPIN_CLOCKWISE);
        Command_Forward(&triangle_steps[2], 1);
        Command_Spin(&triangle_steps[3], 1, SPIN_CLOCKWISE);
        Command_Forward(&triangle_steps[4], 1);
        Command_Spin(&triangle_steps[5], 1, SPIN_CLOCKWISE);

        triangle_children[0] = &triangle_steps[0];
        triangle_children[1] = &triangle_steps[1];
        triangle_children[2] = &triangle_steps[2];
        triangle_children[3] = &triangle_steps[3];
        triangle_children[4] = &triangle_steps[4];
        triangle_children[5] = &triangle_steps[5];

        Command_Sequence(&triangle_sequence, triangle_children, 6);
        scheduleCommand(robot, &triangle_sequence);
    }
    else if (shape == CIRCLE)
    {
        Command_Spin(&circle_spin, 4, SPIN_CLOCKWISE);
        scheduleCommand(robot, &circle_spin);
    }
    else if (shape == FIGURE_8)
    {
        Command_Forward(&figure8_forward, 2);
        Command_Spin(&figure8_spin, 2, SPIN_COUNTERCLOCKWISE);

        figure8_parallel_children[0] = &figure8_forward;
        figure8_parallel_children[1] = &figure8_spin;

        Command_Parallel(&figure8_parallel, figure8_parallel_children, 2);
        scheduleCommand(robot, &figure8_parallel);
    }

    commandDisplayName(shape);
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
        while ((last_command_tick < current_tick) && (robot->active_command != 0))
        {
            commandTick(robot->active_command);
            last_command_tick++;
        }

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

void forward(int time)
{
    static Command forward_command;

    if (active_robot == 0)
    {
        return;
    }

    Command_Forward(&forward_command, time);
    scheduleCommand(active_robot, &forward_command);
}

void reverse(int time)
{
    static Command reverse_command;

    if (active_robot == 0)
    {
        return;
    }

    Command_Reverse(&reverse_command, time);
    scheduleCommand(active_robot, &reverse_command);
}

void spin(int time)
{
    static Command spin_command;

    if (active_robot == 0)
    {
        return;
    }

    Command_Spin(&spin_command, time, SPIN_CLOCKWISE);
    scheduleCommand(active_robot, &spin_command);
}

void turnToAngle(float target_angle_degrees)
{
    static Command turn_command;

    if (active_robot == 0)
    {
        return;
    }

    Command_TurnToAngle(&turn_command, target_angle_degrees);
    scheduleCommand(active_robot, &turn_command);
}

void driveStraight(int time_seconds)
{
    static Command straight_command;

    if (active_robot == 0)
    {
        return;
    }

    Command_DriveStraight(&straight_command, time_seconds);
    scheduleCommand(active_robot, &straight_command);
}

void reverseStraight(int time_seconds)
{
    static Command rev_straight_command;

    if (active_robot == 0)
    {
        return;
    }

    Command_ReverseStraight(&rev_straight_command, time_seconds);
    scheduleCommand(active_robot, &rev_straight_command);
}

void turnSysId(void)
{
    static Command turn_sysid_command;

    if (active_robot == 0)
    {
        return;
    }

    Command_TurnSysId(&turn_sysid_command);
    scheduleCommand(active_robot, &turn_sysid_command);
}

void autoTuneTurnPID(void)
{
    static Command autotune_command;

    if (active_robot == 0)
    {
        return;
    }

    Command_AutoTune(&autotune_command);
    scheduleCommand(active_robot, &autotune_command);
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

RobotCommandChain chainTurnSysId(void)
{
    chainReset();
    return chainAndThenTurnSysId();
}
