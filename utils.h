#pragma once
//---------------------------------------------------------
// file:	utils.h
// author:	Gabriel Koh Quan Kai
// email:	gabrielquankai.koh@digipen.edu
//
// brief:	Header file for utils.c
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------
int IsAreaClicked(float area_center_x, float area_center_y, float area_width, float area_height, float click_x, float click_y);
int IsCircleClicked(float circle_center_x, float circle_center_y, float diameter, float click_x, float click_y);
int AreCirclesIntersecting(float c1_x, float c1_y, float r1, float c2_x, float c2_y, float r2);
void DrawTextWithOutline(const char* text, float x, float y);