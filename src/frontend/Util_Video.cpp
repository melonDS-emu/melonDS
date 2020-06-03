/*
    Copyright 2016-2020 Arisotura

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>
#include <algorithm>

#include "FrontendUtil.h"


namespace Frontend
{

float TopScreenMtx[6];
float BotScreenMtx[6];
float TouchMtx[6];


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


void SetupScreenLayout(int screenWidth, int screenHeight, int screenLayout, int rotation, int sizing, int screenGap, bool integerScale)
{
    float refpoints[4][2] =
    {
        {0, 0}, {256, 192},
        {0, 0}, {256, 192}
    };

    int layout = screenLayout == 0
        ? ((rotation % 2 == 0) ? 0 : 1)
        : screenLayout - 1;

    float botScale = 1;
    float botTrans[4] = {0};

    M23_Identity(TopScreenMtx);
    M23_Identity(BotScreenMtx);

    M23_Translate(TopScreenMtx, -256/2, -192/2);
    M23_Translate(BotScreenMtx, -256/2, -192/2);

    // rotation
    {
        float rotmtx[6];
        M23_Identity(rotmtx);

        M23_RotateFast(rotmtx, rotation);
        M23_Multiply(TopScreenMtx, rotmtx, TopScreenMtx);
        M23_Multiply(BotScreenMtx, rotmtx, BotScreenMtx);

        M23_Transform(TopScreenMtx, refpoints[0][0], refpoints[0][1]);
        M23_Transform(TopScreenMtx, refpoints[1][0], refpoints[1][1]);
        M23_Transform(BotScreenMtx, refpoints[2][0], refpoints[2][1]);
        M23_Transform(BotScreenMtx, refpoints[3][0], refpoints[3][1]);
    }

    // move screens apart
    {
        int idx = layout == 0 ? 1 : 0;
        float offset =
            (((layout == 0 && (rotation % 2 == 0)) || (layout == 1 && (rotation % 2 == 1))
                ? 192.f : 256.f)
            + screenGap) / 2.f;
        if (rotation == 1 || rotation == 2)
            offset *= -1.f;

        M23_Translate(TopScreenMtx, (idx==0)?-offset:0, (idx==1)?-offset:0);
        M23_Translate(BotScreenMtx, (idx==0)?offset:0, (idx==1)?offset:0);

        refpoints[0][idx] -= offset;
        refpoints[1][idx] -= offset;
        refpoints[2][idx] += offset;
        refpoints[3][idx] += offset;

        botTrans[idx] = offset;
    }

    // scale
    {
        if (sizing == 0)
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

            // scale evenly
            float scale = std::min(screenWidth / hSize, screenHeight / vSize);

            if (integerScale)
                scale = floor(scale);

            M23_Scale(TopScreenMtx, scale);
            M23_Scale(BotScreenMtx, scale);

            for (int i = 0; i < 4; i++)
            {
                refpoints[i][0] *= scale;
                refpoints[i][1] *= scale;
            }

            botScale = scale;
        }
        else
        {
            int primOffset = (sizing == 1) ? 0 : 2;
            int secOffset = (sizing == 1) ? 2 : 0;
            float* primMtx = (sizing == 1) ? TopScreenMtx : BotScreenMtx;
            float* secMtx = (sizing == 1) ? BotScreenMtx : TopScreenMtx;

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

            float primHSize = layout == 1 ? std::max(primMaxX, -primMinX) : primMaxX - primMinX;
            float primVSize = layout == 0 ? std::max(primMaxY, -primMinY) : primMaxY - primMinY;

            float secHSize = layout == 1 ? std::max(secMaxX, -secMinX) : secMaxX - secMinX;
            float secVSize = layout == 0 ? std::max(secMaxY, -secMinY) : secMaxY - secMinY;

            float primScale = std::min(screenWidth / primHSize, screenHeight / primVSize);
            float secScale = 1.f;

            if (layout == 0)
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
                primScale = floor(primScale);
                secScale = floor(secScale);
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

            botScale = (sizing == 1) ? secScale : primScale;
        }
    }

    // position
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

        float width = maxX - minX;
        float height = maxY - minY;

        float tx = (screenWidth/2) - (width/2) - minX;
        float ty = (screenHeight/2) - (height/2) - minY;

        M23_Translate(TopScreenMtx, tx, ty);
        M23_Translate(BotScreenMtx, tx, ty);

        botTrans[2] = tx; botTrans[3] = ty;
    }

    // prepare a 'reverse' matrix for the touchscreen
    // this matrix undoes the transforms applied to the bottom screen
    // and can be used to calculate touchscreen coords from host screen coords
    {
        M23_Identity(TouchMtx);

        M23_Translate(TouchMtx, -botTrans[2], -botTrans[3]);
        M23_Scale(TouchMtx, 1.f / botScale);
        M23_Translate(TouchMtx, -botTrans[0], -botTrans[1]);

        float rotmtx[6];
        M23_Identity(rotmtx);
        M23_RotateFast(rotmtx, (4-rotation) & 3);
        M23_Multiply(TouchMtx, rotmtx, TouchMtx);

        M23_Translate(TouchMtx, 256/2, 192/2);
    }
}

void GetScreenTransforms(float* top, float* bot)
{
    memcpy(top, TopScreenMtx, 6*sizeof(float));
    memcpy(bot, BotScreenMtx, 6*sizeof(float));
}

void GetTouchCoords(int& x, int& y)
{
    float vx = x;
    float vy = y;

    M23_Transform(TouchMtx, vx, vy);

    x = (int)vx;
    y = (int)vy;
}

}

