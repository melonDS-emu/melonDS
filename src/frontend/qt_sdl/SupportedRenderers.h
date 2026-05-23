/*
    Copyright 2016-2024 melonDS team

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

#ifndef SUPPORTEDRENDERERS_H
#define SUPPORTEDRENDERERS_H

using namespace melonDS;

#include "glad/glad.h"

#include <QApplication>

#include "EmuInstance.h"


class SupportedRenderers
{
public:
    explicit SupportedRenderers(QWidget* parent);
    ~SupportedRenderers();

    static SupportedRenderers* instance;

    // Software
    bool software;

    // OpenGL
    bool baseGl;
    bool computeGl;

    // Future renderers

private:
    void setSupportedOpenGLRenderers(QWidget* parent);
};

#endif // SUPPORTEDRENDERERS_H
