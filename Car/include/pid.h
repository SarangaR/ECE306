#ifndef PID_H
#define PID_H

#include <stdint.h>

#define GAIN_SCALE_SHIFT    10      /* 2^10 = 1024 */

typedef struct PIDController PIDController;

struct PIDController {
    // Gains are prescaled by 1024
    int32_t kp;
    int32_t ki;
    int32_t kd;

    int32_t integralMax;
    int32_t integralMin;
    int32_t outputMax;
    int32_t outputMin;

    int32_t integral;
    int32_t prevError;

    int32_t (*calculate)(PIDController *self, int32_t error);
    void    (*reset)    (PIDController *self);
};

void PID_Init(PIDController *pid,
              int32_t kp,          int32_t ki,          int32_t kd,
              int32_t integralMax, int32_t integralMin,
              int32_t outputMax,   int32_t outputMin);

#endif // PID_H
