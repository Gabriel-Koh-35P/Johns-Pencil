//---------------------------------------------------------
// file:	mainmenu.c
// author:	Gabriel Koh Quan Kai
// email:	gabrielquankai.koh@digipen.edu
//
// brief:	Logic for launching the main menu
//
// Copyright � 2026 DigiPen, All rights reserved.
//---------------------------------------------------------
#include "cprocessing.h"
#include "mainmenu.h"
#include "levelselect.h"
#include "utils.h"
#include "progress.h"

#define PLAY_BTN_W 500.0f
#define PLAY_BTN_H 300.0f
#define EXIT_BTN_W 600.0f
#define EXIT_BTN_H 350.0f

static CP_Font font;
static CP_Image titleImage;
static CP_Image dogImage;
static CP_Image playBtnImage;
static CP_Image exitBtnImage;
static float centerX, playY, exitY;

void Main_Menu_Init(void)
{
    font = CP_Font_Load("Assets/Exo2-Regular.ttf");
    CP_Font_Set(font);
    CP_Settings_TextSize(80.0f);
    CP_Settings_RectMode(CP_POSITION_CENTER);
    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);

    centerX = CP_System_GetWindowWidth() * 0.5f;
    playY = CP_System_GetWindowHeight() * 0.38f;
    exitY = CP_System_GetWindowHeight() * 0.62f;

    playBtnImage = CP_Image_Load("Assets/play_btn.png");
    exitBtnImage = CP_Image_Load("Assets/exit_btn.png");
    titleImage = CP_Image_Load("Assets/johns_pencil_title.png");

    Progress_LoadBackground();
}

void Main_Menu_Update(void)
{
    float w = (float)CP_System_GetWindowWidth();
    float h = (float)CP_System_GetWindowHeight();

    CP_Image_Draw(Progress_GetBackground(), w / 2.0f, h / 2.0f, w, h, 255);
    CP_Image_Draw(titleImage, centerX, playY - 220.0f, 600.0f, 103.0f, 255);
    CP_Image_Draw(playBtnImage, centerX, playY, PLAY_BTN_W, PLAY_BTN_H, 255);
    CP_Image_Draw(exitBtnImage, centerX, exitY, EXIT_BTN_W, EXIT_BTN_H, 255);

    if (CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT))
    {
        float mx = CP_Input_GetMouseX();
        float my = CP_Input_GetMouseY();

        if (IsAreaClicked(centerX, playY, PLAY_BTN_W, PLAY_BTN_H, mx, my))
            CP_Engine_SetNextGameState(Level_Select_Init, Level_Select_Update, Level_Select_Exit);
        else if (IsAreaClicked(centerX, exitY, EXIT_BTN_W, EXIT_BTN_H, mx, my))
            CP_Engine_Terminate();
    }
}

void Main_Menu_Exit(void)
{

}