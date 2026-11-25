//---------------------------------------------------------
// file:	credits.c
// author:	Muhd Nur Aiman Bin Azmi
// email:	m.binazmi@digipen.edu
//
// brief:	Rolling credits screen with the DigiPen Singapore logo
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------
#include "cprocessing.h"
#include "credits.h"
#include "mainmenu.h"
//---------------------------------------------------------
#define SCROLL_SPEED     80.0f    // pixels per second
#define LOGO_WIDTH       460.0f
#define HEADER_H         215.0f   // fixed header band (logo + copyright)
#define FADE_STRIPS      24       // soft fade under the header
#define HEADING_SIZE     44.0f
#define NAME_SIZE        34.0f
#define HEADING_STEP     64.0f    // vertical space taken by a heading
#define NAME_STEP        50.0f    // vertical space taken by a name
#define SECTION_GAP      90.0f    // empty space between sections
//---------------------------------------------------------
typedef enum { LINE_HEADING, LINE_NAME, LINE_GAP } LineType;

// struct for each line of the credits: type + text (NULL for gaps)
typedef struct
{
    LineType type;
    const char* text;
} CreditLine;

/* the credits themselves, in order of appearance.
Group Members, Instructors, President, Executives.
Each section has a heading, then names, then a gap.*/
static const CreditLine lines[] =
{
    { LINE_HEADING, "Team Members" },
    { LINE_NAME,    "Gabriel" },
    { LINE_NAME,    "Aiman" },
    { LINE_NAME,    "Jia Hock" },
    { LINE_NAME,    "Krishen" },
    { LINE_GAP,     NULL },

    { LINE_HEADING, "Instructors" },
    { LINE_NAME,    "Gerald Wong" },
    { LINE_NAME,    "Dr. Soroor" },
    { LINE_NAME,    "Tommy Tan" },
    { LINE_GAP,     NULL },

    { LINE_HEADING, "President" },
    { LINE_NAME,    "Claude Comair" },
    { LINE_GAP,     NULL },

    { LINE_HEADING, "Executives" },
    { LINE_NAME,    "CHU Jason Yeu Tat" },
    { LINE_NAME,    "Michael GATS" },
    { LINE_NAME,    "Tan Chek Min" },
    { LINE_NAME,    "Prasanna Kumar GHALI" },
    { LINE_NAME,    "Mandy WONG" },
    { LINE_NAME,    "Johnny DEEK" },
};
// number of lines in the credits array
#define LINE_COUNT ((int)(sizeof(lines) / sizeof(lines[0])))

static CP_Image logoImage;
static CP_Font font;

// y position of the first line
static float scrollY;     
// height of all the credit lines together
static float totalHeight;   

// function to initialize the credits screen, loading the logo and font, and calculating the total height of the credits
void Credits_Init(void)
{
    int i;
	// load the logo and font, set up text rendering
    logoImage = CP_Image_Load("Assets/DigiPen_White.png");
    font = CP_Font_Load("Assets/Exo2-Regular.ttf");
    CP_Font_Set(font);
    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
    CP_Settings_RectMode(CP_POSITION_CORNER);
    CP_Settings_NoStroke();

	// work out how tall the whole list is so we know when it has finished scrolling
    totalHeight = 0.0f;
    for (i = 0; i < LINE_COUNT; i++)
    {
        if (lines[i].type == LINE_HEADING)   totalHeight += HEADING_STEP;
        else if (lines[i].type == LINE_NAME) totalHeight += NAME_STEP;
        else totalHeight += SECTION_GAP;
    }

    scrollY = (float)CP_System_GetWindowHeight();   // start just below the screen
}

// function to update the credits screen each frame
void Credits_Update(void)
{
    float w = (float)CP_System_GetWindowWidth();
    float h = (float)CP_System_GetWindowHeight();
    CP_Color bg = CP_Color_Create(10, 14, 34, 255);
    float y;
    int i;

    CP_Graphics_ClearBackground(bg);

    scrollY -= SCROLL_SPEED * CP_System_GetDt();

    // Scrolling credit lines (drawn first, the header is drawn on top)
    y = scrollY;
    for (i = 0; i < LINE_COUNT; i++)
    {
        if (lines[i].type == LINE_HEADING)
        {
            CP_Settings_TextSize(HEADING_SIZE);
            CP_Settings_Fill(CP_Color_Create(214, 32, 52, 255));
            if (y > -60.0f && y < h + 60.0f)
                CP_Font_DrawText(lines[i].text, w * 0.5f, y);
            y += HEADING_STEP;
        }
        else if (lines[i].type == LINE_NAME)
        {
            CP_Settings_TextSize(NAME_SIZE);
            CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
            if (y > -60.0f && y < h + 60.0f)
                CP_Font_DrawText(lines[i].text, w * 0.5f, y);
            y += NAME_STEP;
        }
        else
        {
            y += SECTION_GAP;
        }
    }

    // Fixed header band: hides lines scrolling underneath it
    CP_Settings_Fill(bg);
    CP_Graphics_DrawRect(0.0f, 0.0f, w, HEADER_H);

    // Logo (keeps its aspect ratio)
    {
        float imgW = (float)CP_Image_GetWidth(logoImage);
        float imgH = (float)CP_Image_GetHeight(logoImage);
        float logoH = LOGO_WIDTH * imgH / imgW;
        CP_Image_Draw(logoImage, w * 0.5f, 25.0f + logoH * 0.5f, LOGO_WIDTH, logoH, 255);
    }

    // Copyright line, \xC2\xA9 is the UTF-8 copyright symbol
    CP_Settings_TextSize(18.0f);
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 210));
    CP_Font_DrawText("All content \xC2\xA9 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.",
        w * 0.5f, 188.0f);

    // Crimson accent line, then a soft fade so lines slide out smoothly
    CP_Settings_Fill(CP_Color_Create(214, 32, 52, 255));
    CP_Graphics_DrawRect(w * 0.5f - 200.0f, HEADER_H, 400.0f, 3.0f);
    for (i = 0; i < FADE_STRIPS; i++)
    {
        int a = 255 - (255 * (i + 1)) / FADE_STRIPS;
        CP_Settings_Fill(CP_Color_Create(10, 14, 34, a));
        CP_Graphics_DrawRect(0.0f, HEADER_H + 3.0f + i * 2.0f, w, 2.0f);
    }

    // Back to the menu when ESC is pressed or the credits have finished
    if (CP_Input_KeyTriggered(KEY_ESCAPE) || scrollY + totalHeight < HEADER_H)
        CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
}

// function to exit the credits screen, freeing resources
void Credits_Exit(void)
{
    CP_Image_Free(&logoImage);
    CP_Settings_Stroke(CP_Color_Create(0, 0, 0, 255)); // undo NoStroke for later screens
}
