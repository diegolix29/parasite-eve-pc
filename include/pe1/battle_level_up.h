#ifndef PE1_BATTLE_LEVEL_UP_H
#define PE1_BATTLE_LEVEL_UP_H

#include "pe1/game_state_types.h"

#include "pe1/entity_animation.h"

#include "pe1/player_entity.h"
extern BattleRewardSlot g_BattleRewardSlots[10];
extern u8 g_BattleStepIndex;
extern s16 g_MenuFrameCommand;
extern u32 g_BattleExpReward;
extern u16 g_BattlePeBonusReward;
extern int g_BattleModeState;


void Pm_StopAllBoth(void);
void Aya_SetTotalExp(s32 exp, s32 pe_bonus, void *rewards);
int Render_BeginSceneLoad(void);
int CdRom_StepReadState(int mode);
void Battle_SetupPlayerPalette(void);
void Battle_StepLevelUp(void);

#endif
