/*
    Copyright 2016-2026 melonDS team

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

#include <string.h>
#include <cmath>
#include <algorithm>
#include "Platform.h"
#include "ScreenLayout.h"


void M23_Identity(float* m)
{
    m[0] = 1; m[1] = 0;
    m[2] = 0; m[3] = 1;
    m[4] = 0; m[5] = 0;
}

void M23_Scale(float* m, float s)
{
    m[0] *= s; m[1] *= s;
    m[2] *= s; m[3] *= s;
    m[4] *= s; m[5] *= s;
}

void M23_Scale(float* m, float x, float y)
{
    m[0] *= x; m[1] *= y;
    m[2] *= x; m[3] *= y;
    m[4] *= x; m[5] *= y;
}

void M23_RotateFast(float* m, int angle)
{
    if (angle == 0) return;

    float temp[4]; memcpy(temp, m, sizeof(float)*4);

    switch (angle)
    {
    case 1: // 90
        m[0] = temp[2];
        m[1] = temp[3];
        m[2] = -temp[0];
        m[3] = -temp[1];
        break;

    case 2: // 180
        m[0] = -temp[0];
        m[1] = -temp[1];
        m[2] = -temp[2];
        m[3] = -temp[3];
        break;

    case 3: // 270
        m[0] = -temp[2];
        m[1] = -temp[3];
        m[2] = temp[0];
        m[3] = temp[1];
        break;
    }
}

void M23_Translate(float* m, float tx, float ty)
{
    m[4] += tx;
    m[5] += ty;
}

void M23_Multiply(float* m, float* _a, float* _b)
{
    float a[6]; memcpy(a, _a, 6*sizeof(float));
    float b[6]; memcpy(b, _b, 6*sizeof(float));

    m[0] = (a[0] * b[0]) + (a[2] * b[1]);
    m[1] = (a[1] * b[0]) + (a[3] * b[1]);

    m[2] = (a[0] * b[2]) + (a[2] * b[3]);
    m[3] = (a[1] * b[2]) + (a[3] * b[3]);

    m[4] = (a[0] * b[4]) + (a[2] * b[5]) + a[4];
    m[5] = (a[1] * b[4]) + (a[3] * b[5]) + a[5];
}

void M23_Transform(float* m, float& x, float& y)
{
    float vx = x;
    float vy = y;

    x = (vx * m[0]) + (vy * m[2]) + m[4];
    y = (vx * m[1]) + (vy * m[3]) + m[5];
}


ScreenLayout::ScreenLayout()
{
    M23_Identity(TopScreenMtx);
    M23_Identity(BotScreenMtx);
    M23_Identity(HybScreenMtx);
    M23_Identity(TouchMtx);
    M23_Identity(HybTouchMtx);
    TopEnable = true;
    BotEnable = true;
    HybEnable = false;
    HybScreen = 0;
    HybPrevTouchScreen = 0;
}

void ScreenLayout::Setup(int screenWidth, int screenHeight,
    ScreenLayoutType screenLayout,
    ScreenRotation rotation,
    ScreenSizing sizing,
    SmallScreenPosition smallScreenPosition,
    int screenGap,
    bool integerScale,
    bool swapScreens,
    float topAspect, float botAspect)
{
    HybEnable = screenLayout == screenLayout_HybridScreen;
    if (HybEnable)
    {
        screenLayout = screenLayout_Default;
        sizing = screenSizing_Even;
        HybScreen = swapScreens ? 1 : 0;
        swapScreens = false;
        topAspect = botAspect = 1;
        HybPrevTouchScreen = 0;
    }

    float refpoints[6][2] =
    {
        {0, 0}, {256, 192},
        {0, 0}, {256, 192},
        {0, 0}, {256, 192}
    };

    bool inVerticalLayout = (screenLayout == screenLayout_Default)||(screenLayout == screenLayout_LargeScreen && (smallScreenPosition == smallScreenPos_Above || smallScreenPosition == smallScreenPos_Below));
    float botScale = 1;
    float hybScale = 1;
    float botTrans[4] = {0};
    float hybTrans[2] = {0};

    M23_Identity(TopScreenMtx);
    M23_Identity(BotScreenMtx);
    M23_Identity(HybScreenMtx);

    M23_Translate(TopScreenMtx, -256/2, -192/2);
    M23_Translate(BotScreenMtx, -256/2, -192/2);

    M23_Scale(TopScreenMtx, topAspect, 1);
    M23_Scale(BotScreenMtx, botAspect, 1);

    // rotation
    {
        float rotmtx[6];
        M23_Identity(rotmtx);

        M23_RotateFast(rotmtx, rotation);
        M23_Multiply(TopScreenMtx, rotmtx, TopScreenMtx);
        M23_Multiply(BotScreenMtx, rotmtx, BotScreenMtx);
        M23_Multiply(HybScreenMtx, rotmtx, HybScreenMtx);

        M23_Transform(TopScreenMtx, refpoints[0][0], refpoints[0][1]);
        M23_Transform(TopScreenMtx, refpoints[1][0], refpoints[1][1]);
        M23_Transform(BotScreenMtx, refpoints[2][0], refpoints[2][1]);
        M23_Transform(BotScreenMtx, refpoints[3][0], refpoints[3][1]);
    }

    int posRefPointOffset = 0;
    int posRefPointCount = HybEnable ? 6 : 4;

    if (screenLayout == screenLayout_SingleScreen)
    {
        float* mtx = swapScreens == false ? TopScreenMtx : BotScreenMtx;
        int primOffset = swapScreens == false ? 0 : 2;
        int secOffset = swapScreens == true ? 2 : 0;

        float hSize = fabsf(refpoints[primOffset][0] - refpoints[primOffset+1][0]);
        float vSize = fabsf(refpoints[primOffset][1] - refpoints[primOffset+1][1]);

        float scale = std::min(screenWidth / hSize, screenHeight / vSize);
        if (integerScale)
            scale = floorf(scale);

        TopEnable = swapScreens == false;
        BotEnable = swapScreens == true;
        botScale = scale;

        M23_Scale(mtx, scale);
        refpoints[primOffset][0] *= scale;
        refpoints[primOffset][1] *= scale;
        refpoints[primOffset+1][0] *= scale;
        refpoints[primOffset+1][1] *= scale;

        posRefPointOffset = primOffset;
        posRefPointCount = 2;
    }
    else
    {
        TopEnable = BotEnable = true;

        // move screens apart
        {
            int idx;
            if (inVerticalLayout){
                idx = 1;
            } else {
                idx = 0;
            }
            float offsetTop; 
            float offsetBot;
            bool moveV = inVerticalLayout;
            if (!swapScreens){
                offsetTop = -((moveV ? 192.0 : 256.0 * topAspect) / 2.0 + screenGap / 2.0);
                offsetBot = (moveV ? 192.0 : 256.0 * botAspect) / 2.0 + screenGap / 2.0;
            } else {
                offsetTop = ((moveV ? 192.0 : 256.0 * topAspect) / 2.0 + screenGap / 2.0);
                offsetBot = -(moveV ? 192.0 : 256.0 * botAspect) / 2.0 + screenGap / 2.0;
            }


            M23_Translate(TopScreenMtx, (idx == 0)? offsetTop : 0, (idx == 1) ? offsetTop : 0);
            M23_Translate(BotScreenMtx, (idx == 0)? offsetBot : 0, (idx == 1) ? offsetBot : 0);

            refpoints[0][idx] += offsetTop;
            refpoints[1][idx] += offsetTop;
            refpoints[2][idx] += offsetBot;
            refpoints[3][idx] += offsetBot;

            botTrans[idx] = offsetBot;
        }

        // scale
        {
            if (screenLayout != screenLayout_LargeScreen)
            {
                float minX = refpoints[0][0], maxX = minX;
                float minY = refpoints[0][1], maxY = minY;

                for (int i = 1; i < 4; i++)
                {
                    minX = std::min(minX, refpoints[i][0]);
                    minY = std::min(minY, refpoints[i][1]);
                    maxX = std::max(maxX, refpoints[i][0]);
                    maxY = std::max(maxY, refpoints[i][1]);
                }

                float hSize = maxX - minX;
                float vSize = maxY - minY;

                if (HybEnable)
                {
                    hybScale = inVerticalLayout
                        ? (4 * vSize) / (3 * hSize)
                        : (4 * hSize) / (3 * vSize);
                    if (inVerticalLayout)
                        hSize += (vSize * 4) / 3;
                    else
                        vSize += (hSize * 4) / 3;
                }

                // scale evenly
                float scale = std::min(screenWidth / hSize, screenHeight / vSize);

                if (integerScale)
                    scale = floor(scale);

                hybScale *= scale;

                M23_Scale(TopScreenMtx, scale);
                M23_Scale(BotScreenMtx, scale);
                M23_Scale(HybScreenMtx, hybScale);

                for (int i = 0; i < 4; i++)
                {
                    refpoints[i][0] *= scale;
                    refpoints[i][1] *= scale;
                }

                botScale = scale;

                // move screens aside
                if (HybEnable)
                {
                    float hybWidth = inVerticalLayout
                        ? (scale * vSize * 4) / 3
                        : (scale * hSize * 4) / 3;

                    if (rotation > screenRot_90Deg)
                        hybWidth *= -1;

                    M23_Translate(TopScreenMtx, (inVerticalLayout)?hybWidth:0, (!inVerticalLayout)?hybWidth:0);
                    M23_Translate(BotScreenMtx, (inVerticalLayout)?hybWidth:0, (!inVerticalLayout)?hybWidth:0);
                    if (inVerticalLayout){
                        refpoints[0][0] += hybWidth;
                        refpoints[1][0] += hybWidth;
                        refpoints[2][0] += hybWidth;
                        refpoints[3][0] += hybWidth;
                        botTrans[2+0] += hybWidth;
                    } else {
                        refpoints[0][1] += hybWidth;
                        refpoints[1][1] += hybWidth;
                        refpoints[2][1] += hybWidth;
                        refpoints[3][1] += hybWidth;
                        botTrans[2+1] += hybWidth;
                    }
                    hybTrans[0] = scale * (rotation == screenRot_0Deg || rotation == screenRot_270Deg ? minX : maxX);
                    hybTrans[1] = scale * (rotation == screenRot_0Deg || rotation == screenRot_90Deg ? minY : maxY);
                    M23_Translate(HybScreenMtx, hybTrans[0], hybTrans[1]);

                    M23_Transform(HybScreenMtx, refpoints[4][0], refpoints[4][1]);
                    M23_Transform(HybScreenMtx, refpoints[5][0], refpoints[5][1]);
                }
            }
            else // if (screenlayout == screenLayout_LargeScreen)
            {   
                //Same as deprecated emphasize top
                int primOffset = (!swapScreens)? 0 : 2;
                int secOffset = (!swapScreens)? 2 : 0;
                float* primMtx = (!swapScreens)? TopScreenMtx : BotScreenMtx;
                float* secMtx = (!swapScreens)? BotScreenMtx : TopScreenMtx;

                float primMinX = refpoints[primOffset][0], primMaxX = primMinX;
                float primMinY = refpoints[primOffset][1], primMaxY = primMinY;
                float secMinX = refpoints[secOffset][0], secMaxX = secMinX;
                float secMinY = refpoints[secOffset][1], secMaxY = secMinY;

                primMinX = std::min(primMinX, refpoints[primOffset+1][0]);
                primMinY = std::min(primMinY, refpoints[primOffset+1][1]);
                primMaxX = std::max(primMaxX, refpoints[primOffset+1][0]);
                primMaxY = std::max(primMaxY, refpoints[primOffset+1][1]);

                secMinX = std::min(secMinX, refpoints[secOffset+1][0]);
                secMinY = std::min(secMinY, refpoints[secOffset+1][1]);
                secMaxX = std::max(secMaxX, refpoints[secOffset+1][0]);
                secMaxY = std::max(secMaxY, refpoints[secOffset+1][1]);

                float primHSize = !inVerticalLayout ? std::max(primMaxX, -primMinX) : primMaxX - primMinX;
                float primVSize = inVerticalLayout ? std::max(primMaxY, -primMinY) : primMaxY - primMinY;

                float secHSize = !inVerticalLayout ? std::max(secMaxX, -secMinX) : secMaxX - secMinX;
                float secVSize = inVerticalLayout ? std::max(secMaxY, -secMinY) : secMaxY - secMinY;

                float primScale = std::min(screenWidth / primHSize, screenHeight / primVSize);
                float secScale = 1.f;

                if (integerScale)
                    primScale = floorf(primScale);

                if (inVerticalLayout)
                {
                    if (screenHeight - primVSize * primScale < secVSize)
                        primScale = std::min(screenWidth / primHSize, (screenHeight - secVSize) / primVSize);
                    else
                        secScale = std::min((screenHeight - primVSize * primScale) / secVSize, screenWidth / secHSize);
                }
                else
                {
                    if (screenWidth - primHSize * primScale < secHSize)
                        primScale = std::min((screenWidth - secHSize) / primHSize, screenHeight / primVSize);
                    else
                        secScale = std::min((screenWidth - primHSize * primScale) / secHSize, screenHeight / secVSize);
                }

                if (integerScale)
                {
                    primScale = floorf(primScale);
                    secScale = floorf(secScale);
                }

                M23_Scale(primMtx, primScale);
                M23_Scale(secMtx, secScale);

                refpoints[primOffset+0][0] *= primScale;
                refpoints[primOffset+0][1] *= primScale;
                refpoints[primOffset+1][0] *= primScale;
                refpoints[primOffset+1][1] *= primScale;
                refpoints[secOffset+0][0] *= secScale;
                refpoints[secOffset+0][1] *= secScale;
                refpoints[secOffset+1][0] *= secScale;
                refpoints[secOffset+1][1] *= secScale;



                int primHeight = refpoints[primOffset+1][1] - refpoints[primOffset+0][1];
                int primWidth = refpoints[primOffset+1][0] - refpoints[primOffset+0][0];
                int secHeight = refpoints[secOffset+1][1] - refpoints[secOffset+0][1];
                int secWidth = refpoints[secOffset+1][0] - refpoints[secOffset+0][0];
                float primDeltaX = 0;
                float primDeltaY = 0;
                float secDeltaX = 0;
                float secDeltaY = 0;
                if (smallScreenPosition == smallScreenPos_TopRight){
                    secDeltaY = -((primHeight/2.f)-(secHeight/2.f));
                    //melonDS::Platform::Log(melonDS::Platform::LogLevel::Debug, "primHeight: %i, primWidth: %i, secHeight: %i, secWidth: %i\n", primHeight, primWidth, secHeight, secWidth);
                } else if (smallScreenPosition == smallScreenPos_BottomRight){
                    secDeltaY = (primHeight/2.f)-(secHeight/2.f);
                } else if (smallScreenPosition == smallScreenPos_TopLeft){
                    primDeltaX = secWidth;
                    secDeltaX = (-primWidth);
                    secDeltaY = -((primHeight/2.f)-(secHeight/2.f));
                } else if (smallScreenPosition == smallScreenPos_MiddleLeft){
                    secDeltaX = (-primWidth);
                    primDeltaX = secWidth;
                } else if (smallScreenPosition == smallScreenPos_BottomLeft){
                    primDeltaX = secWidth;
                    secDeltaX = (-primWidth);
                    secDeltaY = (primHeight/2.f)-(secHeight/2.f);
                } else if (smallScreenPosition == smallScreenPos_Above){
                    primDeltaY = secHeight;
                    secDeltaY = (-primHeight);
                }
                M23_Translate(primMtx, primDeltaX, primDeltaY);
                M23_Translate(secMtx, secDeltaX, secDeltaY);

                //Translate bottomTouch Matrices
                if (!swapScreens){
                    botTrans[2] += secDeltaX;
                    botTrans[3] += secDeltaY;
                }  else {
                    botTrans[2] += primDeltaX;
                    botTrans[3] += primDeltaY;
                }        
                botScale = (!swapScreens) ? secScale : primScale;
            }
        }
    }

    // position
    {
        float minX = refpoints[posRefPointOffset][0], maxX = minX;
        float minY = refpoints[posRefPointOffset][1], maxY = minY;

        for (int i = posRefPointOffset + 1; i < posRefPointOffset + posRefPointCount; i++)
        {
            minX = std::min(minX, refpoints[i][0]);
            minY = std::min(minY, refpoints[i][1]);
            maxX = std::max(maxX, refpoints[i][0]);
            maxY = std::max(maxY, refpoints[i][1]);
        }

        float width = maxX - minX;
        float height = maxY - minY;

        float tx = (screenWidth/2) - (width/2) - minX;
        float ty = (screenHeight/2) - (height/2) - minY;

        M23_Translate(TopScreenMtx, tx, ty);
        M23_Translate(BotScreenMtx, tx, ty);
        M23_Translate(HybScreenMtx, tx, ty);

        botTrans[2] += tx; botTrans[3] += ty;
        hybTrans[0] += tx; hybTrans[1] += ty;
    }

    // prepare a 'reverse' matrix for the touchscreen
    // this matrix undoes the transforms applied to the bottom screen
    // and can be used to calculate touchscreen coords from host screen coords
    if (BotEnable)
    {
        M23_Identity(TouchMtx);

        M23_Translate(TouchMtx, -botTrans[2], -botTrans[3]);
        M23_Scale(TouchMtx, 1.f / botScale);
        M23_Translate(TouchMtx, -botTrans[0], -botTrans[1]);

        float rotmtx[6];
        M23_Identity(rotmtx);
        M23_RotateFast(rotmtx, (4-rotation) & 3);
        M23_Multiply(TouchMtx, rotmtx, TouchMtx);

        M23_Scale(TouchMtx, 1.f/botAspect, 1);
        M23_Translate(TouchMtx, 256/2, 192/2);

        if (HybEnable && HybScreen == 1)
        {
            M23_Identity(HybTouchMtx);

            M23_Translate(HybTouchMtx, -hybTrans[0], -hybTrans[1]);
            M23_Scale(HybTouchMtx, 1.f/hybScale);
            M23_Multiply(HybTouchMtx, rotmtx, HybTouchMtx);
        }
    }
}

int ScreenLayout::GetScreenTransforms(float* out, int* kind)
{
    int num = 0;
    if (TopEnable)
    {
        memcpy(out + 6*num, TopScreenMtx, sizeof(TopScreenMtx));
        kind[num++] = 0;
    }
    if (BotEnable)
    {
        memcpy(out + 6*num, BotScreenMtx, sizeof(BotScreenMtx));
        kind[num++] = 1;
    }
    if (HybEnable)
    {
        memcpy(out + 6*num, HybScreenMtx, sizeof(HybScreenMtx));
        kind[num++] = HybScreen;
    }
    return num;
}

bool ScreenLayout::GetTouchCoords(int& x, int& y, bool clamp)
{
    if (HybEnable && HybScreen == 1)
    {
        float vx = x;
        float vy = y;
        float hvx = x;
        float hvy = y;

        M23_Transform(TouchMtx, vx, vy);
        M23_Transform(HybTouchMtx, hvx, hvy);

        if (clamp)
        {
            if (HybPrevTouchScreen == 1)
            {
                x = std::clamp((int)vx, 0, 255);
                y = std::clamp((int)vy, 0, 191);

                return true;
            }
            if (HybPrevTouchScreen == 2)
            {
                x = std::clamp((int)hvx, 0, 255);
                y = std::clamp((int)hvy, 0, 191);

                return true;
            }
        }
        else
        {
            if (vx >= 0 && vx < 256 && vy >= 0 && vy < 192)
            {
                HybPrevTouchScreen = 1;

                x = (int)vx;
                y = (int)vy;

                return true;
            }
            if (hvx >= 0 && hvx < 256 && hvy >= 0 && hvy < 192)
            {
                HybPrevTouchScreen = 2;

                x = (int)hvx;
                y = (int)hvy;

                return true;
            }
        }
    }
    else if (BotEnable)
    {
        float vx = x;
        float vy = y;

        M23_Transform(TouchMtx, vx, vy);

        if (clamp)
        {
            x = std::clamp((int)vx, 0, 255);
            y = std::clamp((int)vy, 0, 191);

            return true;
        }
        else
        {
            if (vx >= 0 && vx < 256 && vy >= 0 && vy < 192)
            {
                x = (int)vx;
                y = (int)vy;

                return true;
            }
        }
    }

    return false;
}
