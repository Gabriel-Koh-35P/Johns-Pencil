//---------------------------------------------------------
// file:	splashscreen.c
// author:  Muhd Nur Aiman Bin Azmi
// email:	m.binazmi@digipen.edu
//
// brief:	Splash screen implementation.
//---------------------------------------------------------

#include "cprocessing.h"
#include "splashscreen.h"
#include "mainmenu.h"
//---------------------------------------------------------
#define SPLASH_FADE_IN   0.6f   // seconds fading in from black
#define SPLASH_HOLD      2.0f   // seconds fully visible (must be >= 2)
#define SPLASH_FADE_OUT  0.6f   // seconds fading out to black
#define SPLASH_TOTAL     (SPLASH_FADE_IN + SPLASH_HOLD + SPLASH_FADE_OUT)
//---------------------------------------------------------
#define LOGO_WIDTH       800.0f
#define BG_STRIPS        90
//---------------------------------------------------------

static CP_Image logoImage;
static CP_Font font;
static float timer;
//---------------------------------------------------------

// initializes the splash screen, loading the logo and font, and setting up text and graphics settings
void Splash_Init(void)
{
    logoImage = CP_Image_Load("Assets/DigiPen_White.png");
    font = CP_Font_Load("Assets/Exo2-Regular.ttf");
    CP_Font_Set(font);
    CP_Settings_TextSize(22.0f);
    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
    CP_Settings_RectMode(CP_POSITION_CORNER);
    CP_Settings_NoStroke();

    timer = 0.0f;
}

// updates the splash screen each frame, drawing the logo and background, and handling the fade in/out effect
void Splash_Update(void)
{
    float w = (float)CP_System_GetWindowWidth();
    float h = (float)CP_System_GetWindowHeight();
    float fade;
    int i;

    timer += CP_System_GetDt();

    // 0..1 visibility: fade in, hold, fade out
    if (timer < SPLASH_FADE_IN)
        fade = timer / SPLASH_FADE_IN;
    else if (timer < SPLASH_FADE_IN + SPLASH_HOLD)
        fade = 1.0f;
    else
        fade = 1.0f - (timer - SPLASH_FADE_IN - SPLASH_HOLD) / SPLASH_FADE_OUT;
    fade = CP_Math_ClampFloat(fade, 0.0f, 1.0f);

    // Background: deep navy at the top blending into DigiPen crimson at the bottom
    {
        CP_Color top = CP_Color_Create(8, 12, 32, 255);
        CP_Color bottom = CP_Color_Create(88, 10, 34, 255);
        float stripH = h / BG_STRIPS;
        for (i = 0; i < BG_STRIPS; i++)
        {
            CP_Settings_Fill(CP_Color_Lerp(top, bottom, (float)i / (BG_STRIPS - 1)));
            CP_Graphics_DrawRect(0.0f, i * stripH, w, stripH + 1.0f);
        }
    }

    // Logo (keeps its aspect ratio), slightly above centre
    {
        float imgW = (float)CP_Image_GetWidth(logoImage);
        float imgH = (float)CP_Image_GetHeight(logoImage);
        float logoH = LOGO_WIDTH * imgH / imgW;
        float logoY = h * 0.45f;

        CP_Image_Draw(logoImage, w * 0.5f, logoY, LOGO_WIDTH, logoH, 255);

        // Thin crimson accent line under the logo
        CP_Settings_Fill(CP_Color_Create(214, 32, 52, 255));
        CP_Graphics_DrawRect(w * 0.5f - 90.0f, logoY + logoH * 0.5f + 36.0f, 180.0f, 4.0f);
    }

    // Copyright line, \xC2\xA9 is the UTF-8 copyright symbol
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 210));
    CP_Font_DrawText("All content \xC2\xA9 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.",
        w * 0.5f, h - 50.0f);

    // Black overlay drives the fade in / fade out of everything above
    CP_Settings_Fill(CP_Color_Create(0, 0, 0, (int)((1.0f - fade) * 255.0f)));
    CP_Graphics_DrawRect(0.0f, 0.0f, w, h);

    if (timer >= SPLASH_TOTAL)
        CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
}

// exits the splash screen and frees its resources
void Splash_Exit(void)
{
    CP_Image_Free(&logoImage);
    CP_Settings_Stroke(CP_Color_Create(0, 0, 0, 255)); // undo NoStroke for later screens
}