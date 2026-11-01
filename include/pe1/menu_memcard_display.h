#ifndef PE1_MENU_MEMCARD_DISPLAY_H
#define PE1_MENU_MEMCARD_DISPLAY_H

#include "common.h"
#include "pe1/display_env.h"
#include "pe1/menu_widget.h"
#include "pe1/psyq_gpu.h"

/* Video display mode of each copied player (2 = 320 wide, 3 = 480 wide). */
extern u8 g_MenuCardScreenVideoDecodeMode;
extern u8 g_MenuCardVideoDecodeMode;
extern u8 g_MenuCardVideoDepthFlip;
extern s16 g_MenuCardVideoSize[2];
int MenuCardFirst_InitVideoBuffers(s8 count, u8 **buffers);
int MenuCardFirst_OpenVideo(s32 number);
void MenuCardFirst_RequestVideoDepthFlip(s8 enabled);
int MenuCardFirst_StepVideo(void);
void MenuCardFirst_StopVideo(void);
void MenuCardFirst_ResetDisplayState(void);

/* Prim list the memory card progress widget flushes; 0x40 marks a write. */
extern int g_MemCardProgressPrimList;
void MemCard_DrawProgress(MenuWidgetNode *node);


#endif
