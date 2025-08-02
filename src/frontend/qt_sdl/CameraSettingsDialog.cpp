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

#include <stdio.h>
#include <QFileDialog>
#include <QPaintEvent>
#include <QPainter>

#include "types.h"
#include "main.h"

#include "CameraSettingsDialog.h"
#include "ui_CameraSettingsDialog.h"

using namespace melonDS;

CameraSettingsDialog* CameraSettingsDialog::currentDlg = nullptr;

extern CameraManager* camManager[2];


CameraPreviewPanel::CameraPreviewPanel(QWidget* parent) : QWidget(parent)
{
    currentCam = nullptr;
    updateTimer = startTimer(50);
}

CameraPreviewPanel::~CameraPreviewPanel()
{
    killTimer(updateTimer);
}

void CameraPreviewPanel::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);

    if (!currentCam)
    {
        painter.fillRect(event->rect(), QColor::fromRgb(0, 0, 0));
        return;
    }

    QImage picture(256, 192, QImage::Format_RGB32);
    currentCam->captureFrame((u32*)picture.bits(), 256, 192, false);
    painter.drawImage(0, 0, picture);
}


CameraSettingsDialog::CameraSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::CameraSettingsDialog)
{
    previewPanel = nullptr;
    currentCfg = nullptr;
    currentCam = nullptr;

    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    emuInstance = ((MainWindow*)parent)->getEmuInstance();

    for (int i = 0; i < 2; i++)
    {
        auto& cfg = camManager[i]->getConfig();

        oldCamSettings[i].InputType = cfg.GetInt("InputType");
        oldCamSettings[i].ImagePath = cfg.GetString("ImagePath");
        oldCamSettings[i].CamDeviceName = cfg.GetString("DeviceName");
        oldCamSettings[i].XFlip = cfg.GetBool("XFlip");
    }

    ui->cbCameraSel->addItem("DSi outer camera");
    ui->cbCameraSel->addItem("DSi inner camera");

#if QT_VERSION >= 0x060000
    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    for (const QCameraDevice &cameraInfo : cameras)
    {
        QString name = cameraInfo.description();
        QCameraDevice::Position pos = cameraInfo.position();
        if (pos != QCameraDevice::UnspecifiedPosition)
        {
            name += " (";
            if (pos == QCameraDevice::FrontFace)
                name += "inner camera";
            else if (pos == QCameraDevice::BackFace)
                name += "outer camera";
            name += ")";
        }

        ui->cbPhysicalCamera->addItem(name, QString(cameraInfo.id()));
    }
#else
    const QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
    for (const QCameraInfo &cameraInfo : cameras)
    {
        QString name = cameraInfo.description();
        QCamera::Position pos = cameraInfo.position();
        if (pos != QCamera::UnspecifiedPosition)
        {
            name += " (";
            if (pos == QCamera::FrontFace)
                name += "inner camera";
            else if (pos == QCamera::BackFace)
                name += "outer camera";
            name += ")";
        }

        ui->cbPhysicalCamera->addItem(name, cameraInfo.deviceName());
    }
#endif
    ui->rbPictureCamera->setEnabled(ui->cbPhysicalCamera->count() > 0);

    grpInputType = new QButtonGroup(this);
    grpInputType->addButton(ui->rbPictureNone,   0);
    grpInputType->addButton(ui->rbPictureImg,    1);
    grpInputType->addButton(ui->rbPictureCamera, 2);
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(grpInputType, SIGNAL(buttonClicked(int)), this, SLOT(onChangeInputType(int)));
#else
    connect(grpInputType, SIGNAL(idClicked(int)), this, SLOT(onChangeInputType(int)));
#endif

    previewPanel = new CameraPreviewPanel(this);
    QVBoxLayout* previewLayout = new QVBoxLayout();
    previewLayout->addWidget(previewPanel);
    ui->grpPreview->setLayout(previewLayout);
    previewPanel->setMinimumSize(256, 192);
    previewPanel->setMaximumSize(256, 192);

    on_cbCameraSel_currentIndexChanged(ui->cbCameraSel->currentIndex());
}

CameraSettingsDialog::~CameraSettingsDialog()
{
    delete ui;
}

void CameraSettingsDialog::on_CameraSettingsDialog_accepted()
{
    for (int i = 0; i < 2; i++)
    {
        camManager[i]->stop();
    }

    Config::Save();

    closeDlg();
}

void CameraSettingsDialog::on_CameraSettingsDialog_rejected()
{
    if (!((MainWindow*)parent())->getEmuInstance())
    {
        closeDlg();
        return;
    }

    for (int i = 0; i < 2; i++)
    {
        camManager[i]->stop();
        camManager[i]->deInit();

        auto& cfg = camManager[i]->getConfig();
        cfg.SetInt("InputType", oldCamSettings[i].InputType);
        cfg.SetString("ImagePath", oldCamSettings[i].ImagePath);
        cfg.SetString("DeviceName", oldCamSettings[i].CamDeviceName);
        cfg.SetBool("XFlip", oldCamSettings[i].XFlip);

        camManager[i]->init();
    }

    closeDlg();
}

void CameraSettingsDialog::on_cbCameraSel_currentIndexChanged(int id)
{
    if (!previewPanel) return;

    if (currentCam)
    {
        currentCam->stop();
    }

    currentId = id;
    currentCfg = &camManager[id]->getConfig();
    //currentCam = camManager[id];
    currentCam = nullptr;
    populateCamControls(id);
    currentCam = camManager[id];
    previewPanel->setCurrentCam(currentCam);

    currentCam->start();
}

void CameraSettingsDialog::onChangeInputType(int type)
{
    if (!currentCfg) return;

    if (currentCam)
    {
        currentCam->stop();
        currentCam->deInit();
    }

    currentCfg->SetInt("InputType", type);

    ui->txtSrcImagePath->setEnabled(type == 1);
    ui->btnSrcImageBrowse->setEnabled(type == 1);
    ui->cbPhysicalCamera->setEnabled((type == 2) && (ui->cbPhysicalCamera->count()>0));

    currentCfg->SetQString("ImagePath", ui->txtSrcImagePath->text());

    if (ui->cbPhysicalCamera->count() > 0)
        currentCfg->SetQString("DeviceName", ui->cbPhysicalCamera->currentData().toString());

    if (currentCam)
    {
        currentCam->init();
        currentCam->start();
    }
}

void CameraSettingsDialog::on_txtSrcImagePath_textChanged()
{
    if (!currentCfg) return;

    if (currentCam)
    {
        currentCam->stop();
        currentCam->deInit();
    }

    currentCfg->SetQString("ImagePath", ui->txtSrcImagePath->text());

    if (currentCam)
    {
        currentCam->init();
        currentCam->start();
    }
}

void CameraSettingsDialog::on_btnSrcImageBrowse_clicked()
{
    QString file = QFileDialog::getOpenFileName(this,
                                                "Select image file...",
                                                emuDirectory,
                                                "Image files (*.png *.jpg *.jpeg *.bmp);;Any file (*.*)");

    if (file.isEmpty()) return;

    ui->txtSrcImagePath->setText(file);
}

void CameraSettingsDialog::on_cbPhysicalCamera_currentIndexChanged(int id)
{
    if (!currentCfg) return;

    if (currentCam)
    {
        currentCam->stop();
        currentCam->deInit();
    }

    currentCfg->SetQString("DeviceName", ui->cbPhysicalCamera->itemData(id).toString());

    if (currentCam)
    {
        currentCam->init();
        currentCam->start();
    }
}

void CameraSettingsDialog::populateCamControls(int id)
{
    Config::Table& cfg = camManager[id]->getConfig();

    int type = cfg.GetInt("InputType");
    if (type < 0 || type >= grpInputType->buttons().count()) type = 0;
    grpInputType->button(type)->setChecked(true);

    ui->txtSrcImagePath->setText(cfg.GetQString("ImagePath"));

    bool deviceset = false;
    QString device = cfg.GetQString("DeviceName");
    for (int i = 0; i < ui->cbPhysicalCamera->count(); i++)
    {
        QString itemdev = ui->cbPhysicalCamera->itemData(i).toString();
        if (itemdev == device)
        {
            ui->cbPhysicalCamera->setCurrentIndex(i);
            deviceset = true;
            break;
        }
    }
    if (!deviceset)
        ui->cbPhysicalCamera->setCurrentIndex(0);

    onChangeInputType(type);

    ui->chkFlipPicture->setChecked(cfg.GetBool("XFlip"));
}

void CameraSettingsDialog::on_chkFlipPicture_clicked()
{
    if (!currentCfg) return;

    bool xflip = ui->chkFlipPicture->isChecked();
    currentCfg->SetBool("XFlip", xflip);
    if (currentCam) currentCam->setXFlip(xflip);
}
