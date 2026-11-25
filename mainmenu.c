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
#include "credits.h"

#define PLAY_BTN_W 500.0f
#define PLAY_BTN_H 300.0f
#define EXIT_BTN_W 600.0f
#define EXIT_BTN_H 350.0f

/* for panel drawing, info rows need to be set with icon size, gap & text size*/
#define ROW_ICON_SIZE 60.0f   /* icon fits inside this square, keeps its own aspect ratio */
#define ROW_ICON_GAP  15.0f   /* space between icon and text */
#define ROW_TEXT_SIZE 26.0f

/* for the left panel, the rows are stacked vertically, each row has this height */
#define PANEL_PAD 20.0f   /* space between panel edge and its content */
#define HEADER_H  60.0f
#define ROW_H     68.0f

/* for the panel positions, as fractions of the window dimensions */
#define PANEL_X_FRAC 0.02f   /* gap from the window edge, as a fraction of width */
#define PANEL_Y_FRAC 0.24f
#define PANEL_W_FRAC 0.31f
#define PANEL_H_FRAC 0.65f

/* for the help button, which toggles the panels */
#define HELP_BTN_W 420.0f
#define HELP_BTN_H 70.0f

/* for the credits button */
#define CREDITS_BTN_W 260.0f
#define CREDITS_BTN_H 70.0f

static CP_Font font;
static CP_Image titleImage;
static CP_Image dogImage;
static CP_Image playBtnImage;
static CP_Image exitBtnImage;

/* for the help panels, we need a bunch of icons and layout variables */
static CP_Image testIcon;   
static CP_Image iconEnemy;
static CP_Image iconPlayer;
static CP_Image iconPencil;
static CP_Image iconPen;
static CP_Image iconSpeed;
static CP_Image iconBoss;
static CP_Image iconReload;
static CP_Image iconScatter;
static CP_Image iconBox;
static float centerX, playY, exitY;

static float helpY;
static int   showHelp;   /* 1 = both panels are visible */

static float creditsX, creditsY;

/* this function draws a semi-transparent panel*/
static void DrawPanel(float x, float y, float w, float h) {
    CP_Settings_RectMode(CP_POSITION_CORNER);
    CP_Settings_NoStroke();
    CP_Settings_Fill(CP_Color_Create(0, 0, 0, 170));
    CP_Graphics_DrawRect(x, y, w, h);

    CP_Settings_RectMode(CP_POSITION_CENTER);   /* restore, the buttons expect center mode */
}

/* this function draws a single info row with an icon and text */
/* one row: icon on the left, wrapped text on the right.
   (x, y) is the row's top-left corner, rowW is the row's full width.
   icon may be NULL if the caller draws its own (yellow circle, "?") */
static void DrawInfoRow(CP_Image icon, float degrees, const char* text,
    float x, float y, float rowW)
{
    float textX = x + ROW_ICON_SIZE + ROW_ICON_GAP;
    float textW = rowW - ROW_ICON_SIZE - ROW_ICON_GAP;

    if (icon)
    {
        float iw = (float)CP_Image_GetWidth(icon);
        float ih = (float)CP_Image_GetHeight(icon);
        float scale = ROW_ICON_SIZE / ((iw > ih) ? iw : ih);   /* fit the longer side */

        CP_Image_DrawAdvanced(icon,
            x + ROW_ICON_SIZE / 2.0f, y + ROW_ICON_SIZE / 2.0f,
            iw * scale, ih * scale, 255, degrees);
    }

    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_LEFT, CP_TEXT_ALIGN_V_TOP);
    CP_Settings_TextSize(ROW_TEXT_SIZE);
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
    CP_Font_DrawTextBox(text, textX, y, textW);

    /* restore what the menu expects */
    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
}

/* this function draws the left panel with its header and rows*/
static void DrawLeftPanel(float w, float h)
{
    float panelX = w * PANEL_X_FRAC;
    float panelY = h * PANEL_Y_FRAC;
    float panelW = w * PANEL_W_FRAC;
    float panelH = h * PANEL_H_FRAC;
    float rowX = panelX + PANEL_PAD;
    float rowW = panelW - 2.0f * PANEL_PAD;
    float rowY = panelY + HEADER_H;

    DrawPanel(panelX, panelY, panelW, panelH);

    /* header */
    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
    CP_Settings_TextSize(40.0f);
    CP_Settings_Fill(CP_Color_Create(255, 220, 0, 255));
    CP_Font_DrawText("OBJECTIVES & MECHANICS", panelX + panelW / 2.0f, panelY + HEADER_H / 2.0f);

    /* rows */
    DrawInfoRow(iconEnemy, 0.0f, "Goal: Hit every enemy before you run out of ammo.",
        rowX, rowY + 0 * ROW_H, rowW);
    DrawInfoRow(iconPlayer, 0.0f, "Hold left click to aim, release to fire.",
        rowX, rowY + 1 * ROW_H, rowW);
    DrawInfoRow(iconPencil, 0.0f, "Shots bounce off walls and edges, but not the bottom.",
        rowX, rowY + 2 * ROW_H, rowW);

    /* yellow button: no image, so draw the circle ourselves */
    DrawInfoRow(NULL, 0.0f, "Shoot yellow buttons to open doors.",
        rowX, rowY + 3 * ROW_H, rowW);
    CP_Settings_Stroke(CP_Color_Create(0, 0, 0, 255));
    CP_Settings_Fill(CP_Color_Create(230, 220, 0, 255));
    CP_Graphics_DrawCircle(rowX + ROW_ICON_SIZE / 2.0f,
        rowY + 3 * ROW_H + ROW_ICON_SIZE / 2.0f, 40.0f);

    DrawInfoRow(iconPen, -90.0f, "Press E for a pen. It pierces walls but bounces off edges.",
        rowX, rowY + 4 * ROW_H, rowW);
    DrawInfoRow(iconSpeed, 0.0f, "Click the 1x/2x button to speed up your shot.",
        rowX, rowY + 5 * ROW_H, rowW);
    DrawInfoRow(iconBoss, 0.0f, "Bosses take 3 hits and move after each one.",
        rowX, rowY + 6 * ROW_H, rowW);

    /* footer */
    CP_Settings_TextSize(22.0f);
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
    CP_Font_DrawText("Press Q to quit to Level Select.",
        panelX + panelW / 2.0f, panelY + panelH - 22.0f);
}

/* this function draws the right panel with its header and rows*/
static void DrawRightPanel(float w, float h)
{
    float panelW = w * PANEL_W_FRAC;
    float panelX = w - w * PANEL_X_FRAC - panelW;   /* mirror of the left panel */
    float panelY = h * PANEL_Y_FRAC;
    float panelH = h * PANEL_H_FRAC;
    float rowX = panelX + PANEL_PAD;
    float rowW = panelW - 2.0f * PANEL_PAD;
    float rowH = 100.0f;                 /* taller than the left rows: the question line needs 3 lines */
    float rowY = panelY + HEADER_H + 40.0f;   /* 40 = room for the intro line */

    DrawPanel(panelX, panelY, panelW, panelH);

    /* header: box icon on the left, title centered */
    {
        float iw = (float)CP_Image_GetWidth(iconBox);
        float ih = (float)CP_Image_GetHeight(iconBox);
        float scale = 150.0f / ((iw > ih) ? iw : ih);

        CP_Image_Draw(iconBox, rowX + 25.0f, panelY + HEADER_H / 2.0f,
            iw * scale, ih * scale, 255);
    }
    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
    CP_Settings_TextSize(40.0f);
    CP_Settings_Fill(CP_Color_Create(255, 220, 0, 255));
    CP_Font_DrawText("POWERUP BOXES", panelX + panelW / 2.0f, panelY + HEADER_H / 2.0f);

    /* intro line */
    CP_Settings_TextSize(24.0f);
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
    CP_Font_DrawText("Shoot a box to get one of these:",
        panelX + panelW / 2.0f, panelY + HEADER_H + 20.0f);

    /* rows */
    DrawInfoRow(iconReload, -90.0f, "Reload: +2 ammo.",
        rowX, rowY + 0 * rowH, rowW);
    DrawInfoRow(iconPen, -90.0f, "Pen: a piercing shot, plus 1 ammo back.",
        rowX, rowY + 1 * rowH, rowW);
    DrawInfoRow(iconScatter, 0.0f, "Scatter: bursts into 3 bouncing shards.",
        rowX, rowY + 2 * rowH, rowW);

    /* question: no image, so draw a "?" ourselves */
    DrawInfoRow(NULL, 0.0f,
        "Question: answer right for a random reward from above. A wrong answer gives nothing.",
        rowX, rowY + 3 * rowH, rowW);
    CP_Settings_TextSize(60.0f);
    CP_Settings_Fill(CP_Color_Create(255, 220, 0, 255));
    CP_Font_DrawText("?", rowX + ROW_ICON_SIZE / 2.0f,
        rowY + 3 * rowH + ROW_ICON_SIZE / 2.0f);
}

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
    creditsX = CP_System_GetWindowWidth() - 180.0f;
    creditsY = CP_System_GetWindowHeight() - 70.0f;
    helpY = CP_System_GetWindowHeight() * 0.78f;
    showHelp = 0;

    playBtnImage = CP_Image_Load("Assets/play_btn.png");
    exitBtnImage = CP_Image_Load("Assets/exit_btn.png");
    titleImage = CP_Image_Load("Assets/johns_pencil_title.png");
    iconEnemy = CP_Image_Load("Assets/stage1_enemy.png");
    testIcon = CP_Image_Load("Assets/player.png");
    iconPencil = CP_Image_Load("Assets/pencil_projectile.png");
    iconPen = CP_Image_Load("Assets/pen_Pierce_Projectile.png");
    iconSpeed = CP_Image_Load("Assets/2xspd.png");
    iconBoss = CP_Image_Load("Assets/boss_enemy.png");
    iconReload = CP_Image_Load("Assets/pencil_ammo.png");
    iconScatter = CP_Image_Load("Assets/marker.png");
    iconBox = CP_Image_Load("Assets/powerup_box.png");

    Progress_LoadBackground();
}

void Main_Menu_Update(void)
{
    float w = (float)CP_System_GetWindowWidth();
    float h = (float)CP_System_GetWindowHeight();

    float panelW = w * 0.31f;
    float panelY = h * 0.24f;
    float panelH = h * 0.65f;

	// draw the background and title
    CP_Image_Draw(Progress_GetBackground(), w / 2.0f, h / 2.0f, w, h, 255);
	// draw the title and buttons
    CP_Image_Draw(titleImage, centerX, playY - 220.0f, 800.0f, 200.0f, 255);
	// draw the play and exit buttons
    CP_Image_Draw(playBtnImage, centerX, playY, PLAY_BTN_W, PLAY_BTN_H, 255);
    CP_Image_Draw(exitBtnImage, centerX, exitY, EXIT_BTN_W, EXIT_BTN_H, 255);

    /* Credits button */
    int hover = IsAreaClicked(creditsX, creditsY, CREDITS_BTN_W, CREDITS_BTN_H,
    CP_Input_GetMouseX(), CP_Input_GetMouseY());
    CP_Settings_Fill(CP_Color_Create(0, 0, 0, hover ? 200 : 140));
    CP_Graphics_DrawRectAdvanced(creditsX, creditsY, CREDITS_BTN_W, CREDITS_BTN_H, 0.0f, 20.0f);
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
    CP_Settings_TextSize(40.0f);
    CP_Font_DrawText("Credits", creditsX, creditsY);

    /* this shows the help panels once How to Play is clicked */
    if (showHelp)
    {
        DrawLeftPanel(w, h);
        DrawRightPanel(w, h);
    }

    /* draw the help button */
    DrawPanel(centerX - HELP_BTN_W / 2.0f, helpY - HELP_BTN_H / 2.0f, HELP_BTN_W, HELP_BTN_H);
    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
    CP_Settings_TextSize(50.0f);
    CP_Settings_Fill(CP_Color_Create(255, 220, 0, 255));
    CP_Font_DrawText(showHelp ? "HIDE HELP" : "HOW TO PLAY", centerX, helpY);

    if (CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT))
    {
        float mx = CP_Input_GetMouseX();
        float my = CP_Input_GetMouseY();

       /* check if the help button was clicked, ordered this way such that the help button is checked first*/
        if (IsAreaClicked(centerX, helpY, HELP_BTN_W, HELP_BTN_H, mx, my))
            showHelp = !showHelp;
        else if (IsAreaClicked(centerX, playY, PLAY_BTN_W, PLAY_BTN_H, mx, my))
            CP_Engine_SetNextGameState(Level_Select_Init, Level_Select_Update, Level_Select_Exit);
        else if (IsAreaClicked(centerX, exitY, EXIT_BTN_W, EXIT_BTN_H, mx, my))
            CP_Engine_Terminate();
        else if (IsAreaClicked(creditsX, creditsY, CREDITS_BTN_W, CREDITS_BTN_H, mx, my))
            CP_Engine_SetNextGameState(Credits_Init, Credits_Update, Credits_Exit);
    }
}

void Main_Menu_Exit(void)
{

}