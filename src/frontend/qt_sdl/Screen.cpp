/*
    Copyright 2016-2023 melonDS team

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

#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#include <optional>
#include <vector>
#include <string>
#include <algorithm>

#include <QPaintEvent>
#include <QPainter>
#ifndef _WIN32
#ifndef APPLE
#include <qpa/qplatformnativeinterface.h>
#endif
#endif

#include "OpenGLSupport.h"
#include "duckstation/gl/context.h"

#include "main.h"

#include "NDS.h"
#include "GPU.h"
#include "GPU3D_Soft.h"
#include "GPU3D_OpenGL.h"
#include "Platform.h"
#include "Config.h"

#include "main_shaders.h"

#include "OSD.h"

using namespace melonDS;


// TEMP
extern MainWindow* mainWindow;
extern EmuThread* emuThread;
extern bool RunningSomething;
extern int autoScreenSizing;

extern int videoRenderer;
extern bool videoSettingsDirty;


ScreenHandler::ScreenHandler(QWidget* widget)
{
    widget->setMouseTracking(true);
    widget->setAttribute(Qt::WA_AcceptTouchEvents);
    QTimer* mouseTimer = setupMouseTimer();
    widget->connect(mouseTimer, &QTimer::timeout, [=] { if (Config::MouseHide) widget->setCursor(Qt::BlankCursor);});
}

ScreenHandler::~ScreenHandler()
{
    mouseTimer->stop();
    delete mouseTimer;
}

void ScreenHandler::screenSetupLayout(int w, int h)
{
    int sizing = Config::ScreenSizing;
    if (sizing == 3) sizing = autoScreenSizing;

    float aspectTop, aspectBot;

    for (auto ratio : aspectRatios)
    {
        if (ratio.id == Config::ScreenAspectTop)
            aspectTop = ratio.ratio;
        if (ratio.id == Config::ScreenAspectBot)
            aspectBot = ratio.ratio;
    }

    if (aspectTop == 0)
        aspectTop = ((float) w / h) / (4.f / 3.f);

    if (aspectBot == 0)
        aspectBot = ((float) w / h) / (4.f / 3.f);

    Frontend::SetupScreenLayout(w, h,
                                static_cast<Frontend::ScreenLayout>(Config::ScreenLayout),
                                static_cast<Frontend::ScreenRotation>(Config::ScreenRotation),
                                static_cast<Frontend::ScreenSizing>(sizing),
                                Config::ScreenGap,
                                Config::IntegerScaling != 0,
                                Config::ScreenSwap != 0,
                                aspectTop,
                                aspectBot);

    numScreens = Frontend::GetScreenTransforms(screenMatrix[0], screenKind);
}

QSize ScreenHandler::screenGetMinSize(int factor = 1)
{
    bool isHori = (Config::ScreenRotation == Frontend::screenRot_90Deg
        || Config::ScreenRotation == Frontend::screenRot_270Deg);
    int gap = Config::ScreenGap * factor;

    int w = 256 * factor;
    int h = 192 * factor;

    if (Config::ScreenSizing == Frontend::screenSizing_TopOnly
        || Config::ScreenSizing == Frontend::screenSizing_BotOnly)
    {
        return QSize(w, h);
    }

    if (Config::ScreenLayout == Frontend::screenLayout_Natural)
    {
        if (isHori)
            return QSize(h+gap+h, w);
        else
            return QSize(w, h+gap+h);
    }
    else if (Config::ScreenLayout == Frontend::screenLayout_Vertical)
    {
        if (isHori)
            return QSize(h, w+gap+w);
        else
            return QSize(w, h+gap+h);
    }
    else if (Config::ScreenLayout == Frontend::screenLayout_Horizontal)
    {
        if (isHori)
            return QSize(h+gap+h, w);
        else
            return QSize(w+gap+w, h);
    }
    else // hybrid
    {
        if (isHori)
            return QSize(h+gap+h, 3*w + (int)ceil((4*gap) / 3.0));
        else
            return QSize(3*w + (int)ceil((4*gap) / 3.0), h+gap+h);
    }
}

void ScreenHandler::screenOnMousePress(QMouseEvent* event)
{
    event->accept();
    if (event->button() != Qt::LeftButton) return;

    int x = event->pos().x();
    int y = event->pos().y();

    if (Frontend::GetTouchCoords(x, y, false))
    {
        touching = true;
        assert(emuThread->NDS != nullptr);
        emuThread->NDS->TouchScreen(x, y);
    }
}

void ScreenHandler::screenOnMouseRelease(QMouseEvent* event)
{
    event->accept();
    if (event->button() != Qt::LeftButton) return;

    if (touching)
    {
        touching = false;
        assert(emuThread->NDS != nullptr);
        emuThread->NDS->ReleaseScreen();
    }
}

void ScreenHandler::screenOnMouseMove(QMouseEvent* event)
{
    event->accept();

    showCursor();

    if (!(event->buttons() & Qt::LeftButton)) return;
    if (!touching) return;

    int x = event->pos().x();
    int y = event->pos().y();

    if (Frontend::GetTouchCoords(x, y, true))
    {
        assert(emuThread->NDS != nullptr);
        emuThread->NDS->TouchScreen(x, y);
    }
}

void ScreenHandler::screenHandleTablet(QTabletEvent* event)
{
    event->accept();

    switch(event->type())
    {
    case QEvent::TabletPress:
    case QEvent::TabletMove:
        {
            int x = event->x();
            int y = event->y();

            if (Frontend::GetTouchCoords(x, y, event->type()==QEvent::TabletMove))
            {
                touching = true;
                assert(emuThread->NDS != nullptr);
                emuThread->NDS->TouchScreen(x, y);
            }
        }
        break;
    case QEvent::TabletRelease:
        if (touching)
        {
            assert(emuThread->NDS != nullptr);
            emuThread->NDS->ReleaseScreen();
            touching = false;
        }
        break;
    default:
        break;
    }
}

void ScreenHandler::screenHandleTouch(QTouchEvent* event)
{
    event->accept();

    switch(event->type())
    {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
        if (event->touchPoints().length() > 0)
        {
            QPointF lastPosition = event->touchPoints().first().lastPos();
            int x = (int)lastPosition.x();
            int y = (int)lastPosition.y();

            if (Frontend::GetTouchCoords(x, y, event->type()==QEvent::TouchUpdate))
            {
                touching = true;
                assert(emuThread->NDS != nullptr);
                emuThread->NDS->TouchScreen(x, y);
            }
        }
        break;
    case QEvent::TouchEnd:
        if (touching)
        {
            assert(emuThread->NDS != nullptr);
            emuThread->NDS->ReleaseScreen();
            touching = false;
        }
        break;
    default:
        break;
    }
}

void ScreenHandler::showCursor()
{
    mainWindow->panelWidget->setCursor(Qt::ArrowCursor);
    mouseTimer->start();
}

QTimer* ScreenHandler::setupMouseTimer()
{
    mouseTimer = new QTimer();
    mouseTimer->setSingleShot(true);
    mouseTimer->setInterval(Config::MouseHideSeconds*1000);
    mouseTimer->start();

    return mouseTimer;
}

ScreenPanelNative::ScreenPanelNative(QWidget* parent) : QWidget(parent), ScreenHandler(this)
{
    screen[0] = QImage(256, 192, QImage::Format_RGB32);
    screen[1] = QImage(256, 192, QImage::Format_RGB32);

    screenTrans[0].reset();
    screenTrans[1].reset();

    OSD::Init(false);
}

ScreenPanelNative::~ScreenPanelNative()
{
    OSD::DeInit();
}

void ScreenPanelNative::setupScreenLayout()
{
    int w = width();
    int h = height();

    screenSetupLayout(w, h);

    for (int i = 0; i < numScreens; i++)
    {
        float* mtx = screenMatrix[i];
        screenTrans[i].setMatrix(mtx[0], mtx[1], 0.f,
                                mtx[2], mtx[3], 0.f,
                                mtx[4], mtx[5], 1.f);
    }
}

void ScreenPanelNative::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);

    // fill background
    painter.fillRect(event->rect(), QColor::fromRgb(0, 0, 0));

    if (emuThread->emuIsActive())
    {
        assert(emuThread->NDS != nullptr);
        emuThread->FrontBufferLock.lock();
        int frontbuf = emuThread->FrontBuffer;
        if (!emuThread->NDS->GPU.Framebuffer[frontbuf][0] || !emuThread->NDS->GPU.Framebuffer[frontbuf][1])
        {
            emuThread->FrontBufferLock.unlock();
            return;
        }

        memcpy(screen[0].scanLine(0), emuThread->NDS->GPU.Framebuffer[frontbuf][0].get(), 256 * 192 * 4);
        memcpy(screen[1].scanLine(0), emuThread->NDS->GPU.Framebuffer[frontbuf][1].get(), 256 * 192 * 4);
        emuThread->FrontBufferLock.unlock();

        QRect screenrc(0, 0, 256, 192);

        for (int i = 0; i < numScreens; i++)
        {
            painter.setTransform(screenTrans[i]);
            painter.drawImage(screenrc, screen[screenKind[i]]);
        }
    }

    OSD::Update();
    OSD::DrawNative(painter);
}

void ScreenPanelNative::resizeEvent(QResizeEvent* event)
{
    setupScreenLayout();
}

void ScreenPanelNative::mousePressEvent(QMouseEvent* event)
{
    screenOnMousePress(event);
}

void ScreenPanelNative::mouseReleaseEvent(QMouseEvent* event)
{
    screenOnMouseRelease(event);
}

void ScreenPanelNative::mouseMoveEvent(QMouseEvent* event)
{
    screenOnMouseMove(event);
}

void ScreenPanelNative::tabletEvent(QTabletEvent* event)
{
    screenHandleTablet(event);
}

bool ScreenPanelNative::event(QEvent* event)
{
    if (event->type() == QEvent::TouchBegin
        || event->type() == QEvent::TouchEnd
        || event->type() == QEvent::TouchUpdate)
    {
        screenHandleTouch((QTouchEvent*)event);
        return true;
    }
    return QWidget::event(event);
}

void ScreenPanelNative::onScreenLayoutChanged()
{
    setMinimumSize(screenGetMinSize());
    setupScreenLayout();
}


ScreenPanelGL::ScreenPanelGL(QWidget* parent) : QWidget(parent), ScreenHandler(this)
{
    setAutoFillBackground(false);
    setAttribute(Qt::WA_NativeWindow, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_PaintOnScreen, true);
    setAttribute(Qt::WA_KeyCompression, false);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(screenGetMinSize());
}

ScreenPanelGL::~ScreenPanelGL()
{}

bool ScreenPanelGL::createContext()
{
    std::optional<WindowInfo> windowInfo = getWindowInfo();
    std::array<GL::Context::Version, 2> versionsToTry = {
        GL::Context::Version{GL::Context::Profile::Core, 4, 3},
        GL::Context::Version{GL::Context::Profile::Core, 3, 2}};
    if (windowInfo.has_value())
    {
        glContext = GL::Context::Create(*getWindowInfo(), versionsToTry);
        glContext->DoneCurrent();
    }

    return glContext != nullptr;
}

void ScreenPanelGL::setSwapInterval(int intv)
{
    if (!glContext) return;

    glContext->SetSwapInterval(intv);
}

void ScreenPanelGL::initOpenGL()
{
    if (!glContext) return;

    glContext->MakeCurrent();

    OpenGL::BuildShaderProgram(kScreenVS, kScreenFS, screenShaderProgram, "ScreenShader");
    GLuint pid = screenShaderProgram[2];
    glBindAttribLocation(pid, 0, "vPosition");
    glBindAttribLocation(pid, 1, "vTexcoord");
    glBindFragDataLocation(pid, 0, "oColor");

    OpenGL::LinkShaderProgram(screenShaderProgram);

    glUseProgram(pid);
    glUniform1i(glGetUniformLocation(pid, "ScreenTex"), 0);

    screenShaderScreenSizeULoc = glGetUniformLocation(pid, "uScreenSize");
    screenShaderTransformULoc = glGetUniformLocation(pid, "uTransform");

    // to prevent bleeding between both parts of the screen
    // with bilinear filtering enabled
    const int paddedHeight = 192*2+2;
    const float padPixels = 1.f / paddedHeight;

    const float vertices[] =
    {
        0.f,   0.f,    0.f, 0.f,
        0.f,   192.f,  0.f, 0.5f - padPixels,
        256.f, 192.f,  1.f, 0.5f - padPixels,
        0.f,   0.f,    0.f, 0.f,
        256.f, 192.f,  1.f, 0.5f - padPixels,
        256.f, 0.f,    1.f, 0.f,

        0.f,   0.f,    0.f, 0.5f + padPixels,
        0.f,   192.f,  0.f, 1.f,
        256.f, 192.f,  1.f, 1.f,
        0.f,   0.f,    0.f, 0.5f + padPixels,
        256.f, 192.f,  1.f, 1.f,
        256.f, 0.f,    1.f, 0.5f + padPixels
    };

    glGenBuffers(1, &screenVertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, screenVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenVertexArrays(1, &screenVertexArray);
    glBindVertexArray(screenVertexArray);
    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*4, (void*)(0));
    glEnableVertexAttribArray(1); // texcoord
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*4, (void*)(2*4));

    glGenTextures(1, &screenTexture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, screenTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, paddedHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    // fill the padding
    u8 zeroData[256*4*4];
    memset(zeroData, 0, sizeof(zeroData));
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 192, 256, 2, GL_RGBA, GL_UNSIGNED_BYTE, zeroData);

    OSD::Init(true);

    glContext->SetSwapInterval(Config::ScreenVSync ? Config::ScreenVSyncInterval : 0);
    transferLayout();
}

void ScreenPanelGL::deinitOpenGL()
{
    if (!glContext) return;

    glDeleteTextures(1, &screenTexture);

    glDeleteVertexArrays(1, &screenVertexArray);
    glDeleteBuffers(1, &screenVertexBuffer);

    OpenGL::DeleteShaderProgram(screenShaderProgram);

    OSD::DeInit();

    glContext->DoneCurrent();

    lastScreenWidth = lastScreenHeight = -1;
}

void ScreenPanelGL::drawScreenGL()
{
    if (!glContext) return;
    if (!emuThread->NDS) return;

    int w = windowInfo.surface_width;
    int h = windowInfo.surface_height;
    float factor = windowInfo.surface_scale;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(false);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glClear(GL_COLOR_BUFFER_BIT);

    glViewport(0, 0, w, h);

    glUseProgram(screenShaderProgram[2]);
    glUniform2f(screenShaderScreenSizeULoc, w / factor, h / factor);

    int frontbuf = emuThread->FrontBuffer;
    glActiveTexture(GL_TEXTURE0);

#ifdef OGLRENDERER_ENABLED
    if (emuThread->NDS->GPU.GetRenderer3D().Accelerated)
    {
        // hardware-accelerated render
        static_cast<GLRenderer&>(emuThread->NDS->GPU.GetRenderer3D()).GetCompositor().BindOutputTexture(frontbuf);
    }
    else
#endif
    {
        // regular render
        glBindTexture(GL_TEXTURE_2D, screenTexture);

        if (emuThread->NDS->GPU.Framebuffer[frontbuf][0] && emuThread->NDS->GPU.Framebuffer[frontbuf][1])
        {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 192, GL_RGBA,
                            GL_UNSIGNED_BYTE, emuThread->NDS->GPU.Framebuffer[frontbuf][0].get());
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 192+2, 256, 192, GL_RGBA,
                            GL_UNSIGNED_BYTE, emuThread->NDS->GPU.Framebuffer[frontbuf][1].get());
        }
    }

    screenSettingsLock.lock();

    GLint filter = this->filter ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

    glBindBuffer(GL_ARRAY_BUFFER, screenVertexBuffer);
    glBindVertexArray(screenVertexArray);

    for (int i = 0; i < numScreens; i++)
    {
        glUniformMatrix2x3fv(screenShaderTransformULoc, 1, GL_TRUE, screenMatrix[i]);
        glDrawArrays(GL_TRIANGLES, screenKind[i] == 0 ? 0 : 2*3, 2*3);
    }

    screenSettingsLock.unlock();

    OSD::Update();
    OSD::DrawGL(w, h);

    glContext->SwapBuffers();
}

qreal ScreenPanelGL::devicePixelRatioFromScreen() const
{
    const QScreen* screen_for_ratio = window()->windowHandle()->screen();
    if (!screen_for_ratio)
        screen_for_ratio = QGuiApplication::primaryScreen();

    return screen_for_ratio ? screen_for_ratio->devicePixelRatio() : static_cast<qreal>(1);
}

int ScreenPanelGL::scaledWindowWidth() const
{
  return std::max(static_cast<int>(std::ceil(static_cast<qreal>(width()) * devicePixelRatioFromScreen())), 1);
}

int ScreenPanelGL::scaledWindowHeight() const
{
  return std::max(static_cast<int>(std::ceil(static_cast<qreal>(height()) * devicePixelRatioFromScreen())), 1);
}

std::optional<WindowInfo> ScreenPanelGL::getWindowInfo()
{
    WindowInfo wi;

    // Windows and Apple are easy here since there's no display connection.
    #if defined(_WIN32)
    wi.type = WindowInfo::Type::Win32;
    wi.window_handle = reinterpret_cast<void*>(winId());
    #elif defined(__APPLE__)
    wi.type = WindowInfo::Type::MacOS;
    wi.window_handle = reinterpret_cast<void*>(winId());
    #else
    QPlatformNativeInterface* pni = QGuiApplication::platformNativeInterface();
    const QString platform_name = QGuiApplication::platformName();
    if (platform_name == QStringLiteral("xcb"))
    {
        wi.type = WindowInfo::Type::X11;
        wi.display_connection = pni->nativeResourceForWindow("display", windowHandle());
        wi.window_handle = reinterpret_cast<void*>(winId());
    }
    else if (platform_name == QStringLiteral("wayland"))
    {
        wi.type = WindowInfo::Type::Wayland;
        QWindow* handle = windowHandle();
        if (handle == nullptr)
            return std::nullopt;

        wi.display_connection = pni->nativeResourceForWindow("display", handle);
        wi.window_handle = pni->nativeResourceForWindow("surface", handle);
    }
    else
    {
        qCritical() << "Unknown PNI platform " << platform_name;
        return std::nullopt;
    }
    #endif

    wi.surface_width = static_cast<u32>(scaledWindowWidth());
    wi.surface_height = static_cast<u32>(scaledWindowHeight());
    wi.surface_scale = static_cast<float>(devicePixelRatioFromScreen());

    return wi;
}


QPaintEngine* ScreenPanelGL::paintEngine() const
{
  return nullptr;
}

void ScreenPanelGL::setupScreenLayout()
{
    int w = width();
    int h = height();

    screenSetupLayout(w, h);
    transferLayout();
}

void ScreenPanelGL::resizeEvent(QResizeEvent* event)
{
    setupScreenLayout();

    QWidget::resizeEvent(event);
}

void ScreenPanelGL::mousePressEvent(QMouseEvent* event)
{
    screenOnMousePress(event);
}

void ScreenPanelGL::mouseReleaseEvent(QMouseEvent* event)
{
    screenOnMouseRelease(event);
}

void ScreenPanelGL::mouseMoveEvent(QMouseEvent* event)
{
    screenOnMouseMove(event);
}

void ScreenPanelGL::tabletEvent(QTabletEvent* event)
{
    screenHandleTablet(event);
}

bool ScreenPanelGL::event(QEvent* event)
{
    if (event->type() == QEvent::TouchBegin
        || event->type() == QEvent::TouchEnd
        || event->type() == QEvent::TouchUpdate)
    {
        screenHandleTouch((QTouchEvent*)event);
        return true;
    }
    return QWidget::event(event);
}

void ScreenPanelGL::transferLayout()
{
    std::optional<WindowInfo> windowInfo = getWindowInfo();
    if (windowInfo.has_value())
    {
        screenSettingsLock.lock();

        if (lastScreenWidth != windowInfo->surface_width || lastScreenHeight != windowInfo->surface_height)
        {
            if (glContext)
                glContext->ResizeSurface(windowInfo->surface_width, windowInfo->surface_height);
            lastScreenWidth = windowInfo->surface_width;
            lastScreenHeight = windowInfo->surface_height;
        }

        this->filter = Config::ScreenFilter;
        this->windowInfo = *windowInfo;

        screenSettingsLock.unlock();
    }
}

void ScreenPanelGL::onScreenLayoutChanged()
{
    setMinimumSize(screenGetMinSize());
    setupScreenLayout();
}
