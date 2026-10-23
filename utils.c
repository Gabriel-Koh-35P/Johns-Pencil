//---------------------------------------------------------
// file:	utils.c
// author:	Gabriel Koh Quan Kai
// email:	gabrielquankai.koh@digipen.edu
//
// brief:	Contains the functions IsAreaClicked(), IsCircleClicked() and AreCirclesIntersecting()
//
// Copyright � 2026 DigiPen, All rights reserved.
//---------------------------------------------------------
#include <math.h>
#include "cprocessing.h"
#include "utils.h"

int IsAreaClicked(float area_center_x, float area_center_y, float area_width, float area_height, float click_x, float click_y)
{
    return (click_x >= area_center_x - area_width / 2.0f &&
        click_x <= area_center_x + area_width / 2.0f &&
        click_y >= area_center_y - area_height / 2.0f &&
        click_y <= area_center_y + area_height / 2.0f);
}

int IsCircleClicked(float circle_center_x, float circle_center_y, float diameter, float click_x, float click_y)
{
    float dx = click_x - circle_center_x;
    float dy = click_y - circle_center_y;
    float r = diameter / 2.0f;
    return (dx * dx + dy * dy <= r * r);
}

int AreCirclesIntersecting(float c1_x, float c1_y, float r1,
    float c2_x, float c2_y, float r2)
{
    float dx = c2_x - c1_x;
    float dy = c2_y - c1_y;
    float distSq = dx * dx + dy * dy;
    float radiiSum = r1 + r2;

    return (distSq <= radiiSum * radiiSum);
}

void DrawTextWithOutline(const char* text, float x, float y)
{
    float offset = 2.0f;

    /* dark outline: draw 4 times, offset in each direction */
    CP_Settings_Fill(CP_Color_Create(0, 0, 0, 255));
    CP_Font_DrawText(text, x - offset, y);
    CP_Font_DrawText(text, x + offset, y);
    CP_Font_DrawText(text, x, y - offset);
    CP_Font_DrawText(text, x, y + offset);

    /* light fill on top */
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
    CP_Font_DrawText(text, x, y);
}
