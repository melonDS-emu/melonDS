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