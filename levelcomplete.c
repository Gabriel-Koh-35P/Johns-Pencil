//---------------------------------------------------------
// file:	levelcomplete.c
// author:	Gabriel Koh Quan Kai
// email:	gabrielquankai.koh@digipen.edu
//
// brief:	Contains the functions IsAreaClicked(), IsCircleClicked() and AreCirclesIntersecting()
//
// Copyright � 2026 DigiPen, All rights reserved.
//---------------------------------------------------------
#include <math.h>
#include "cprocessing.h"
#include "levelcomplete.h"
#include "mainmenu.h"
#include "game.h"
#include "progress.h"
#include "utils.h"

#define LEVEL_COUNT   6

#define BANNER_W      800.0f
#define BANNER_H      250.0f
#define BTN_W         400.0f
#define BTN_H         250.0f
#define MAIN_BTN_W    388.0f
#define MAIN_BTN_H    240.0f

static CP_Image bannerImage;
static CP_Image menuBtnImage;
static CP_Image nextBtnImage;

static float centerX;
static float titleY, nextY, menuY;
static int   hasNextLevel;

void Level_Complete_Init(void)
{
    float w = (float)CP_System_GetWindowWidth();
    float h = (float)CP_System_GetWindowHeight();

    centerX = w * 0.5f;
    titleY = h * 0.2f;
    nextY = h * 0.45f;
    menuY = h * 0.65f;

    hasNextLevel = (Progress_GetCurrentLevel() < 6);   /* was < 3; update if not already changed for 6 levels */

    Progress_LoadBackground();

    bannerImage = CP_Image_Load("Assets/level_complete.png");
    menuBtnImage = CP_Image_Load("Assets/main-menu.png");
    nextBtnImage = CP_Image_Load("Assets/next-level.png");

    CP_Settings_RectMode(CP_POSITION_CENTER);
}

void Level_Complete_Update(void)
{
    float w = (float)CP_System_GetWindowWidth();
    float h = (float)CP_System_GetWindowHeight();

    CP_Image_Draw(Progress_GetBackground(), w / 2.0f, h / 2.0f, w, h, 255);

    CP_Image_Draw(bannerImage, centerX, titleY, BANNER_W, BANNER_H, 255);

    if (hasNextLevel)
        CP_Image_Draw(nextBtnImage, centerX, nextY, BTN_W, BTN_H, 255);

    CP_Image_Draw(menuBtnImage, centerX, menuY, MAIN_BTN_W, MAIN_BTN_H, 255);

    if (CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT))
    {
        float mx = CP_Input_GetMouseX();
        float my = CP_Input_GetMouseY();

        if (IsAreaClicked(centerX, menuY, BTN_W, BTN_H, mx, my))
        {
            CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
        }
        else if (hasNextLevel && IsAreaClicked(centerX, nextY, BTN_W, BTN_H, mx, my))
        {
            Progress_SetCurrentLevel(Progress_GetCurrentLevel() + 1);
            CP_Engine_SetNextGameState(Game_Init, Game_Update, Game_Exit);
        }
    }
}

void Level_Complete_Exit(void)
{
}


