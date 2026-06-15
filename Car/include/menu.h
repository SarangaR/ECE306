#ifndef MENU_H
#define MENU_H

#include "esp.h"

typedef enum
{
    BL_NONE = 0,
    BL_START,
    BL_INTERCEPT,
    BL_TURN,
    BL_TRAVEL,
    BL_CIRCLE,
    BL_EXIT,
    BL_STOP,
    BL_PAD_8
} BLState;

void Menu_Init(void);
void Menu_Update(void);
void Menu_Render(void);
void Menu_OnSW1(void);
void Menu_OnSW2(void);
unsigned char Menu_ConsumeRunMissionRequest(void);
unsigned char Menu_ConsumeCancelMissionRequest(void);
void Menu_SetMissionRunning(unsigned char running);

void Menu_NotifyESPCommandReceived(const ESPCommandEvent *evt);

void Menu_SetPadArrival(int pad_number);

void Menu_SetBLState(BLState state);

unsigned char Menu_IsInIRCal(void);

#endif // MENU_H
