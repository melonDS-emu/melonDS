/*
    Copyright 2016-2025 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#ifndef SCREENLAYOUT_H
#define SCREENLAYOUT_H

enum ScreenLayoutType
{
    screenLayout_Natural, // top screen above bottom screen always
    screenLayout_Vertical,
    screenLayout_Horizontal,
    screenLayout_Hybrid,
    screenLayout_MAX,
};

enum ScreenRotation
{
    screenRot_0Deg,
    screenRot_90Deg,
    screenRot_180Deg,
    screenRot_270Deg,
    screenRot_MAX,
};

enum ScreenSizing
{
    screenSizing_Even, // both screens get same size
    screenSizing_EmphTop, // make top screen as big as possible, fit bottom screen in remaining space
    screenSizing_EmphBot,
    screenSizing_Auto, // not applied in SetupScreenLayout
    screenSizing_TopOnly,
    screenSizing_BotOnly,
    screenSizing_MAX,
};

const int kMaxScreenTransforms = 3;

class ScreenLayout
{
public:
    ScreenLayout();
    ~ScreenLayout() {}

    // setup the display layout based on the provided display size and parameters
    // * screenWidth/screenHeight: size of the host display
    // * screenLayout: how the DS screens are laid out
    // * rotation: angle at which the DS screens are presented
    // * sizing: how the display size is shared between the two screens
    // * screenGap: size of the gap between the two screens in pixels
    // * integerScale: force screens to be scaled up at integer scaling factors
    // * screenSwap: whether to swap the position of both screens
    // * topAspect/botAspect: ratio by which to scale the top and bottom screen respectively
    void Setup(int screenWidth, int screenHeight,
               ScreenLayoutType screenLayout,
               ScreenRotation rotation,
               ScreenSizing sizing,
               int screenGap,
               bool integerScale,
               bool swapScreens,
               float topAspect, float botAspect);

    // get a 2x3 transform matrix for each screen and whether it's a top or bottom screen
    // note: the transform assumes an origin point at the top left of the display,
    // X going right and Y going down
    // for each screen the source coordinates should be (0,0) and (256,192)
    // 'out' should point to an array of 6*MaxScreenTransforms floats
    // 'kind' should point to an array of MaxScreenTransforms ints
    // (0 = indicates top screen, 1 = bottom screen)
    // returns the amount of screens
    int GetScreenTransforms(float* out, int* kind);

    // de-transform the provided host display coordinates to get coordinates
    // on the bottom screen
    bool GetTouchCoords(int& x, int& y, bool clamp);

private:
    float TopScreenMtx[6];
    float BotScreenMtx[6];
    float HybScreenMtx[6];
    float TouchMtx[6];
    float HybTouchMtx[6];
    bool TopEnable;
    bool BotEnable;
    bool HybEnable;
    int HybScreen;
    int HybPrevTouchScreen; // 0:unknown, 1:buttom screen, 2:hybrid screen
};

#endif // SCREENLAYOUT_H
