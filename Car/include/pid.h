#ifndef PID_H
#define PID_H

#include <stdint.h>

#define GAIN_SCALE_SHIFT    10      /* 2^10 = 1024 */

typedef struct PIDController PIDController;

struct PIDController {
    // Gains are prescaled by 1024
    int kp;
    int ki;
    int kd;
    int integralMax;
    int integralMin;
    int outputMax;
    int outputMin;
    int integral;
    int prevError;
    int tolerance;
    int error;
    int (*calculate)(PIDController *self, int error);
    void    (*reset)    (PIDController *self);
    int (*isDone)(PIDController *self);
};

void PID_Init(PIDController *pid,
              int kp,          int ki,          int kd,
              int integralMax, int integralMin,
              int outputMax,   int outputMin, float tolerance);

#endif // PID_H
