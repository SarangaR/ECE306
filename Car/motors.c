//------------------------------------------------------------------------------
//
//  Description: This file contains the definitions for the functions that control the motors.
//
//  Saranga Rajagopalan
//  Feb 2026
//  Built with Code Composer Version: CCS12.4.0.00007_win64
//------------------------------------------------------------------------------

#include "msp430.h"
#include "include/functions.h"
#include "include/ports.h"
#include "include/motors.h"
#include "include/robot.h"
#include "include/macros.h"

typedef enum
{
    MOTOR_MODE_STOP = 0,
    MOTOR_MODE_FORWARD,
    MOTOR_MODE_REVERSE,
    MOTOR_MODE_SPIN,
    MOTOR_MODE_SPIN_REVERSE
} MotorDriveMode;

static MotorDriveMode requested_mode = MOTOR_MODE_STOP;
static MotorDriveMode applied_mode = MOTOR_MODE_STOP;
static MotorDriveMode last_directional_mode = MOTOR_MODE_STOP;
static unsigned int direction_change_delay_counter = 0;
static unsigned int g_pending_left = 0;
static unsigned int g_pending_right = 0;
static unsigned char stop_history_armed = 0U;
static unsigned long stop_history_start_tick = 0UL;

#define SHOOT_THROUGH_FLASH_TICKS (5UL) // 5 * 20ms = 100ms at 50Hz tick
#define SHOOT_THROUGH_TEST_DURATION_TICKS (50UL) // 50 * 20ms = 1s simulated fault window

static unsigned char shoot_through_test_active = 0U;
static unsigned long shoot_through_test_start_tick = 0UL;

void Motors_TestShootThroughIndicator(void)
{
    shoot_through_test_active = 1U;
    shoot_through_test_start_tick = one_second_timer;
}

static void serviceShootThroughIndicator(void)
{
    static unsigned long last_flash_tick = 0UL;
    static unsigned char red_led_on = 0U;
    unsigned char left_shoot_through;
    unsigned char right_shoot_through;
    unsigned char any_shoot_through;
    unsigned long now;

    now = one_second_timer;

    left_shoot_through = ((LEFT_FORWARD_SPEED > 0U) && (LEFT_REVERSE_SPEED > 0U)) ? 1U : 0U;
    right_shoot_through = ((RIGHT_FORWARD_SPEED > 0U) && (RIGHT_REVERSE_SPEED > 0U)) ? 1U : 0U;
    any_shoot_through = (unsigned char)(left_shoot_through || right_shoot_through);

    if (shoot_through_test_active)
    {
        if ((now - shoot_through_test_start_tick) < SHOOT_THROUGH_TEST_DURATION_TICKS)
        {
            any_shoot_through = 1U;
        }
        else
        {
            shoot_through_test_active = 0U;
        }
    }

    if (!any_shoot_through)
    {
        P1OUT &= ~RED_LED;
        red_led_on = 0U;
        last_flash_tick = one_second_timer;
        return;
    }

    if ((now - last_flash_tick) >= SHOOT_THROUGH_FLASH_TICKS)
    {
        last_flash_tick = now;
        if (red_led_on)
        {
            P1OUT &= ~RED_LED;
            red_led_on = 0U;
        }
        else
        {
            P1OUT |= RED_LED;
            red_led_on = 1U;
        }
    }
}

static unsigned int pwmCountsFromPercent(unsigned int duty_percent)
{
    if (duty_percent >= 100U)
    {
        return WHEEL_PERIOD;
    }

    return (unsigned int)(((unsigned long)WHEEL_PERIOD * (unsigned long)duty_percent) / 100UL);
}

static unsigned int clampPWM(unsigned int duty)
{
    unsigned int min_duty_counts = pwmCountsFromPercent(MOTOR_MIN_DUTY_PERCENT);

    if ((duty > 0) && (duty < min_duty_counts))
    {
        return 0;
    }
    if (duty > WHEEL_PERIOD)
    {
        return WHEEL_PERIOD;
    }
    return duty;
}

static void applyMotorMode(MotorDriveMode mode)
{
    LEFT_FORWARD_SPEED = 0;
    RIGHT_FORWARD_SPEED = 0;
    LEFT_REVERSE_SPEED = 0;
    RIGHT_REVERSE_SPEED = 0;

    if (mode == MOTOR_MODE_FORWARD)
    {
        LEFT_FORWARD_SPEED = g_pending_left;
        RIGHT_FORWARD_SPEED = g_pending_right;
    }
    else if (mode == MOTOR_MODE_REVERSE)
    {
        LEFT_REVERSE_SPEED = g_pending_left;
        RIGHT_REVERSE_SPEED = g_pending_right;
    }
    else if (mode == MOTOR_MODE_SPIN)
    {
        LEFT_FORWARD_SPEED = g_pending_left;
        RIGHT_REVERSE_SPEED = g_pending_right;
    }
    else if (mode == MOTOR_MODE_SPIN_REVERSE)
    {
        LEFT_REVERSE_SPEED = g_pending_left;
        RIGHT_FORWARD_SPEED = g_pending_right;
    }

    if (mode != MOTOR_MODE_STOP)
    {
        last_directional_mode = mode;
    }

    applied_mode = mode;
}

static int leftWheelDirectionForMode(MotorDriveMode mode)
{
    if (mode == MOTOR_MODE_FORWARD)
    {
        return 1;
    }
    if (mode == MOTOR_MODE_REVERSE)
    {
        return -1;
    }
    if (mode == MOTOR_MODE_SPIN)
    {
        return 1;
    }
    if (mode == MOTOR_MODE_SPIN_REVERSE)
    {
        return -1;
    }
    return 0;
}

static int rightWheelDirectionForMode(MotorDriveMode mode)
{
    if (mode == MOTOR_MODE_FORWARD)
    {
        return 1;
    }
    if (mode == MOTOR_MODE_REVERSE)
    {
        return -1;
    }
    if (mode == MOTOR_MODE_SPIN)
    {
        return -1;
    }
    if (mode == MOTOR_MODE_SPIN_REVERSE)
    {
        return 1;
    }
    return 0;
}

static int requiresDirectionChangeDeadtime(MotorDriveMode from_mode, MotorDriveMode to_mode)
{
    int from_left = leftWheelDirectionForMode(from_mode);
    int from_right = rightWheelDirectionForMode(from_mode);
    int to_left = leftWheelDirectionForMode(to_mode);
    int to_right = rightWheelDirectionForMode(to_mode);

    if (((from_left != 0) && (to_left != 0) && (from_left != to_left)) ||
        ((from_right != 0) && (to_right != 0) && (from_right != to_right)))
    {
        return 1;
    }

    return 0;
}

void allMotorsOff(void)
{
    g_pending_left = 0;
    g_pending_right = 0;
    requested_mode = MOTOR_MODE_STOP;
    direction_change_delay_counter = 0;
    applyMotorMode(MOTOR_MODE_STOP);
}

void driveStop(void)
{
    requested_mode = MOTOR_MODE_STOP;
}

void driveForward(void)
{
    g_pending_left = WHEEL_PERIOD;
    g_pending_right = WHEEL_PERIOD;
    requested_mode = MOTOR_MODE_FORWARD;
}

void leftMotorOff(void)
{
    requested_mode = MOTOR_MODE_STOP;
}

void rightMotorOff(void)
{
    requested_mode = MOTOR_MODE_STOP;
}

void leftMotorOn(void)
{
    g_pending_left = WHEEL_PERIOD;
    g_pending_right = WHEEL_PERIOD;
    requested_mode = MOTOR_MODE_FORWARD;
}

void rightMotorOn(void)
{
    g_pending_left = WHEEL_PERIOD;
    g_pending_right = WHEEL_PERIOD;
    requested_mode = MOTOR_MODE_FORWARD;
}

void leftMotorReverse(void)
{
    g_pending_left = WHEEL_PERIOD;
    g_pending_right = WHEEL_PERIOD;
    requested_mode = MOTOR_MODE_REVERSE;
}

void rightMotorReverse(void)
{
    g_pending_left = WHEEL_PERIOD;
    g_pending_right = WHEEL_PERIOD;
    requested_mode = MOTOR_MODE_REVERSE;
}

void driveReverse(void)
{
    g_pending_left = WHEEL_PERIOD;
    g_pending_right = WHEEL_PERIOD;
    requested_mode = MOTOR_MODE_REVERSE;
}

void driveSpin(void)
{
    g_pending_left = WHEEL_PERIOD;
    g_pending_right = WHEEL_PERIOD;
    requested_mode = MOTOR_MODE_SPIN;
}

void driveSpinReverse(void)
{
    g_pending_left = WHEEL_PERIOD;
    g_pending_right = WHEEL_PERIOD;
    requested_mode = MOTOR_MODE_SPIN_REVERSE;
}

void Motors_Service(void)
{
    serviceShootThroughIndicator();

    if ((requested_mode == MOTOR_MODE_STOP) && (applied_mode == MOTOR_MODE_STOP))
    {
        if (!stop_history_armed)
        {
            stop_history_armed = 1U;
            stop_history_start_tick = one_second_timer;
        }
        else if ((one_second_timer - stop_history_start_tick) >= COMMAND_TICKS_FROM_MS(100U))
        {
            last_directional_mode = MOTOR_MODE_STOP;
        }
    }
    else
    {
        stop_history_armed = 0U;
    }

    if (direction_change_delay_counter > 0)
    {
        applyMotorMode(MOTOR_MODE_STOP);
        direction_change_delay_counter--;

        if (direction_change_delay_counter == 0)
        {
            applyMotorMode(requested_mode);
        }
        return;
    }

    if (applied_mode == requested_mode)
    {
        return;
    }

    if (requested_mode == MOTOR_MODE_STOP)
    {
        applyMotorMode(MOTOR_MODE_STOP);
        return;
    }

    if (requiresDirectionChangeDeadtime(last_directional_mode, requested_mode))
    {
        applyMotorMode(MOTOR_MODE_STOP);
        direction_change_delay_counter = MOTOR_DIRECTION_CHANGE_DELAY_CYCLES;
        return;
    }

    applyMotorMode(requested_mode);
}

unsigned char Motors_IsSettled(void)
{
    if (direction_change_delay_counter > 0)
    {
        return 0;
    }

    if (applied_mode != requested_mode)
    {
        return 0;
    }

    return 1;
}

void Motors_SetPWM(unsigned int left_fwd, unsigned int right_fwd,
                   unsigned int left_rev, unsigned int right_rev)
{
    LEFT_FORWARD_SPEED = clampPWM(left_fwd);
    RIGHT_FORWARD_SPEED = clampPWM(right_fwd);
    LEFT_REVERSE_SPEED = clampPWM(left_rev);
    RIGHT_REVERSE_SPEED = clampPWM(right_rev);
}

void Motors_DriveForwardPWM(unsigned int left_pwm, unsigned int right_pwm)
{
    g_pending_left = clampPWM(left_pwm);
    g_pending_right = clampPWM(right_pwm);
    requested_mode = MOTOR_MODE_FORWARD;

    if (requiresDirectionChangeDeadtime(last_directional_mode, MOTOR_MODE_FORWARD))
    {
        if (direction_change_delay_counter == 0)
        {
            applyMotorMode(MOTOR_MODE_STOP);
            direction_change_delay_counter = MOTOR_DIRECTION_CHANGE_DELAY_CYCLES;
        }
        return;
    }

    applyMotorMode(MOTOR_MODE_FORWARD);
}

void Motors_DriveReversePWM(unsigned int left_pwm, unsigned int right_pwm)
{
    g_pending_left = clampPWM(left_pwm);
    g_pending_right = clampPWM(right_pwm);
    requested_mode = MOTOR_MODE_REVERSE;

    if (requiresDirectionChangeDeadtime(last_directional_mode, MOTOR_MODE_REVERSE))
    {
        if (direction_change_delay_counter == 0)
        {
            applyMotorMode(MOTOR_MODE_STOP);
            direction_change_delay_counter = MOTOR_DIRECTION_CHANGE_DELAY_CYCLES;
        }
        return;
    }

    applyMotorMode(MOTOR_MODE_REVERSE);
}

void Motors_DriveSpinPWM(unsigned int left_pwm, unsigned int right_pwm)
{
    g_pending_left = clampPWM(left_pwm);
    g_pending_right = clampPWM(right_pwm);
    requested_mode = MOTOR_MODE_SPIN;

    if (requiresDirectionChangeDeadtime(last_directional_mode, MOTOR_MODE_SPIN))
    {
        if (direction_change_delay_counter == 0)
        {
            applyMotorMode(MOTOR_MODE_STOP);
            direction_change_delay_counter = MOTOR_DIRECTION_CHANGE_DELAY_CYCLES;
        }
        return;
    }

    applyMotorMode(MOTOR_MODE_SPIN);
}

void Motors_DriveSpinReversePWM(unsigned int left_pwm, unsigned int right_pwm)
{
    g_pending_left = clampPWM(left_pwm);
    g_pending_right = clampPWM(right_pwm);
    requested_mode = MOTOR_MODE_SPIN_REVERSE;

    if (requiresDirectionChangeDeadtime(last_directional_mode, MOTOR_MODE_SPIN_REVERSE))
    {
        if (direction_change_delay_counter == 0)
        {
            applyMotorMode(MOTOR_MODE_STOP);
            direction_change_delay_counter = MOTOR_DIRECTION_CHANGE_DELAY_CYCLES;
        }
        return;
    }

    applyMotorMode(MOTOR_MODE_SPIN_REVERSE);
}

void Motors_DriveForwardDifferential(unsigned char left_on, unsigned char right_on)
{
    Motors_DriveForwardPWM(left_on ? WHEEL_PERIOD : 0, right_on ? WHEEL_PERIOD : 0);
}

void Motors_DriveReverseDifferential(unsigned char left_on, unsigned char right_on)
{
    Motors_DriveReversePWM(left_on ? WHEEL_PERIOD : 0, right_on ? WHEEL_PERIOD : 0);
}

void Motors_DriveSpinDifferential(unsigned char left_on, unsigned char right_on)
{
    Motors_DriveSpinPWM(left_on ? WHEEL_PERIOD : 0, right_on ? WHEEL_PERIOD : 0);
}

void Motors_DriveSpinReverseDifferential(unsigned char left_on, unsigned char right_on)
{
    Motors_DriveSpinReversePWM(left_on ? WHEEL_PERIOD : 0, right_on ? WHEEL_PERIOD : 0);
}

void Motors_PWM_Test(void)
{
    static unsigned char test_phase = 0;
    static unsigned char test_duty_percent = 0;
    static unsigned long last_tick = 0;
    unsigned long tick_interval;
    unsigned int duty_counts;
    unsigned long now;

    tick_interval = (unsigned long)COMMAND_TICKS_PER_SECOND / 5UL;
    if (tick_interval == 0UL)
    {
        tick_interval = 1UL;
    }

    now = one_second_timer;
    if ((now - last_tick) < tick_interval)
    {
        return;
    }
    last_tick = now;

    switch (test_phase)
    {
    case 0:
        LEFT_FORWARD_SPEED = 0;
        RIGHT_FORWARD_SPEED = 0;
        LEFT_REVERSE_SPEED = 0;
        RIGHT_REVERSE_SPEED = 0;

        test_duty_percent += MOTOR_PWM_TEST_STEP_PERCENT;
        if (test_duty_percent > 100U)
        {
            test_duty_percent = 100U;
        }
        duty_counts = pwmCountsFromPercent(test_duty_percent);
        LEFT_FORWARD_SPEED = duty_counts;
        if (test_duty_percent >= 100U)
        {
            test_duty_percent = 0;
            LEFT_FORWARD_SPEED = 0;
            test_phase = 1;
        }
        break;

    case 1:
        LEFT_FORWARD_SPEED = 0;
        RIGHT_FORWARD_SPEED = 0;
        LEFT_REVERSE_SPEED = 0;
        RIGHT_REVERSE_SPEED = 0;

        test_duty_percent += MOTOR_PWM_TEST_STEP_PERCENT;
        if (test_duty_percent > 100U)
        {
            test_duty_percent = 100U;
        }
        duty_counts = pwmCountsFromPercent(test_duty_percent);
        RIGHT_FORWARD_SPEED = duty_counts;
        if (test_duty_percent >= 100U)
        {
            test_duty_percent = 0;
            RIGHT_FORWARD_SPEED = 0;
            test_phase = 2;
        }
        break;

    case 2:
        LEFT_FORWARD_SPEED = 0;
        RIGHT_FORWARD_SPEED = 0;
        LEFT_REVERSE_SPEED = 0;
        RIGHT_REVERSE_SPEED = 0;

        test_duty_percent += MOTOR_PWM_TEST_STEP_PERCENT;
        if (test_duty_percent > 100U)
        {
            test_duty_percent = 100U;
        }
        duty_counts = pwmCountsFromPercent(test_duty_percent);
        LEFT_REVERSE_SPEED = duty_counts;
        if (test_duty_percent >= 100U)
        {
            test_duty_percent = 0;
            LEFT_REVERSE_SPEED = 0;
            test_phase = 3;
        }
        break;

    case 3:
        LEFT_FORWARD_SPEED = 0;
        RIGHT_FORWARD_SPEED = 0;
        LEFT_REVERSE_SPEED = 0;
        RIGHT_REVERSE_SPEED = 0;

        test_duty_percent += MOTOR_PWM_TEST_STEP_PERCENT;
        if (test_duty_percent > 100U)
        {
            test_duty_percent = 100U;
        }
        duty_counts = pwmCountsFromPercent(test_duty_percent);
        RIGHT_REVERSE_SPEED = duty_counts;
        if (test_duty_percent >= 100U)
        {
            test_duty_percent = 0;
            RIGHT_REVERSE_SPEED = 0;
            test_phase = 4;
        }
        break;

    case 4:
    default:
        LEFT_FORWARD_SPEED = 0;
        RIGHT_FORWARD_SPEED = 0;
        LEFT_REVERSE_SPEED = 0;
        RIGHT_REVERSE_SPEED = 0;
        break;
    }
}
