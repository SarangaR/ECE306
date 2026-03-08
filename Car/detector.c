#include "msp430.h"
#include "include/detector.h"
#include "include/adc.h"

volatile int detector_white_min = 0;
volatile int detector_white_max = DETECTOR_WHITE_THRESHOLD_DEFAULT;
volatile int detector_black_min = DETECTOR_BLACK_THRESHOLD_DEFAULT;
volatile int detector_black_max = ADC_MAX_VALUE;

static float detector_right_offset_white = 0.0f;
static float detector_right_offset_black = 0.0f;
static int detector_white_avg_raw = DETECTOR_WHITE_THRESHOLD_DEFAULT;
static int detector_black_avg_raw = DETECTOR_BLACK_THRESHOLD_DEFAULT;
static unsigned int detector_white_cal_valid = 0U;
static unsigned int detector_black_cal_valid = 0U;

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

static int getLogicalRawUnaligned(DetectorSide side)
{
    if (side == DETECTOR_LEFT)
    {
        return (int)adc_left_det_raw;
    }

    return (int)adc_right_det_raw;
}

static float getDynamicRightOffset(int left_raw, int right_raw)
{
    float offset = 0.0f;

    if (detector_white_cal_valid && detector_black_cal_valid)
    {
        int avg_raw = (left_raw + right_raw) / 2;
        int span = detector_black_avg_raw - detector_white_avg_raw;
        float t;

        if (span == 0)
        {
            t = 0.0f;
        }
        else
        {
            t = (float)(avg_raw - detector_white_avg_raw) / (float)span;
        }

        if (t < 0.0f)
        {
            t = 0.0f;
        }
        if (t > 1.0f)
        {
            t = 1.0f;
        }

        offset = detector_right_offset_white +
                 (t * (detector_right_offset_black - detector_right_offset_white));
    }
    else if (detector_black_cal_valid)
    {
        offset = detector_right_offset_black;
    }
    else if (detector_white_cal_valid)
    {
        offset = detector_right_offset_white;
    }

    return offset;
}

static void getAlignedRawPair(int *left_aligned, int *right_aligned)
{
    int left_raw;
    int right_raw;
    int right_corrected;
    float dynamic_offset;

    left_raw = getLogicalRawUnaligned(DETECTOR_LEFT);
    right_raw = getLogicalRawUnaligned(DETECTOR_RIGHT);
    dynamic_offset = getDynamicRightOffset(left_raw, right_raw);

    if (dynamic_offset >= 0.0f)
    {
        right_corrected = right_raw - (int)(dynamic_offset + 0.5f);
    }
    else
    {
        right_corrected = right_raw - (int)(dynamic_offset - 0.5f);
    }

    if (left_aligned != 0)
    {
        *left_aligned = clampAdc(left_raw);
    }
    if (right_aligned != 0)
    {
        *right_aligned = clampAdc(right_corrected);
    }
}

void detectorSetWhiteRangeFromCurrent(void)
{
    int left_raw = getLogicalRawUnaligned(DETECTOR_LEFT);
    int right_raw = getLogicalRawUnaligned(DETECTOR_RIGHT);
    int right_aligned;
    int center;
    int min_v;
    int max_v;

    detector_right_offset_white = (float)(right_raw - left_raw);
    detector_white_avg_raw = (left_raw + right_raw) / 2;
    detector_white_cal_valid = 1U;

    right_aligned = right_raw - (int)(detector_right_offset_white + 0.5f);
    center = clampAdc((left_raw + right_aligned) / 2);
    min_v = clampAdc(center - DETECTOR_CAL_RANGE_HALF);
    max_v = clampAdc(center + DETECTOR_CAL_RANGE_HALF);

    if (max_v >= detector_black_min)
    {
        max_v = detector_black_min - 1;
        if (max_v < min_v)
        {
            max_v = min_v;
        }
    }

    detector_white_min = min_v;
    detector_white_max = max_v;
}

void detectorSetBlackRangeFromCurrent(void)
{
    int left_raw = getLogicalRawUnaligned(DETECTOR_LEFT);
    int right_raw = getLogicalRawUnaligned(DETECTOR_RIGHT);
    int right_aligned;
    int center;
    int min_v;
    int max_v;

    detector_right_offset_black = (float)(right_raw - left_raw);
    detector_black_avg_raw = (left_raw + right_raw) / 2;
    detector_black_cal_valid = 1U;

    right_aligned = right_raw - (int)(detector_right_offset_black + 0.5f);
    center = clampAdc((left_raw + right_aligned) / 2);
    min_v = clampAdc(center - DETECTOR_CAL_RANGE_HALF);
    max_v = clampAdc(center + DETECTOR_CAL_RANGE_HALF);

    if (min_v <= detector_white_max)
    {
        min_v = detector_white_max + 1;
        if (min_v > max_v)
        {
            min_v = max_v;
        }
    }

    detector_black_min = min_v;
    detector_black_max = max_v;
}

float getDetectorValue(DetectorSide side)
{
    int raw;
    int left_aligned;
    int right_aligned;

    getAlignedRawPair(&left_aligned, &right_aligned);

    if (side == DETECTOR_LEFT)
    {
        raw = left_aligned;
    }
    else
    {
        raw = right_aligned;
    }

    float value = (float)raw / (float)ADC_MAX_VALUE;
    value *= 100;

    if (value > 100.0f) value = 100.0f;
    if (value < 0.0f) value = 0.0f;

    return value;
}

int getRawDetectorValue(DetectorSide side) {
    int raw;
    int left_aligned;
    int right_aligned;

    getAlignedRawPair(&left_aligned, &right_aligned);

    if (side == DETECTOR_LEFT)
    {
        raw = left_aligned;
    }
    else
    {
        raw = right_aligned;
    }

    return raw;
}

float getDetectorWhiteLevel(DetectorSide side)
{
    int raw = getRawDetectorValue(side);
    int white_ref = detector_white_max;
    int black_ref = detector_black_min;
    int span = black_ref - white_ref;
    float value;

    if (span <= 0)
    {
        span = 1;
    }

    /* 0.0 => black range, 1.0 => white range */
    value = (float)(black_ref - raw) / (float)span;

    if (value < 0.0f)
    {
        value = 0.0f;
    }
    if (value > 1.0f)
    {
        value = 1.0f;
    }

    return value;
}

Color getDetectedColor(DetectorSide side)
{
    int raw;
    int white_center;
    int black_center;

    raw = getRawDetectorValue(side);

    if ((raw >= detector_white_min) && (raw <= detector_white_max))
    {
        return COLOR_WHITE;
    }
    else if ((raw >= detector_black_min) && (raw <= detector_black_max))
    {
        return COLOR_BLACK;
    }
    else
    {
        int delta_white;
        int delta_black;

        white_center = (detector_white_min + detector_white_max) / 2;
        black_center = (detector_black_min + detector_black_max) / 2;

        delta_white = raw - white_center;
        if (delta_white < 0)
        {
            delta_white = -delta_white;
        }
        delta_black = raw - black_center;
        if (delta_black < 0)
        {
            delta_black = -delta_black;
        }

        if (delta_white < delta_black)
        {
            return COLOR_WHITE;
        }
        else
        {
            return COLOR_BLACK;
        }
    }
}
