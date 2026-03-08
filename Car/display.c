#include "msp430.h"
#include <string.h>
#include "include/functions.h"
#include "include/LCD.h"
#include "include/macros.h"
#include "include/otos.h"
#include "include/adc.h"
#include "include/detector.h"

void format_signed_int_3(int value, char *out3)
{
  char sign;
  unsigned int mag;

  if (value < 0)
  {
    sign = '-';
    mag = (unsigned int)(-value);
  }
  else
  {
    sign = '+';
    mag = (unsigned int)value;
  }

  if (mag > 999U)
  {
    mag = 999U;
  }

  out3[0] = sign;
  out3[1] = (char)('0' + ((mag / 100U) % 10U));
  out3[2] = (char)('0' + ((mag / 10U) % 10U));
  out3[3] = (char)('0' + (mag % 10U));
}

void format_xy_position(float x_in, float y_in, char *message)
{
  int x_int;
  int y_int;
  char xbuf[5];
  char ybuf[5];

  /* Convert to integer inches, round toward zero */
  x_int = (x_in >= 0.0f) ? (int)(x_in + 0.5f) : -(int)(-x_in + 0.5f);
  y_int = (y_in >= 0.0f) ? (int)(y_in + 0.5f) : -(int)(-y_in + 0.5f);

  format_signed_int_3(x_int, xbuf);
  format_signed_int_3(y_int, ybuf);

  /* Format: "X+NNNY+NNN" (10 chars) */
  message[0] = 'X';
  message[1] = xbuf[0];
  message[2] = xbuf[1];
  message[3] = xbuf[2];
  message[4] = xbuf[3];
  message[5] = 'Y';
  message[6] = ybuf[0];
  message[7] = ybuf[1];
  message[8] = ybuf[2];
  message[9] = ybuf[3];
  message[10] = '\0';
}

void format_bottom_heading(float heading, char *message)
{
  long heading_tenths;
  unsigned long magnitude;
  char sign;
  unsigned int whole;
  unsigned int tenths;

  if (heading >= 0.0f)
  {
    heading_tenths = (long)((heading * 10.0f) + 0.5f);
  }
  else
  {
    heading_tenths = (long)((heading * 10.0f) - 0.5f);
  }

  sign = (heading_tenths < 0) ? '-' : '+';
  magnitude = (heading_tenths < 0) ? (unsigned long)(-heading_tenths) : (unsigned long)heading_tenths;
  whole = (unsigned int)(magnitude / 10UL);
  tenths = (unsigned int)(magnitude % 10UL);

  message[0] = 'H';
  message[1] = ':';
  message[2] = sign;
  message[3] = (char)('0' + ((whole / 100U) % 10U));
  message[4] = (char)('0' + ((whole / 10U) % 10U));
  message[5] = (char)('0' + (whole % 10U));
  message[6] = '.';
  message[7] = (char)('0' + (tenths % 10U));
  message[8] = ' ';
  message[9] = ' ';
  message[10] = '\0';
}

void format_thumbwheel_line(int thumb_val, char *message)
{
  // Format: "Thumb: NN " (10 chars)
  message[0] = 'T';
  message[1] = 'h';
  message[2] = 'u';
  message[3] = 'm';
  message[4] = 'b';
  message[5] = ':';
  message[6] = ' ';
  if (thumb_val >= 10)
  {
    message[7] = '1';
    message[8] = '0';
  }
  else
  {
    message[7] = ' ';
    message[8] = (char)('0' + thumb_val);
  }
  message[9] = ' ';
  message[10] = '\0';
}

void format_float(int value, char *message) {
  char sign = (value < 0) ? '-' : '+';
  unsigned int mag = (value < 0) ? (unsigned int)(-value) : (unsigned int)value;

  if (mag > 999U)
  {
    mag = 999U;
  }

  message[0] = 'V';
  message[1] = 'a';
  message[2] = 'l';
  message[3] = ':';
  message[4] = sign;
  message[5] = (char)('0' + ((mag / 100U) % 10U));
  message[6] = (char)('0' + ((mag / 10U) % 10U));
  message[7] = (char)('0' + (mag % 10U));
  message[8] = ' ';
  message[9] = ' ';
  message[10] = '\0';
}

void format_color_line(char side, int color, char *message)
{
  // Format: "L: WHITE  " or "R: BLACK  " (10 chars)
  message[0] = side;
  message[1] = ':';
  message[2] = ' ';
  if (color == 0) // COLOR_WHITE
  {
    message[3] = 'W';
    message[4] = 'H';
    message[5] = 'I';
    message[6] = 'T';
    message[7] = 'E';
    message[8] = ' ';
    message[9] = ' ';
  }
  else // COLOR_BLACK
  {
    message[3] = 'B';
    message[4] = 'L';
    message[5] = 'A';
    message[6] = 'C';
    message[7] = 'K';
    message[8] = ' ';
    message[9] = ' ';
  }
  message[10] = '\0';
}

void format_raw_detector(char side, int raw, char *message)
{
  // Format: "L:  NNNN  " (10 chars), value 0-1023
  unsigned int mag = (raw < 0) ? 0U : (unsigned int)raw;
  if (mag > 9999U) mag = 9999U;
  message[0] = side;
  message[1] = ':';
  message[2] = ' ';
  message[3] = ' ';
  message[4] = (char)('0' + ((mag / 1000U) % 10U));
  message[5] = (char)('0' + ((mag / 100U) % 10U));
  message[6] = (char)('0' + ((mag / 10U) % 10U));
  message[7] = (char)('0' + (mag % 10U));
  message[8] = ' ';
  message[9] = ' ';
  message[10] = '\0';
}

void format_emitter_line(unsigned char on, int thumb, char *message)
{
  // Format: "N:EMIT ON " or "N:EMIT OFF" (10 chars)
  if (thumb > 9)
  {
    message[0] = '1';
    message[1] = '0';
  }
  else
  {
    message[0] = (char)('0' + thumb);
    message[1] = ':';
  }
  message[2] = 'E';
  message[3] = 'M';
  message[4] = 'T';
  message[5] = ' ';
  if (on)
  {
    message[6] = 'O';
    message[7] = 'N';
    message[8] = ' ';
    message[9] = ' ';
  }
  else
  {
    message[6] = 'O';
    message[7] = 'F';
    message[8] = 'F';
    message[9] = ' ';
  }
  message[10] = '\0';
}

void format_detector_line(char side, int raw, int color, char *message)
{
  // Format: "L:NNNN WHT" or "R:NNNN BLK" (10 chars)
  unsigned int mag = (raw < 0) ? 0U : (unsigned int)raw;
  if (mag > 9999U) mag = 9999U;
  message[0] = side;
  message[1] = ':';
  message[2] = (char)('0' + ((mag / 1000U) % 10U));
  message[3] = (char)('0' + ((mag / 100U) % 10U));
  message[4] = (char)('0' + ((mag / 10U) % 10U));
  message[5] = (char)('0' + (mag % 10U));
  message[6] = ' ';
  if (color == 0)
  {
    message[7] = 'W';
    message[8] = 'H';
    message[9] = 'T';
  }
  else
  {
    message[7] = 'B';
    message[8] = 'L';
    message[9] = 'K';
  }
  message[10] = '\0';
}

void Display_Process(void)
{
  if (update_display)
  {
    update_display = 0;

    if (display_changed)
    {
      display_changed = 0;
      Display_Update(0, 0, 0, 0);
    }
  }
}
