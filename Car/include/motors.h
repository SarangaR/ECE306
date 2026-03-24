//------------------------------------------------------------------------------
//
//  Description: This file contains the declarations for the functions that control the motors.
//
//  Saranga Rajagopalan
//  Feb 2026
//  Built with Code Composer Version: CCS12.4.0.00007_win64
//------------------------------------------------------------------------------

#define MOTOR_MIN_DUTY_PERCENT (10U)
#define MOTOR_PWM_TEST_STEP_PERCENT (1U)

void allMotorsOff(void);
void allMotorsOn(void);

void leftMotorOff(void);
void rightMotorOff(void);

void leftMotorOn(void);
void rightMotorOn(void);

void leftMotorReverse(void);
void rightMotorReverse(void);

void driveStop(void);
void driveForward(void);
void driveReverse(void);
void driveSpin(void);
void driveSpinReverse(void);
void Motors_Service(void);
unsigned char Motors_IsSettled(void);
void Motors_SetPWM(unsigned int left_fwd, unsigned int right_fwd, unsigned int left_rev, unsigned int right_rev);
void Motors_DriveForwardPWM(unsigned int left_pwm, unsigned int right_pwm);
void Motors_DriveReversePWM(unsigned int left_pwm, unsigned int right_pwm);
void Motors_DriveSpinPWM(unsigned int left_pwm, unsigned int right_pwm);
void Motors_DriveSpinReversePWM(unsigned int left_pwm, unsigned int right_pwm);
void Motors_DriveForwardDifferential(unsigned char left_on, unsigned char right_on);
void Motors_DriveReverseDifferential(unsigned char left_on, unsigned char right_on);
void Motors_DriveSpinDifferential(unsigned char left_on, unsigned char right_on);
void Motors_DriveSpinReverseDifferential(unsigned char left_on, unsigned char right_on);
void Motors_PWM_Test(void);
void Motors_TestShootThroughIndicator(void);
