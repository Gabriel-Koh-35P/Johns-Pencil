#pragma once
//---------------------------------------------------------
// file:	progress.h
// author:	Gabriel Koh Quan Kai
// email:	gabrielquankai.koh@digipen.edu
//
// brief:	Header file for progress.c
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------
#pragma once

void Progress_Init(void);              /* safe to call many times; only resets once */
int  Progress_IsUnlocked(int level);   /* level is 1, 2, or 3 */
void Progress_UnlockLevel(int level);
int  Progress_GetCurrentLevel(void);
void Progress_SetCurrentLevel(int level);
void     Progress_LoadBackground(void);
CP_Image Progress_GetBackground(void);