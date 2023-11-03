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

#ifndef CAMERASETTINGSDIALOG_H
#define CAMERASETTINGSDIALOG_H

#include <QDialog>
#include <QButtonGroup>

#include "Config.h"
#include "CameraManager.h"

namespace Ui { class CameraSettingsDialog; }
class CameraSettingsDialog;

class CameraPreviewPanel : public QWidget
{
    Q_OBJECT

public:
    CameraPreviewPanel(QWidget* parent);
    ~CameraPreviewPanel();

    void setCurrentCam(CameraManager* cam)
    {
        currentCam = cam;
    }

protected:
    void paintEvent(QPaintEvent* event) override;
    void timerEvent(QTimerEvent* event) override
    {
        repaint();
    }

private:
    int updateTimer;
    CameraManager* currentCam;
};

class CameraSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CameraSettingsDialog(QWidget* parent);
    ~CameraSettingsDialog();

    static CameraSettingsDialog* currentDlg;
    static CameraSettingsDialog* openDlg(QWidget* parent)
    {
        if (currentDlg)
        {
            currentDlg->activateWindow();
            return currentDlg;
        }

        currentDlg = new CameraSettingsDialog(parent);
        currentDlg->open();
        return currentDlg;
    }
    static void closeDlg()
    {
        currentDlg = nullptr;
    }

private slots:
    void on_CameraSettingsDialog_accepted();
    void on_CameraSettingsDialog_rejected();

    void on_cbCameraSel_currentIndexChanged(int id);
    void onChangeInputType(int type);
    void on_txtSrcImagePath_textChanged();
    void on_btnSrcImageBrowse_clicked();
    void on_cbPhysicalCamera_currentIndexChanged(int id);
    void on_chkFlipPicture_clicked();

private:
    Ui::CameraSettingsDialog* ui;

    QButtonGroup* grpInputType;
    CameraPreviewPanel* previewPanel;

    int currentId;
    Config::CameraConfig* currentCfg;
    CameraManager* currentCam;

    Config::CameraConfig oldCamSettings[2];

    void populateCamControls(int id);
};

#endif // CAMERASETTINGSDIALOG_H
