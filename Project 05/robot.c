#include "Include\robot.h"

static int clampDuty(int duty)
{
    if (duty < 0)
    {
        return 0;
    }
    if (duty > DUTY_CYCLE_MAX)
    {
        return DUTY_CYCLE_MAX;
    }
    return duty;
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

static float headingError(float target, float current)
{
    return wrapAngle(target - current);
}

static void applyDutyCycle(Robot *robot, int leftDuty, int rightDuty)
{
    leftDuty = clampDuty(leftDuty);
    rightDuty = clampDuty(rightDuty);

    allMotorsOn();

    if ((int)robot->left_motor_count >= leftDuty)
    {
        leftMotorOff();
    }
    if ((int)robot->right_motor_count >= rightDuty)
    {
        rightMotorOff();
    }

    robot->left_motor_count++;
    robot->right_motor_count++;

    if (cycle_time >= WHEEL_COUNT_TIME)
    {
        cycle_time = 0;
        robot->left_motor_count = 0;
        robot->right_motor_count = 0;
        robot->segment_count++;
    }
}

void initRobot(Robot *robot) {
    robot->state = WAIT;
    robot->event = NONE;
    robot->delay_start = 0;
    robot->right_motor_count = 0;
    robot->left_motor_count = 0;
    robot->triangle_straight_count = 0;
    robot->triangle_turn_count = 0;
    robot->segment_count = 0;
    robot->target_heading = 0.0;

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
    static CircleState active_phase = CIRCLE_1;
    static float previous_heading = 0.0f;
    static float rotation_progress = 0.0f;
    static float target_heading = 0.0f;
    static unsigned int circles_completed = 0;
    static unsigned char phase_initialized = 0;
    const float full_turn_threshold = 315.0f;
    const float circle_close_window_deg = 12.0f;

    float current_heading;
    float delta_heading;
    float directed_delta;
    float error_to_target;
    float abs_error;

    if (!time_change) return;
    time_change = 0;

    current_heading = getHeading();

    if ((!phase_initialized) || (active_phase != robot->circle_state)) {
        active_phase = robot->circle_state;
        previous_heading = current_heading;
        target_heading = current_heading;
        rotation_progress = 0.0f;
        phase_initialized = 1;
        cycle_time = 0;
        robot->segment_count = 0;
        robot->left_motor_count = 0;
        robot->right_motor_count = 0;
    }

    delta_heading = wrapAngle(current_heading - previous_heading);
    previous_heading = current_heading;

    applyDutyCycle(robot, LEFT_COUNT_CIRCLE, RIGHT_COUNT_CIRCLE);

    directed_delta = -delta_heading;
    if (directed_delta > 0.0f) {
        rotation_progress += directed_delta;
    }

    error_to_target = headingError(target_heading, current_heading);
    abs_error = (error_to_target < 0.0f) ? -error_to_target : error_to_target;

    switch (robot->circle_state) {
        case CIRCLE_1:
            if ((rotation_progress >= full_turn_threshold) && (abs_error <= circle_close_window_deg)) {
                circles_completed++;
                cycle_time = 0;
                robot->segment_count = 0;
                robot->right_motor_count = 0;
                robot->left_motor_count = 0;
                rotation_progress = 0.0f;
                if (circles_completed >= 3) {
                    robot->circle_state = CIRCLE_1;
                    robot->state = END;
                    circles_completed = 0;
                }
                else {
                    robot->circle_state = CIRCLE_2;
                }
            }
            displayLogHeading("CIRCLE 1", current_heading);
            break;
        case CIRCLE_2:
            if ((rotation_progress >= full_turn_threshold) && (abs_error <= circle_close_window_deg)) {
                circles_completed++;
                cycle_time = 0;
                robot->segment_count = 0;
                robot->right_motor_count = 0;
                robot->left_motor_count = 0;
                rotation_progress = 0.0f;
                if (circles_completed >= 3) {
                    robot->circle_state = CIRCLE_1;
                    robot->state = END;
                    circles_completed = 0;
                }
                else {
                    robot->circle_state = CIRCLE_1;
                }
            }
            displayLogHeading("CIRCLE 2", current_heading);
            break;
    }
}

void run_figure8(Robot *robot) {
    static Figure8State active_phase = FIGURE8_CIRCLE_1;
    static float previous_heading = 0.0f;
    static float rotation_progress = 0.0f;
    static unsigned char phase_initialized = 0;
    const float full_turn_threshold = 330.0f;

    float current_heading;
    float delta_heading;
    float directed_delta;
    float target_heading;
    float error_to_target;
    float abs_error;
    int leftDuty;
    int rightDuty;
    int direction;

    if (!time_change) return;
    time_change = 0;

    current_heading = getHeading();

    if ((!phase_initialized) || (active_phase != robot->figure8_state)) {
        active_phase = robot->figure8_state;
        previous_heading = current_heading;
        rotation_progress = 0.0f;
        phase_initialized = 1;
        cycle_time = 0;
        robot->segment_count = 0;
        robot->left_motor_count = 0;
        robot->right_motor_count = 0;
    }

    delta_heading = wrapAngle(current_heading - previous_heading);
    previous_heading = current_heading;

    switch (robot->figure8_state) {
        case FIGURE8_CIRCLE_1:
            direction = -1;
            leftDuty = LEFT_COUNT_CIRCLE;
            rightDuty = RIGHT_COUNT_CIRCLE;
            target_heading = -90.0f;
            displayLogHeading("F8 LEFT 1", current_heading);
            break;
        case FIGURE8_CIRCLE_2:
            direction = 1;
            leftDuty = RIGHT_COUNT_CIRCLE;
            rightDuty = 1;
            target_heading = -90.0f;
            displayLogHeading("F8 RIGHT1", current_heading);
            break;
        case FIGURE8_CIRCLE_3:
            direction = -1;
            leftDuty = LEFT_COUNT_CIRCLE;
            rightDuty = RIGHT_COUNT_CIRCLE;
            target_heading = -90.0f;
            displayLogHeading("F8 LEFT 2", current_heading);
            break;
        case FIGURE8_CIRCLE_4:
        default:
            direction = 1;
            leftDuty = RIGHT_COUNT_CIRCLE;
            rightDuty = 1;
            target_heading = -90.0f;
            displayLogHeading("F8 RIGHT2", current_heading);
            break;
    }

    applyDutyCycle(robot, leftDuty, rightDuty);

    directed_delta = ((float)direction) * delta_heading;
    if (directed_delta > 0.0f) {
        rotation_progress += directed_delta;
    }

    error_to_target = headingError(target_heading, current_heading);
    abs_error = (error_to_target < 0.0f) ? -error_to_target : error_to_target;

    if ((rotation_progress >= full_turn_threshold) && (abs_error <= HEADING_TOLERANCE_DEG)) {
        cycle_time = 0;
        robot->segment_count = 0;
        robot->left_motor_count = 0;
        robot->right_motor_count = 0;
        rotation_progress = 0.0f;

        if (robot->figure8_state == FIGURE8_CIRCLE_1) {
            robot->figure8_state = FIGURE8_CIRCLE_2;
        }
        else if (robot->figure8_state == FIGURE8_CIRCLE_2) {
            robot->figure8_state = FIGURE8_CIRCLE_3;
        }
        else if (robot->figure8_state == FIGURE8_CIRCLE_3) {
            robot->figure8_state = FIGURE8_CIRCLE_4;
        }
        else {
            robot->figure8_state = FIGURE8_CIRCLE_1;
            robot->state = END;
        }
    }
}

void run_triangle(Robot *robot) {
    static const float triangle_turn_targets[3] = {-30.0f, -120.0f, 90.0f};
    float current_heading;
    float error;
    float abs_error;
    unsigned int turn_index;

    if (!time_change) return;

    if ((robot->triangle_turn_count == 0) &&
        (robot->triangle_straight_count == 0) &&
        (robot->triangle_state != TRIANGLE_TURN)) {
        robot->triangle_state = TRIANGLE_TURN;
        robot->segment_count = 0;
        robot->right_motor_count = 0;
        robot->left_motor_count = 0;
    }

    switch (robot->triangle_state) {
        case TRIANGLE_STRAIGHT:
            moveStraight(robot);
            if (robot->segment_count >= TRIANGLE_STRAIGHT_DISTANCE) {
                cycle_time = 0;
                robot->segment_count = 0;
                robot->triangle_straight_count++;

                if (robot->triangle_straight_count >= TRIANGLE_STRAIGHT_LIMIT) {
                    robot->state = END;
                }
                else {
                    robot->triangle_state = TRIANGLE_TURN;
                }

                robot->right_motor_count = 0;
                robot->left_motor_count = 0;
            }
            displayLogHeading("STRAIGHT", getHeading());
            break;
        case TRIANGLE_TURN:
            turn_index = robot->triangle_turn_count % 3;
            turnToAngle(robot, triangle_turn_targets[turn_index]);

            current_heading = getHeading();
            error = headingError(triangle_turn_targets[turn_index], current_heading);
            abs_error = (error < 0.0f) ? -error : error;

            if (abs_error <= HEADING_TOLERANCE_DEG) {
                cycle_time = 0;
                robot->segment_count = 0;
                robot->triangle_turn_count++;
                robot->triangle_state = TRIANGLE_STRAIGHT;

                robot->right_motor_count = 0;
                robot->left_motor_count = 0;
            }
            displayLogHeading("TURN", getHeading());
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

void displayLogHeading(char* message, float heading) {
    strcpy(display_line[0], "          ");
    format_heading_for_log(heading, display_line[1]);
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

void moveStraight(Robot* robot) {
    float current_heading;
    float error;
    int correction;
    int leftDuty;
    int rightDuty;

    if (!time_change)
    {
        return;
    }
    time_change = 0;

    current_heading = getHeading();
    error = headingError(robot->target_heading, current_heading);
    correction = (int)(KP * error);

    if (correction > 3)
    {
        correction = 3;
    }
    else if (correction < -3)
    {
        correction = -3;
    }

    leftDuty = STRAIGHT_BASE_DUTY + correction;
    rightDuty = STRAIGHT_BASE_DUTY - correction;

    applyDutyCycle(robot, leftDuty, rightDuty);
}

void turnToAngle(Robot* robot, float angle) {
    float current_heading;
    float error;
    float abs_error;
    int turnDuty;

    if (!time_change)
    {
        return;
    }
    time_change = 0;

    robot->target_heading = wrapAngle(angle);
    current_heading = getHeading();
    error = headingError(robot->target_heading, current_heading);
    abs_error = (error < 0.0f) ? -error : error;

    if (abs_error <= HEADING_TOLERANCE_DEG)
    {
        allMotorsOff();
        robot->left_motor_count = 0;
        robot->right_motor_count = 0;
        return;
    }

    turnDuty = (int)(abs_error * 0.12f);
    if (turnDuty < TURN_MIN_DUTY)
    {
        turnDuty = TURN_MIN_DUTY;
    }
    else if (turnDuty > TURN_MAX_DUTY)
    {
        turnDuty = TURN_MAX_DUTY;
    }

    if (error > 0.0f)
    {
        applyDutyCycle(robot, turnDuty, 0);
    }
    else
    {
        applyDutyCycle(robot, 0, turnDuty);
    }
}

void curveToAngle(Robot* robot, float angle) {
    float current_heading;
    float error;
    int delta;
    int leftDuty;
    int rightDuty;

    if (!time_change)
    {
        return;
    }
    time_change = 0;

    robot->target_heading = wrapAngle(angle);
    current_heading = getHeading();
    error = headingError(robot->target_heading, current_heading);
    delta = (int)(KP * error * 0.08f);

    if (delta > 4)
    {
        delta = 4;
    }
    else if (delta < -4)
    {
        delta = -4;
    }

    leftDuty = CURVE_BASE_DUTY + delta;
    rightDuty = CURVE_BASE_DUTY - delta;

    applyDutyCycle(robot, leftDuty, rightDuty);
}
