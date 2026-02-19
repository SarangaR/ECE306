//------------------------------------------------------------------------------
//
//  Description: This file contains the declarations for the functions that control the robot's behavior as a whole.
//
//  Saranga Rajagopalan
//  Feb 2026
//  Built with Code Composer Version: CCS12.4.0.00007_win64
//------------------------------------------------------------------------------

#ifndef ROBOT_H
#define ROBOT_H

#include "msp430.h"
#include <string.h>
#include "Include\functions.h"
#include "Include\ports.h"
#include "Include\robot.h"
#include "Include\macros.h"
#include "Include\motors.h"

#define WAITING2START                       500

#define WHEEL_COUNT_TIME                    10

// Circle timings
#define CIRCLE_DISTANCE                     50

// Figure8 timings
#define FIGURE8_CIRCLE_1_DISTANCE           62
#define FIGURE8_CIRCLE_2_DISTANCE           50
#define FIGURE8_CIRCLE_3_DISTANCE           50
#define FIGURE8_CIRCLE_4_DISTANCE           50

// Triangle timings
#define TRIANGLE_TURN_DISTANCE              9
#define TRIANGLE_STRAIGHT_DISTANCE          10

// Circle speeds
#define RIGHT_COUNT_CIRCLE                  8
#define LEFT_COUNT_CIRCLE                   3

// Triangle wheel speeds
#define TRIANGLE_STRAIGHT_RIGHT_ON_TIME     8
#define TRIANGLE_STRAIGHT_LEFT_ON_TIME      8   
#define TRIANGLE_TURN_ON_TIME               8   
#define TRIANGLE_STRAIGHT_LIMIT             6    
#define TRIANGLE_TURN_LIMIT                 5    

// Figure 8 wheel speeds
#define FIGURE8_CIRCLE_1_RIGHT_ON_TIME      8
#define FIGURE8_CIRCLE_1_LEFT_ON_TIME       3

#define FIGURE8_CIRCLE_2_RIGHT_ON_TIME      1
#define FIGURE8_CIRCLE_2_LEFT_ON_TIME       8

#define FIGURE8_CIRCLE_3_RIGHT_ON_TIME      8
#define FIGURE8_CIRCLE_3_LEFT_ON_TIME       3

#define FIGURE8_CIRCLE_4_RIGHT_ON_TIME      1
#define FIGURE8_CIRCLE_4_LEFT_ON_TIME       8

typedef enum {
    WAIT = 'W',
    START = 'S',
    RUN = 'R',
    END = 'E',
    STANDBY = 'B'
} State;

typedef enum {
    NONE = 'N',
    CIRCLE = 'C',
    FIGURE_8 = '8',
    TRIANGLE = 'T'
} Event;

typedef enum {
    PRESSED = 1,
    RELEASED = 0,
    DEBOUNCE_RESTART = 0,
    DEBOUNCE_TIME = 5,
} SwitchPosition;

typedef enum {
    LOCK = 0,
    UNLOCK = 1
} SwitchLock;

typedef enum {
    TRIANGLE_STRAIGHT = 0,
    TRIANGLE_TURN = 1
} TriangleState;

typedef enum {
    FIGURE8_CIRCLE_1 = 0,
    FIGURE8_CIRCLE_2 = 1,
    FIGURE8_CIRCLE_3 = 2,
    FIGURE8_CIRCLE_4 = 3
} Figure8State;

typedef enum {
    CIRCLE_1 = 0,
    CIRCLE_2 = 1
} CircleState;

typedef struct {
    State state;
    Event event;
    TriangleState triangle_state;
    Figure8State figure8_state;
    CircleState circle_state;
    volatile unsigned int delay_start;
    volatile unsigned int segment_count;
    volatile unsigned int right_motor_count;
    volatile unsigned int left_motor_count;
    volatile unsigned int triangle_straight_count;
    volatile unsigned int triangle_turn_count;
} Robot;

volatile int sw1_position;
volatile int sw2_position;
volatile int switch1_lock;
volatile int switch2_lock;
volatile int count_debounce_sw1;
volatile int count_debounce_sw2;

void initRobot(Robot* robot);
void resetRobot(Robot* robot);
void updateRobot(Robot* robot, int Time_Sequence);
void run_shape_logic(Robot* robot);
void setRobotShape(Robot* robot, Event shape);
void switchLeftProcess(Robot* robot);
void switchRightProcess(Robot* robot);
void switchProcess(Robot* robot);
void run_circle(Robot* robot);
void run_figure8(Robot* robot);
void run_triangle(Robot* robot);
void displayLog(char* message);

#endif // ROBOT_H
