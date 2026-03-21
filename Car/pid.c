#include "include/pid.h"

static int32_t PID_Calculate(PIDController *self, int32_t error)
{
    self->integral += error;
    if (self->integral > self->integralMax) self->integral = self->integralMax;
    if (self->integral < self->integralMin) self->integral = self->integralMin;

    int32_t derivative  = error - self->prevError;
    self->prevError     = error;

    int32_t output = (self->kp * error
                    + self->ki * self->integral
                    + self->kd * derivative) >> GAIN_SCALE_SHIFT;

    if (output > self->outputMax) output = self->outputMax;
    if (output < self->outputMin) output = self->outputMin;

    return output;
}

static void PID_Reset(PIDController *self)
{
    self->integral  = 0;
    self->prevError = 0;
}

/* ---------------------------------------------------------------
 * PID_Init  (public – declared in pid.h)
 * --------------------------------------------------------------- */
void PID_Init(PIDController *pid,
              int32_t kp,          int32_t ki,          int32_t kd,
              int32_t integralMax, int32_t integralMin,
              int32_t outputMax,   int32_t outputMin)
{
    pid->kp          = kp;
    pid->ki          = ki;
    pid->kd          = kd;
    pid->integralMax = integralMax;
    pid->integralMin = integralMin;
    pid->outputMax   = outputMax;
    pid->outputMin   = outputMin;
    pid->integral    = 0;
    pid->prevError   = 0;

    /* Bind function pointers to the static implementations above */
    pid->calculate = PID_Calculate;
    pid->reset     = PID_Reset;
}