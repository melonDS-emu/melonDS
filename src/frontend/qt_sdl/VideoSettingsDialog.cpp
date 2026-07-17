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

#include <QFileDialog>
#include <QtGlobal>

#include "types.h"
#include "Platform.h"
#include "Config.h"
#include "GPU.h"
#include "main.h"
#ifdef VKRENDERER_ENABLED
#include "VulkanSupport.h"
#endif

#include "VideoSettingsDialog.h"
#include "ui_VideoSettingsDialog.h"


inline bool VideoSettingsDialog::UsesGL()
{
    auto& cfg = emuInstance->getGlobalConfig();
    return cfg.GetBool("Screen.UseGL") || (cfg.GetInt("3D.Renderer") != renderer3D_Software);
}

VideoSettingsDialog* VideoSettingsDialog::currentDlg = nullptr;

void VideoSettingsDialog::setEnabled()
{
    auto& cfg = emuInstance->getGlobalConfig();
    int renderer = cfg.GetInt("3D.Renderer");

    bool softwareRenderer = renderer == renderer3D_Software;
    ui->cbGLDisplay->setEnabled(softwareRenderer);
    ui->cbSoftwareThreaded->setEnabled(softwareRenderer);
    ui->cbxGLResolution->setEnabled(!softwareRenderer);
    ui->cbBetterPolygons->setEnabled(renderer == renderer3D_OpenGL);
    bool computeRenderer = renderer == renderer3D_OpenGLCompute;
#ifdef VKRENDERER_ENABLED
    computeRenderer |= renderer == renderer3D_VulkanCompute;
    computeRenderer |= renderer == renderer3D_VulkanFull;
#endif
    ui->cbxComputeHiResCoords->setEnabled(computeRenderer);

    // the dither/texture-filter enhancements are implemented in the full
    // Vulkan renderer only
    bool vulkanRenderer = false;
#ifdef VKRENDERER_ENABLED
    vulkanRenderer = renderer == renderer3D_VulkanFull;
#endif
    ui->cbDither->setEnabled(vulkanRenderer);
    ui->cbTexFilter->setEnabled(vulkanRenderer);
}

VideoSettingsDialog::VideoSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::VideoSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    emuInstance = ((MainWindow*)parent)->getEmuInstance();

    auto& cfg = emuInstance->getGlobalConfig();
    oldRenderer = cfg.GetInt("3D.Renderer");
    oldGLDisplay = cfg.GetBool("Screen.UseGL");
    oldVSync = cfg.GetBool("Screen.VSync");
    oldVSyncInterval = cfg.GetInt("Screen.VSyncInterval");
    oldSoftThreaded = cfg.GetBool("3D.Soft.Threaded");
    oldGLScale = cfg.GetInt("3D.GL.ScaleFactor");
    oldGLBetterPolygons = cfg.GetBool("3D.GL.BetterPolygons");
    oldHiresCoordinates = cfg.GetBool("3D.GL.HiresCoordinates");
    oldDither = cfg.GetBool("3D.GL.Dither");
    oldTexFilter = cfg.GetBool("3D.GL.TexFilter");

    grp3DRenderer = new QButtonGroup(this);
    grp3DRenderer->addButton(ui->rb3DSoftware, renderer3D_Software);
    grp3DRenderer->addButton(ui->rb3DOpenGL,   renderer3D_OpenGL);
    grp3DRenderer->addButton(ui->rb3DCompute,  renderer3D_OpenGLCompute);
#ifdef VKRENDERER_ENABLED
    grp3DRenderer->addButton(ui->rb3DVulkanFull, renderer3D_VulkanFull);
#else
    ui->rb3DVulkanFull->hide();
#endif
    // the hybrid renderer (Vulkan 3D + OpenGL 2D) was a stepping stone to
    // the full Vulkan renderer, which supersedes it; keep it working for a
    // config that still selects it, but drop it from the menu
    ui->rb3DVulkan->hide();
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(grp3DRenderer, SIGNAL(buttonClicked(int)), this, SLOT(onChange3DRenderer(int)));
#else
    connect(grp3DRenderer, SIGNAL(idClicked(int)), this, SLOT(onChange3DRenderer(int)));
#endif
    if (auto* btn = grp3DRenderer->button(oldRenderer))
        btn->setChecked(true);

#ifndef OGLRENDERER_ENABLED
    ui->rb3DOpenGL->setEnabled(false);
#endif

#ifdef __APPLE__
    // macOS caps OpenGL below 4.3, so the GL compute renderer can never work there
    ui->rb3DCompute->setEnabled(false);
    ui->rb3DCompute->setToolTip(tr("The OpenGL compute renderer requires OpenGL 4.3, which macOS does not support.\nUse the Vulkan renderer instead."));
#endif

#ifdef VKRENDERER_ENABLED
    if (!melonDS::VK::IsRuntimeAvailable())
    {
        ui->rb3DVulkanFull->setEnabled(false);
        ui->rb3DVulkanFull->setToolTip(tr("No Vulkan runtime was found on this system."));
    }
#endif

    ui->cbGLDisplay->setChecked(oldGLDisplay != 0);

    ui->cbVSync->setChecked(oldVSync != 0);
    ui->sbVSyncInterval->setValue(oldVSyncInterval);

    ui->cbSoftwareThreaded->setChecked(oldSoftThreaded);

    for (int i = 1; i <= 16; i++)
        ui->cbxGLResolution->addItem(QString("%1x native (%2x%3)").arg(i).arg(256*i).arg(192*i));
    ui->cbxGLResolution->setCurrentIndex(oldGLScale-1);

    ui->cbBetterPolygons->setChecked(oldGLBetterPolygons != 0);
    ui->cbxComputeHiResCoords->setChecked(oldHiresCoordinates != 0);
    ui->cbDither->setChecked(oldDither != 0);
    ui->cbTexFilter->setChecked(oldTexFilter != 0);

    if (!oldVSync)
        ui->sbVSyncInterval->setEnabled(false);
    setVsyncControlEnable(UsesGL());

    setEnabled();
}

VideoSettingsDialog::~VideoSettingsDialog()
{
    delete ui;
}

void VideoSettingsDialog::on_VideoSettingsDialog_accepted()
{
    Config::Save();

    closeDlg();
}

void VideoSettingsDialog::on_VideoSettingsDialog_rejected()
{
    if (!((MainWindow*)parent())->getEmuInstance())
    {
        closeDlg();
        return;
    }

    bool old_gl = UsesGL();

    auto& cfg = emuInstance->getGlobalConfig();
    cfg.SetInt("3D.Renderer", oldRenderer);
    cfg.SetBool("Screen.UseGL", oldGLDisplay);
    cfg.SetBool("Screen.VSync", oldVSync);
    cfg.SetInt("Screen.VSyncInterval", oldVSyncInterval);
    cfg.SetBool("3D.Soft.Threaded", oldSoftThreaded);
    cfg.SetInt("3D.GL.ScaleFactor", oldGLScale);
    cfg.SetBool("3D.GL.BetterPolygons", oldGLBetterPolygons);
    cfg.SetBool("3D.GL.HiresCoordinates", oldHiresCoordinates);
    cfg.SetBool("3D.GL.Dither", oldDither);
    cfg.SetBool("3D.GL.TexFilter", oldTexFilter);

    emit updateVideoSettings(old_gl != UsesGL());

    closeDlg();
}

void VideoSettingsDialog::setVsyncControlEnable(bool hasOGL)
{
    ui->cbVSync->setEnabled(hasOGL);
    ui->sbVSyncInterval->setEnabled(hasOGL);
}

void VideoSettingsDialog::onChange3DRenderer(int renderer)
{
    bool old_gl = UsesGL();

    auto& cfg = emuInstance->getGlobalConfig();
    cfg.SetInt("3D.Renderer", renderer);

    setEnabled();

    emit updateVideoSettings(old_gl != UsesGL());
}

void VideoSettingsDialog::on_cbGLDisplay_stateChanged(int state)
{
    bool old_gl = UsesGL();

    auto& cfg = emuInstance->getGlobalConfig();
    cfg.SetBool("Screen.UseGL", (state != 0));

    setVsyncControlEnable(UsesGL());

    emit updateVideoSettings(old_gl != UsesGL());
}

void VideoSettingsDialog::on_cbVSync_stateChanged(int state)
{
    bool vsync = (state != 0);
    ui->sbVSyncInterval->setEnabled(vsync);

    auto& cfg = emuInstance->getGlobalConfig();
    cfg.SetBool("Screen.VSync", vsync);

    emit updateVideoSettings(false);
}

void VideoSettingsDialog::on_sbVSyncInterval_valueChanged(int val)
{
    auto& cfg = emuInstance->getGlobalConfig();
    cfg.SetInt("Screen.VSyncInterval", val);

    emit updateVideoSettings(false);
}

void VideoSettingsDialog::on_cbSoftwareThreaded_stateChanged(int state)
{
    auto& cfg = emuInstance->getGlobalConfig();
    cfg.SetBool("3D.Soft.Threaded", (state != 0));

    emit updateVideoSettings(false);
}

void VideoSettingsDialog::on_cbxGLResolution_currentIndexChanged(int idx)
{
    // prevent a spurious change
    if (ui->cbxGLResolution->count() < 16) return;

    auto& cfg = emuInstance->getGlobalConfig();
    cfg.SetInt("3D.GL.ScaleFactor", idx+1);

    setVsyncControlEnable(UsesGL());

    emit updateVideoSettings(false);
}

void VideoSettingsDialog::on_cbBetterPolygons_stateChanged(int state)
{
    auto& cfg = emuInstance->getGlobalConfig();
    cfg.SetBool("3D.GL.BetterPolygons", (state != 0));

    emit updateVideoSettings(false);
}

void VideoSettingsDialog::on_cbxComputeHiResCoords_stateChanged(int state)
{
    auto& cfg = emuInstance->getGlobalConfig();
    cfg.SetBool("3D.GL.HiresCoordinates", (state != 0));

    emit updateVideoSettings(false);
}

void VideoSettingsDialog::on_cbDither_stateChanged(int state)
{
    auto& cfg = emuInstance->getGlobalConfig();
    cfg.SetBool("3D.GL.Dither", (state != 0));

    emit updateVideoSettings(false);
}

void VideoSettingsDialog::on_cbTexFilter_stateChanged(int state)
{
    auto& cfg = emuInstance->getGlobalConfig();
    cfg.SetBool("3D.GL.TexFilter", (state != 0));

    emit updateVideoSettings(false);
}
