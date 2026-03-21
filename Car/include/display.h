#ifndef DISPLAY_H
#define DISPLAY_H

void format_signed_int_3(int value, char *out3);
void format_xy_position(float x_in, float y_in, char *message);
void format_thumbwheel_line(int thumb_val, char *message);
void format_float(float value, char *message);
void format_bottom_heading(float heading, char *message);
void format_color_line(char side, int color, char *message);
void format_raw_detector(char side, int raw, char *message);
void format_emitter_line(unsigned char on, int thumb, char *message);
void format_detector_line(char side, int raw, int color, char *message);

#endif //DISPLAY_H
