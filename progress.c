//---------------------------------------------------------
// file:	progress.c
// author:	Gabriel Koh Quan Kai
// email:	gabrielquankai.koh@digipen.edu
//
// brief:	Contains the functions IsAreaClicked(), IsCircleClicked() and AreCirclesIntersecting()
//
// Copyright � 2026 DigiPen, All rights reserved.
//---------------------------------------------------------
#include <math.h>
#include "cprocessing.h"
#include "progress.h"

#define LEVEL_COUNT 18

static int unlocked[LEVEL_COUNT];
static int currentLevel;
static int initialized;
static CP_Image stage1Background;
static CP_Image stage2Background;
static int       backgroundLoaded;

void Progress_LoadBackground(void)
{
    if (backgroundLoaded)
        return;

    stage1Background = CP_Image_Load("Assets/background_main_menu_stage_1.png");
    stage2Background = CP_Image_Load("Assets/background_stage_2.png");   
    backgroundLoaded = 1;
}

CP_Image Progress_GetBackground(void)
{
    if (currentLevel >= 13 && currentLevel <= 18) {
        return stage1Background;
    }
    else if (currentLevel >= 7 && currentLevel <= 12) {
        return stage2Background;
    }
    else {
        return stage1Background;
    }
}

static void DrawBackground(void)
{
    float w = (float)CP_System_GetWindowWidth();
    float h = (float)CP_System_GetWindowHeight();

    CP_Image_Draw(Progress_GetBackground(), w / 2.0f, h / 2.0f, w, h, 255);
}

void Progress_Init(void)
{
    int i;

    if (initialized)    /* guard: only the very first call does anything */
        return;
    initialized = 1;

    unlocked[0] = 1;    /* level 1 always available */
    for (i = 1; i < LEVEL_COUNT; i++)
		// this is the line that needs to be changed to 1 for all levels to be unlocked for testing purposes
		unlocked[i] = 1;  /* DEBUG: was 1 */ // user must unlock levels in order, so only the first is unlocked at the start

    currentLevel = 1;
}

int Progress_IsUnlocked(int level)
{
    if (level < 1 || level > LEVEL_COUNT)
        return 0;
    return unlocked[level - 1];
}

void Progress_UnlockLevel(int level)
{
    if (level < 1 || level > LEVEL_COUNT)
        return;
    unlocked[level - 1] = 1;
}

int  Progress_GetCurrentLevel(void) { return currentLevel; }
void Progress_SetCurrentLevel(int level) { currentLevel = level; }
