//---------------------------------------------------------
// file:	levelfailed.c
// author:	Gabriel Koh Quan Kai
// email:	gabrielquankai.koh@digipen.edu
//
// brief:	Contains the functions IsAreaClicked(), IsCircleClicked() and AreCirclesIntersecting()
//
// Copyright � 2026 DigiPen, All rights reserved.
//---------------------------------------------------------
#include <math.h>
#include "cprocessing.h"
#include "levelfailed.h"
#include "levelselect.h"
#include "game.h"
#include "utils.h"
#include "progress.h"

#define TITLE_W       800.0f
#define TITLE_H       250.0f
#define BTN_W         400.0f
#define BTN_H         250.0f
#define MAIN_BTN_W    388.0f
#define MAIN_BTN_H    240.0f

static CP_Image titleImage;
static CP_Image retryBtnImage;
static CP_Image selectBtnImage;

static float centerX;
static float titleY, retryY, selectY;

void Level_Failed_Init(void)
{
    float w = (float)CP_System_GetWindowWidth();
    float h = (float)CP_System_GetWindowHeight();

    centerX = w * 0.5f;
    titleY = h * 0.2f;
    retryY = h * 0.45f;
    selectY = h * 0.65f;

    Progress_LoadBackground();

    titleImage = CP_Image_Load("Assets/out-of-ammo.png");
    retryBtnImage = CP_Image_Load("Assets/try-again.png");
    selectBtnImage = CP_Image_Load("Assets/level-select.png");

    CP_Settings_RectMode(CP_POSITION_CENTER);
}

void Level_Failed_Update(void)
{
    float w = (float)CP_System_GetWindowWidth();
    float h = (float)CP_System_GetWindowHeight();

    CP_Image_Draw(Progress_GetBackground(), w / 2.0f, h / 2.0f, w, h, 255);

    CP_Image_Draw(titleImage, centerX, titleY, TITLE_W, TITLE_H, 255);

    CP_Image_Draw(retryBtnImage, centerX, retryY, BTN_W, BTN_H, 255);
    CP_Image_Draw(selectBtnImage, centerX, selectY, MAIN_BTN_W, MAIN_BTN_H, 255);

    if (CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT))
    {
        float mx = CP_Input_GetMouseX();
        float my = CP_Input_GetMouseY();

        if (IsAreaClicked(centerX, retryY, BTN_W, BTN_H, mx, my))
            CP_Engine_SetNextGameState(Game_Init, Game_Update, Game_Exit);
        else if (IsAreaClicked(centerX, selectY, BTN_W, BTN_H, mx, my))
            CP_Engine_SetNextGameState(Level_Select_Init, Level_Select_Update, Level_Select_Exit);
    }
}

void Level_Failed_Exit(void)
{
}


