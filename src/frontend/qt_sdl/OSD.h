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

#ifndef OSD_H
#define OSD_H

namespace OSD
{

bool Init(QOpenGLFunctions_3_2_Core* f);
void DeInit(QOpenGLFunctions_3_2_Core* f);

void AddMessage(u32 color, const char* text);

void Update(QOpenGLFunctions_3_2_Core* f);
void DrawNative(QPainter& painter);
void DrawGL(QOpenGLFunctions_3_2_Core* f, float w, float h);

}

#endif // OSD_H
