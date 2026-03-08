#ifndef DETECTOR_H
#define DETECTOR_H

typedef enum
{
    DETECTOR_LEFT  = 0,
    DETECTOR_RIGHT = 1
} DetectorSide;

typedef enum
{
    COLOR_WHITE = 0,
    COLOR_BLACK = 1
} Color;

#define DETECTOR_WHITE_THRESHOLD_DEFAULT  (200)
#define DETECTOR_BLACK_THRESHOLD_DEFAULT  (800)
#define DETECTOR_CAL_RANGE_HALF           (60)

extern volatile int detector_white_min;
extern volatile int detector_white_max;
extern volatile int detector_black_min;
extern volatile int detector_black_max;

float getDetectorValue(DetectorSide side);
int getRawDetectorValue(DetectorSide side);
float getDetectorWhiteLevel(DetectorSide side);
Color getDetectedColor(DetectorSide side);
void detectorSetWhiteRangeFromCurrent(void);
void detectorSetBlackRangeFromCurrent(void);

#endif // DETECTOR_H