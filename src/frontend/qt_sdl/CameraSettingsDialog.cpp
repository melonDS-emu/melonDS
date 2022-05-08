/*
    Copyright 2016-2022 melonDS team

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
#include "Platform.h"
#include "Config.h"

#include "CameraSettingsDialog.h"
#include "ui_CameraSettingsDialog.h"


CameraSettingsDialog* CameraSettingsDialog::currentDlg = nullptr;

extern std::string EmuDirectory;

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
    currentCam = nullptr;

    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->cbCameraSel->addItem("DSi outer camera");
    ui->cbCameraSel->addItem("DSi inner camera");

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

    grpInputType = new QButtonGroup(this);
    grpInputType->addButton(ui->rbPictureNone,   0);
    grpInputType->addButton(ui->rbPictureImg,    1);
    grpInputType->addButton(ui->rbPictureCamera, 2);
    connect(grpInputType, SIGNAL(buttonClicked(int)), this, SLOT(onChangeInputType(int)));

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
    //
    Config::Save();

    closeDlg();
}

void CameraSettingsDialog::on_CameraSettingsDialog_rejected()
{
    //

    closeDlg();
}

void CameraSettingsDialog::on_cbCameraSel_currentIndexChanged(int id)
{
    if (!previewPanel) return;

    if (currentCam)
    {
        currentCam->stop();
    }

    currentCam = camManager[id];
    previewPanel->setCurrentCam(currentCam);

    currentCam->start();
}

void CameraSettingsDialog::onChangeInputType(int type)
{
    printf("INPUT = %d\n", type);
}
