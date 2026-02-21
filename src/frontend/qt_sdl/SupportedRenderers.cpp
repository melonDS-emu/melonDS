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

#include "types.h"
#include "Platform.h"
#include "Config.h"
#include "GPU.h"
#include "main.h"

#include "SupportedRenderers.h"

SupportedRenderers* SupportedRenderers::instance = nullptr;

SupportedRenderers::SupportedRenderers(QWidget* parent)
{
    if (SupportedRenderers::instance == nullptr)
        instance = this;

    software = true;

    // OpenGL
    setSupportedOpenGLRenderers(parent);

    // Future renderers
}

SupportedRenderers::~SupportedRenderers() {}

void SupportedRenderers::setSupportedOpenGLRenderers(QWidget* parent)
{
    ScreenPanelGL *glPanel = new ScreenPanelGL(parent);
    std::optional<WindowInfo> windowInfo = glPanel->getWindowInfo();

    if (windowInfo.has_value())
    {
        std::array<GL::Context::Version, 2> versionsToTry = {
            GL::Context::Version{GL::Context::Profile::Core, 4, 3},
            GL::Context::Version{GL::Context::Profile::Core, 3, 2}
        };

        std::unique_ptr<GL::Context> glContext = GL::Context::Create(*windowInfo, versionsToTry);

        if (glContext)
        {
            const char* glVersionStr = reinterpret_cast<const char*>(glGetString(GL_VERSION));

            if (glVersionStr && strlen(glVersionStr) >= 3)
            {
                int gl_version = 0;
                
                // A proper version string or object isn't provided, so we have to parse it ourselves
                if (isdigit(glVersionStr[0]) && isdigit(glVersionStr[2]))
                    gl_version = (glVersionStr[0] - '0') * 100 +
                                 (glVersionStr[2] - '0') * 10;

                // OpenGL 4.3 is required for Compute Shaders while 3.2 is the base requirement
                if (gl_version >= 430)
                    computeGl = true;
                if (gl_version >= 320)
                    baseGl = true;
            }
        }
    }

    delete glPanel;
}