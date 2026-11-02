//---------------------------------------------------------
// file:	levelselect.c
// author:	Gabriel Koh Quan Kai
// email:	gabrielquankai.koh@digipen.edu
//
// brief:	Contains the functions IsAreaClicked(), IsCircleClicked() and AreCirclesIntersecting()
//
// Copyright � 2026 DigiPen, All rights reserved.
//---------------------------------------------------------
#include <math.h>
#include "cprocessing.h"
#include "levelselect.h"
#include "mainmenu.h"
#include "game.h"
#include "progress.h"
#include "utils.h"
#include <stdio.h>

#define LEVEL_COUNT 6
#define LEVELS_PER_PAGE 6 // 2 rows of 3 levels each page
#define PAGE_COUNT 2 // currently 
#define COLS 3
#define BTN_W   250.0f
#define BTN_H   140.0f
#define LOCKED_BTN_W   300.0f
#define LOCKED_BTN_H   180.0f
#define BTN_GAP_X 60.0f
#define BTN_GAP_Y 60.0f
#define BACK_BTN_W 500.0f
#define BACK_BTN_H 250.0f
#define PAGE_BTN_W 200.0f
#define PAGE_BTN_H 100.0f

static CP_Image levelImages[LEVEL_COUNT];
static CP_Image lockedImage;
static CP_Image backBtnImage;
static CP_Image pageTitleImage;
static CP_Image pageTitle2Image;
static CP_Image arrowBtnImage;

static float btnX[LEVEL_COUNT];
static float btnY[LEVEL_COUNT];
static float backX, backY;

static int currentPage;
static float pageBtnX, pageBtnY;

void Level_Select_Init(void)
{
    float w = (float)CP_System_GetWindowWidth();
    float h = (float)CP_System_GetWindowHeight();
    float totalWidth = COLS * BTN_W + (COLS - 1) * BTN_GAP_X;
    float startX = (w - totalWidth) / 2.0f + BTN_W / 2.0f;
    float row0Y = h * 0.3f;
    int i;

    Progress_Init();
    Progress_LoadBackground();

    CP_Settings_TextSize(80.0f);
    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
    CP_Settings_RectMode(CP_POSITION_CENTER);

    for (i = 0; i < LEVEL_COUNT; i++)
    {
        char path[32];
        sprintf_s(path, sizeof(path), "Assets/level_%d.png", i + 1);
        levelImages[i] = CP_Image_Load(path);
    }

    lockedImage = CP_Image_Load("Assets/locked_level.png");
    backBtnImage = CP_Image_Load("Assets/menu_back_btn.png");
    pageTitleImage = CP_Image_Load("Assets/page_1.png");     /* NEW */
    pageTitle2Image = CP_Image_Load("Assets/page_2.png");
    arrowBtnImage = CP_Image_Load("Assets/next_page.png");   /* NEW */

	for (i = 0; i < LEVELS_PER_PAGE; i++) // changed: positions are now calculated based on the current page and the number of levels per page
    {
        int col = i % COLS;
        int row = i / COLS;

        btnX[i] = startX + col * (BTN_W + BTN_GAP_X);
        btnY[i] = row0Y + row * (BTN_H + BTN_GAP_Y);
    }

    backX = w * 0.5f;
    backY = h * 0.85f;

    currentPage = 0;
	pageBtnX = w * 0.85f;
	pageBtnY = h * 0.85f;
}

void Level_Select_Update(void)
{
    int i;
    float w = (float)CP_System_GetWindowWidth();
    float h = (float)CP_System_GetWindowHeight();
    float pageBtnDrawX = (currentPage == 0) ? w * 0.85f : w * 0.15f;
    const char* pageLabel = (currentPage == 0) ? ">" : "<";

    CP_Image_Draw(Progress_GetBackground(), w / 2.0f, h / 2.0f, w, h, 255);

    for (i = 0; i < LEVELS_PER_PAGE; i++)
    {
		int levelNum = currentPage * LEVELS_PER_PAGE + i + 1;
        int isUnlocked = Progress_IsUnlocked(levelNum);
        CP_Image img = isUnlocked ? levelImages[levelNum - 1] : lockedImage;

        CP_Image_Draw(img, btnX[i], btnY[i], isUnlocked ? BTN_W : LOCKED_BTN_W, isUnlocked ? BTN_H : LOCKED_BTN_H, 255);
    }

    CP_Image_Draw(backBtnImage, backX, backY, BACK_BTN_W, BACK_BTN_H, 255);

    /* page button: arrow flips 180 degrees on page 2 */
    if (currentPage == 0)
        CP_Image_Draw(arrowBtnImage, pageBtnDrawX, pageBtnY, PAGE_BTN_W, PAGE_BTN_H, 255);
    else
        CP_Image_DrawAdvanced(arrowBtnImage, pageBtnDrawX, pageBtnY, PAGE_BTN_W, PAGE_BTN_H, 255, 180.0f);

    /* page title */
    CP_Image title = (currentPage == 0) ? pageTitleImage : pageTitle2Image;
    CP_Image_Draw(title, w * 0.5f, h * 0.1f, 300.0f, 86.0f, 255);

	////check if the page button is clicked
 //   char pageText[16];
 //   sprintf_s(pageText, sizeof(pageText), "Page %d", currentPage + 1);
 //   CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
 //   CP_Font_DrawText(pageText, w * 0.5f, h * 0.08f);

    if (CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT))
    {
        float mx = CP_Input_GetMouseX();
        float my = CP_Input_GetMouseY();

        for (i = 0; i < LEVELS_PER_PAGE; i++)
        {
			int levelNum = currentPage * LEVELS_PER_PAGE + i + 1;

            if (Progress_IsUnlocked(i + 1) &&
                IsAreaClicked(btnX[i], btnY[i], BTN_W, BTN_H, mx, my))
            {
                Progress_SetCurrentLevel(i + 1);
                CP_Engine_SetNextGameState(Game_Init, Game_Update, Game_Exit);
                return;
            }
        }

        /* next / previous page button */
        if (IsAreaClicked(pageBtnDrawX, pageBtnY, PAGE_BTN_W, PAGE_BTN_H, mx, my))
            currentPage = (currentPage + 1) % PAGE_COUNT;

        /* back button */
        if (IsAreaClicked(backX, backY, BACK_BTN_W, BACK_BTN_H, mx, my))
            CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
    }
}

void Level_Select_Exit(void)
{
}

