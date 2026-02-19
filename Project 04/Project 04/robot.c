#include "Include\robot.h"

void initRobot(Robot *robot) {
    robot->state = WAIT;
    robot->event = NONE;
    robot->delay_start = 0;
    robot->right_motor_count = 0;
    robot->left_motor_count = 0;
    robot->triangle_straight_count = 0;
    robot->triangle_turn_count = 0;
    robot->segment_count = 0;

    robot->triangle_state = TRIANGLE_STRAIGHT;
    robot->figure8_state = FIGURE8_CIRCLE_1;
    robot->circle_state = CIRCLE_1;

    sw1_position = RELEASED;
    sw2_position = RELEASED;
    switch1_lock = UNLOCK;
    switch2_lock = UNLOCK;
    count_debounce_sw1 = 0;
    count_debounce_sw2 = 0;

}

void resetRobot(Robot *robot) {
    robot->state = WAIT;
    robot->delay_start = 0;
    robot->right_motor_count = 0;
    robot->left_motor_count = 0;
    robot->triangle_straight_count = 0;
    robot->triangle_turn_count = 0;
    robot->segment_count = 0;

    robot->triangle_state = TRIANGLE_STRAIGHT;
    robot->figure8_state = FIGURE8_CIRCLE_1;
    robot->circle_state = CIRCLE_1;
}

void updateRobot(Robot *robot, int Time_Sequence) {
    switchProcess(robot);
    if (robot->event == NONE) return;

    switch (robot->state) {
        case WAIT:
            allMotorsOff();
            if (time_change) {
                time_change = 0;
                if (robot->delay_start++ >= WAITING2START)
                {
                    robot->delay_start = 0;
                    robot->state = START;
                }
            }
            break;

        case START:
            robot->delay_start = 0;
            allMotorsOn();
            robot->state = RUN;
            break;

        case RUN:
            if (robot->event == CIRCLE) run_circle(robot);
            else if (robot->event == FIGURE_8) run_figure8(robot);
            else if (robot->event == TRIANGLE) run_triangle(robot);
            break;

        case END:
            allMotorsOff();
            setRobotShape(robot, NONE);
            break;
        case STANDBY:
            allMotorsOff();
            robot->delay_start = 0;
            break;
    }
}

void setRobotShape(Robot *robot, Event shape) {
    resetRobot(robot);
    robot->event = shape;
    robot->state = STANDBY;
    robot->delay_start = 0;
    strcpy(display_line[0], "          ");
    strcpy(display_line[1], "  SHAPE:  ");
    switch (robot->event) {
    case NONE:
        strcpy(display_line[2], "   NONE   ");
        break;
    case CIRCLE:
        strcpy(display_line[2], "  CIRCLE  ");
        break;
    case FIGURE_8:
        strcpy(display_line[2], "   FIG8   ");
        break;
    case TRIANGLE:
        strcpy(display_line[2], " TRIANGLE ");
        break;
    default:
        strcpy(display_line[2], "   NONE   ");
        break;
    }
    strcpy(display_line[3], "          ");

    update_string(display_line[0], 0);
    update_string(display_line[1], 11);
    update_string(display_line[2], 22);
    update_string(display_line[3], 33);
    update_display = 1;

    display_changed = TRUE;
}

void run_circle(Robot *robot) {
    if (!time_change) return;
    time_change = 0;

    allMotorsOn();
    switch (robot->circle_state) {
        case CIRCLE_1:
            if (robot->right_motor_count++ >= RIGHT_COUNT_CIRCLE) rightMotorOff();
            if (robot->left_motor_count++ >= LEFT_COUNT_CIRCLE) leftMotorOff();
            if (cycle_time >= WHEEL_COUNT_TIME) {
                cycle_time = 0;
                robot->segment_count++;
                robot->right_motor_count = 0;
                robot->left_motor_count = 0;
            }
            if (robot->segment_count >= CIRCLE_DISTANCE) {
                cycle_time = 0;
                robot->segment_count = 0;
                robot->circle_state = CIRCLE_2;
                robot->right_motor_count = 0;
                robot->left_motor_count = 0;
            }
            displayLog("CIRCLE 1");
            break;
        case CIRCLE_2:
            if (robot->right_motor_count++ >= RIGHT_COUNT_CIRCLE) rightMotorOff();
            if (robot->left_motor_count++ >= LEFT_COUNT_CIRCLE) leftMotorOff();
            if (cycle_time >= WHEEL_COUNT_TIME) {
                cycle_time = 0;
                robot->segment_count++;
                robot->right_motor_count = 0;
                robot->left_motor_count = 0;
            }
            if (robot->segment_count >= CIRCLE_DISTANCE) {
                cycle_time = 0;
                robot->segment_count = 0;
                robot->circle_state = CIRCLE_1;
                robot->right_motor_count = 0;
                robot->left_motor_count = 0;
                robot->state = END;
            }
            displayLog("CIRCLE 2");
            break;
    }
}

void run_figure8(Robot *robot) {
    if (!time_change) return;
    time_change = 0;

    allMotorsOn();
    switch (robot->figure8_state) {
        case FIGURE8_CIRCLE_1:
            if (robot->right_motor_count++ >= FIGURE8_CIRCLE_1_RIGHT_ON_TIME) rightMotorOff();
            if (robot->left_motor_count++ >= FIGURE8_CIRCLE_1_LEFT_ON_TIME) leftMotorOff();
            if (cycle_time >= WHEEL_COUNT_TIME) {
                cycle_time = 0;
                robot->segment_count++;
                robot->right_motor_count = 0;
                robot->left_motor_count = 0;
            }
            if (robot->segment_count >= FIGURE8_CIRCLE_1_DISTANCE) {
                cycle_time = 0;
                robot->segment_count = 0;
                robot->figure8_state = FIGURE8_CIRCLE_2;
                robot->right_motor_count = 0;
                robot->left_motor_count = 0;
            }
            displayLog("CIRCLE 1");
            break;
        case FIGURE8_CIRCLE_2:
            if (robot->right_motor_count++ >= FIGURE8_CIRCLE_2_RIGHT_ON_TIME) rightMotorOff();
            if (robot->left_motor_count++ >= FIGURE8_CIRCLE_2_LEFT_ON_TIME) leftMotorOff();
            if (cycle_time >= WHEEL_COUNT_TIME) {
                cycle_time = 0;
                robot->segment_count++;
                robot->right_motor_count = 0;
                robot->left_motor_count = 0;
            }
            if (robot->segment_count >= FIGURE8_CIRCLE_2_DISTANCE) {
                cycle_time = 0;
                robot->segment_count = 0;
                robot->figure8_state = FIGURE8_CIRCLE_3;
                robot->right_motor_count = 0;
                robot->left_motor_count = 0;
            }
            displayLog("CIRCLE 2");
            break;
        case FIGURE8_CIRCLE_3:
            if (robot->right_motor_count++ >= FIGURE8_CIRCLE_3_RIGHT_ON_TIME) rightMotorOff();
            if (robot->left_motor_count++ >= FIGURE8_CIRCLE_3_LEFT_ON_TIME) leftMotorOff();
            if (cycle_time >= WHEEL_COUNT_TIME) {
                cycle_time = 0;
                robot->segment_count++;
                robot->right_motor_count = 0;
                robot->left_motor_count = 0;
            }
            if (robot->segment_count >= FIGURE8_CIRCLE_3_DISTANCE) {
                cycle_time = 0;
                robot->segment_count = 0;
                robot->figure8_state = FIGURE8_CIRCLE_4;
                robot->right_motor_count = 0;
                robot->left_motor_count = 0;
            }
            displayLog("CIRCLE 3");
            break;
        case FIGURE8_CIRCLE_4:
            if (robot->right_motor_count++ >= FIGURE8_CIRCLE_4_RIGHT_ON_TIME) rightMotorOff();
            if (robot->left_motor_count++ >= FIGURE8_CIRCLE_4_LEFT_ON_TIME) leftMotorOff();
            if (cycle_time >= WHEEL_COUNT_TIME) {
                cycle_time = 0;
                robot->segment_count++;
                robot->right_motor_count = 0;
                robot->left_motor_count = 0;
            }
            if (robot->segment_count >= FIGURE8_CIRCLE_4_DISTANCE) {
                cycle_time = 0;
                robot->segment_count = 0;
                robot->figure8_state = FIGURE8_CIRCLE_1;
                robot->right_motor_count = 0;
                robot->left_motor_count = 0;
                robot->state = END;
            }
            displayLog("CIRCLE 4");
            break;
    }
}

void run_triangle(Robot *robot) {
    if (!time_change) return;
    time_change = 0;

    switch (robot->triangle_state) {
        case TRIANGLE_STRAIGHT:
            allMotorsOn();
            if (robot->right_motor_count++ >= TRIANGLE_STRAIGHT_RIGHT_ON_TIME) {
                rightMotorOff();
                robot->right_motor_count = 0;
            }
            if (robot->left_motor_count++ >= TRIANGLE_STRAIGHT_LEFT_ON_TIME) {
                leftMotorOff();
                robot->left_motor_count = 0;
            }
            if (cycle_time >= WHEEL_COUNT_TIME) {
                cycle_time = 0;
                robot->segment_count++;
                robot->right_motor_count = 0;
                robot->left_motor_count = 0;
            }
            if (robot->segment_count >= TRIANGLE_STRAIGHT_DISTANCE) {
                cycle_time = 0;
                robot->segment_count = 0;
                robot->triangle_straight_count++;
                
                if (robot->triangle_turn_count >= TRIANGLE_TURN_LIMIT) {
                    robot->state = END;
                }
                else {
                    robot->triangle_state = TRIANGLE_TURN;
                }

                robot->right_motor_count = 0;
                robot->left_motor_count = 0;
            }
            displayLog("STRAIGHT");
            break;
        case TRIANGLE_TURN:
            allMotorsOn();
            leftMotorOff();
            if (robot->right_motor_count++ >= TRIANGLE_TURN_ON_TIME) {
                rightMotorOff();
                robot->right_motor_count = 0;
            }
            if (cycle_time >= WHEEL_COUNT_TIME) {
                cycle_time = 0;
                robot->segment_count++;
                robot->right_motor_count = 0;
                robot->left_motor_count = 0;
            }
            if (robot->segment_count >= TRIANGLE_TURN_DISTANCE) {
                cycle_time = 0;
                robot->segment_count = 0;
                robot->triangle_turn_count++;
                
                if (robot->triangle_straight_count >= TRIANGLE_STRAIGHT_LIMIT) {
                    robot->state = END;
                }
                else {
                    robot->triangle_state = TRIANGLE_STRAIGHT;
                }

                robot->right_motor_count = 0;
                robot->left_motor_count = 0;
            }
            displayLog("TURN");
            break;
    }
}

// TRIANGLE -> CIRCLE -> FIGURE 8

void cycleShapeRight(Robot *robot) {
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
        setRobotShape(robot, NONE);
        break;
    }
}

void cycleShapeLeft(Robot *robot) {
    switch (robot->event) {
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
        setRobotShape(robot, CIRCLE);
        break;
    }
}

void switchRightProcess(Robot *robot) {
    if (switch1_lock == UNLOCK && sw1_position == RELEASED) {
        if (!(P4IN & SW1)) {
            sw1_position = PRESSED;
            switch1_lock = LOCK;
            count_debounce_sw1 = DEBOUNCE_RESTART;
            cycleShapeLeft(robot);
            robot->state = WAIT;
        }
    }
    if (count_debounce_sw1 <= DEBOUNCE_TIME) {
        count_debounce_sw1++;
    }
    else {
        switch1_lock = UNLOCK;
        if (P4IN & SW1)
        {
            sw1_position = RELEASED;
        }
    }
}

void switchLeftProcess(Robot *robot) {
    if (switch2_lock == UNLOCK && sw2_position == RELEASED) {
        if (!(P2IN & SW2)) {
            sw2_position = PRESSED;
            switch2_lock = LOCK;
            count_debounce_sw2 = DEBOUNCE_RESTART;
            cycleShapeRight(robot);
            robot->state = WAIT;
        }
    }
    if (count_debounce_sw2 <= DEBOUNCE_TIME) {
        count_debounce_sw2++;
    }
    else{
        switch2_lock = UNLOCK;
        if (P2IN & SW2)
        {
            sw2_position = RELEASED;
        }
    }
}

void switchProcess(Robot *robot) {
    switchLeftProcess(robot);
    switchRightProcess(robot);
}

void displayLog(char* message) {
    strcpy(display_line[0], "          ");
    strcpy(display_line[1], "   LOG:   ");

    char formatted_message[11] = "          ";
    int length = strlen(message);

    if (length > 10) {
        strncpy(formatted_message, message, 10);
    }
    else {
        int total_padding = 10 - length;
        int front_padding = total_padding / 2;
        int back_padding = total_padding - front_padding;

        memset(formatted_message, ' ', 10);
        memcpy(&formatted_message[front_padding], message, length);
        formatted_message[10] = '\0';

        if (back_padding == 0) {
            formatted_message[10] = '\0';
        }
    }

    strcpy(display_line[2], formatted_message);

    strcpy(display_line[3], "          ");

    update_string(display_line[0], 0);
    update_string(display_line[1], 11);
    update_string(display_line[2], 22);
    update_string(display_line[3], 33);
    update_display = 1;

    display_changed = TRUE;
}
