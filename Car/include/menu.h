#ifndef MENU_H
#define MENU_H

void Menu_Init(void);
void Menu_Update(void);
void Menu_Render(void);
void Menu_OnSW1(void);
void Menu_OnSW2(void);
unsigned char Menu_ConsumeRunMissionRequest(void);
unsigned char Menu_ConsumeCancelMissionRequest(void);
void Menu_SetMissionRunning(unsigned char running);

/* Call after a valid ESP command has been parsed to trigger the big-display
   countdown on the ESP Cmds menu page (Project 09). */
void Menu_NotifyESPCommandReceived(void);

/* Called by ESP handler for the N command — sets pad number and switches to
   the Final Demo display page. */
void Menu_SetPadArrival(int pad_number);

#endif // MENU_H
