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

#define LEVEL_COUNT 18 // total number of levels in the game
#define LEVELS_PER_PAGE 6 // 2 rows of 3 levels each page
#define PAGE_COUNT 3 // currently how many "chapters" or pages of levels there are
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
static CP_Image pageTitle3Image;
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
    pageTitle2Image = CP_Image_Load("Assets/page_2.png");    /* NEW */
    pageTitle3Image = CP_Image_Load("Assets/page_3.png");    /* NEW */
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

    //currentPage = 0; /* REMOVE: this resets to page 1 every time you re-enter */
    currentPage = (Progress_GetCurrentLevel() - 1) / LEVELS_PER_PAGE;
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
    CP_Image title = pageTitleImage;
    if (currentPage == 1) title = pageTitle2Image;
    else if (currentPage == 2) title = pageTitle3Image;
    CP_Image_Draw(title, w * 0.5f, h * 0.1f, 300.0f, 86.0f, 255);

    /* next arrow (right side), hidden on the last page */
    if (currentPage < PAGE_COUNT - 1)
        CP_Image_Draw(arrowBtnImage, w * 0.85f, pageBtnY, PAGE_BTN_W, PAGE_BTN_H, 255);

    /* previous arrow (left side, flipped), hidden on the first page */
    if (currentPage > 0)
        CP_Image_DrawAdvanced(arrowBtnImage, w * 0.15f, pageBtnY, PAGE_BTN_W, PAGE_BTN_H, 255, 180.0f);

    if (CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT))
    {
        float mx = CP_Input_GetMouseX();
        float my = CP_Input_GetMouseY();
        float w = (float)CP_System_GetWindowWidth();   /* CHANGE: needed for arrow positions */

        for (i = 0; i < LEVELS_PER_PAGE; i++)
        {
			int levelNum = currentPage * LEVELS_PER_PAGE + i + 1;

            /* CHANGE 1: was Progress_IsUnlocked(i + 1).
               Now uses levelNum so page 2 checks levels 7-12. */
            if (Progress_IsUnlocked(levelNum) &&
                IsAreaClicked(btnX[i], btnY[i], BTN_W, BTN_H, mx, my))
            {
                /* CHANGE 2: was Progress_SetCurrentLevel(i + 1).
                   Now uses levelNum so page 2 loads levels 7-12. */
                Progress_SetCurrentLevel(levelNum);
                CP_Engine_SetNextGameState(Game_Init, Game_Update, Game_Exit);
                return;
            }
        }

        /* CHANGE: next page (right arrow), not on the last page */
        if (currentPage < PAGE_COUNT - 1 &&
            IsAreaClicked(w * 0.85f, pageBtnY, PAGE_BTN_W, PAGE_BTN_H, mx, my))
        {
            currentPage++;
            return;
        }

        /* CHANGE: previous page (left arrow), not on the first page */
        if (currentPage > 0 &&
            IsAreaClicked(w * 0.15f, pageBtnY, PAGE_BTN_W, PAGE_BTN_H, mx, my))
        {
            currentPage--;
            return;
        }

        /* back button */
        if (IsAreaClicked(backX, backY, BACK_BTN_W, BACK_BTN_H, mx, my)) {
			currentPage = 0; /* reset to page 1 when returning to level select */
            CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
        }
            
    }
}

void Level_Select_Exit(void)
{
}

