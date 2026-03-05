#include "msp430.h"
#include <string.h>
#include "Include\functions.h"
#include "Include\LCD.h"
#include "Include\macros.h"
#include "Include\otos.h"

static void format_signed_int_3(int value, char *out3)
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

static void format_xy_position(float x_in, float y_in, char *message)
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

static void format_bottom_heading(float heading, char *message)
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

void Display_Process(void)
{
  char heading_line[11];
  char xy_line[11];

  format_bottom_heading(getHeading(), heading_line);
  if (strncmp(display_line[3], heading_line, 10) != 0)
  {
    memcpy(display_line[3], heading_line, 11);
    display_changed = 1;
  }

  format_xy_position(getPositionX(), getPositionY(), xy_line);
  if (strncmp(display_line[2], xy_line, 10) != 0)
  {
    memcpy(display_line[2], xy_line, 11);
    display_changed = 1;
  }

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
