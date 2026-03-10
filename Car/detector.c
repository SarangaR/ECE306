#include "msp430.h"
#include "include/detector.h"
#include "include/adc.h"

#pragma CODE_SECTION(getDetectorValue, ".TI.ramfunc")
#pragma CODE_SECTION(getDetectorWhiteLevel, ".TI.ramfunc")

// Per-side white calibration ranges
volatile int detector_left_white_min  = 0;
volatile int detector_left_white_max  = DETECTOR_WHITE_THRESHOLD_DEFAULT;
volatile int detector_right_white_min = 0;
volatile int detector_right_white_max = DETECTOR_WHITE_THRESHOLD_DEFAULT;

// Per-side black calibration ranges
volatile int detector_left_black_min  = DETECTOR_BLACK_THRESHOLD_DEFAULT;
volatile int detector_left_black_max  = ADC_MAX_VALUE;
volatile int detector_right_black_min = DETECTOR_BLACK_THRESHOLD_DEFAULT;
volatile int detector_right_black_max = ADC_MAX_VALUE;

static int clampAdc(int value)
{
    if (value < 0)
    {
        return 0;
    }
    if (value > (int)ADC_MAX_VALUE)
    {
        return (int)ADC_MAX_VALUE;
    }
    return value;
}

static int getRaw(DetectorSide side)
{
    if (side == DETECTOR_LEFT)
    {
        return (int)adc_left_det_raw;
    }
    return (int)adc_right_det_raw;
}

// Set white range individually for each detector from their current readings.
void detectorSetWhiteRangeFromCurrent(void)
{
    int left_raw  = getRaw(DETECTOR_LEFT);
    int right_raw = getRaw(DETECTOR_RIGHT);

    // Left white range
    detector_left_white_min = clampAdc(left_raw - DETECTOR_CAL_RANGE_HALF);
    detector_left_white_max = clampAdc(left_raw + DETECTOR_CAL_RANGE_HALF);

    // Right white range
    detector_right_white_min = clampAdc(right_raw - DETECTOR_CAL_RANGE_HALF);
    detector_right_white_max = clampAdc(right_raw + DETECTOR_CAL_RANGE_HALF);
}

// Set black range individually for each detector from their current readings.
void detectorSetBlackRangeFromCurrent(void)
{
    int left_raw  = getRaw(DETECTOR_LEFT);
    int right_raw = getRaw(DETECTOR_RIGHT);

    // Left black range
    detector_left_black_min = clampAdc(left_raw - DETECTOR_CAL_RANGE_HALF);
    detector_left_black_max = clampAdc(left_raw + DETECTOR_CAL_RANGE_HALF);

    // Right black range
    detector_right_black_min = clampAdc(right_raw - DETECTOR_CAL_RANGE_HALF);
    detector_right_black_max = clampAdc(right_raw + DETECTOR_CAL_RANGE_HALF);
}

int getDetectorValue(DetectorSide side)
{
    int raw = getRaw(side);
    int value = (int)((long)raw * 100L / (long)ADC_MAX_VALUE);
    if (value > 100) value = 100;
    if (value < 0)   value = 0;
    return value;
}

int getRawDetectorValue(DetectorSide side)
{
    return getRaw(side);
}

float getDetectorWhiteLevel(DetectorSide side)
{
    int raw       = getRaw(side);
    int white_ref = (side == DETECTOR_LEFT) ? detector_left_white_max  : detector_right_white_max;
    int black_ref = (side == DETECTOR_LEFT) ? detector_left_black_min  : detector_right_black_min;
    int span      = black_ref - white_ref;
    float value;

    if (span <= 0)
    {
        span = 1;
    }

    /* 0.0 => black range, 1.0 => white range */
    value = (float)(black_ref - raw) / (float)span;

    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;

    return value;
}

Color getDetectedColor(DetectorSide side)
{
    int raw       = getRaw(side);
    int white_min = (side == DETECTOR_LEFT) ? detector_left_white_min  : detector_right_white_min;
    int white_max = (side == DETECTOR_LEFT) ? detector_left_white_max  : detector_right_white_max;
    int black_min = (side == DETECTOR_LEFT) ? detector_left_black_min  : detector_right_black_min;
    int black_max = (side == DETECTOR_LEFT) ? detector_left_black_max  : detector_right_black_max;

    if ((raw >= white_min) && (raw <= white_max))
    {
        return COLOR_WHITE;
    }
    if ((raw >= black_min) && (raw <= black_max))
    {
        return COLOR_BLACK;
    }

    // Between ranges: pick whichever center is closer
    {
        int white_center = (white_min + white_max) / 2;
        int black_center = (black_min + black_max) / 2;
        int delta_white  = raw - white_center;
        int delta_black  = raw - black_center;
        if (delta_white < 0) delta_white = -delta_white;
        if (delta_black < 0) delta_black = -delta_black;
        return (delta_white < delta_black) ? COLOR_WHITE : COLOR_BLACK;
    }
}
