#include "msp430.h"
#include <string.h>
#include "Include\functions.h"
#include "Include\LCD.h"
#include "Include\macros.h"
#include "Include\imu.h"

static void format_bottom_heading(float heading, char *message)
{
  long heading_tenths;
  long second_tenths;
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

  if (one_second_timer >= 0.0f)
  {
    second_tenths = (long)((one_second_timer * 10.0f) + 0.5f);
  }
  else
  {
    heading_tenths = (long)((one_second_timer * 10.0f) - 0.5f);
  }

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

  format_bottom_heading(getHeading(), heading_line);
  if (strncmp(display_line[3], heading_line, 10) != 0)
  {
    memcpy(display_line[3], heading_line, 11);
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
