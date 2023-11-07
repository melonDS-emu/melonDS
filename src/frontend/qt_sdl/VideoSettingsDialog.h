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

#ifndef VIDEOSETTINGSDIALOG_H
#define VIDEOSETTINGSDIALOG_H

#include <QDialog>
#include <QButtonGroup>

namespace Ui { class VideoSettingsDialog; }
class VideoSettingsDialog;

class VideoSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VideoSettingsDialog(QWidget* parent);
    ~VideoSettingsDialog();

    static VideoSettingsDialog* currentDlg;
    static VideoSettingsDialog* openDlg(QWidget* parent)
    {
        if (currentDlg)
        {
            currentDlg->activateWindow();
            return currentDlg;
        }

        currentDlg = new VideoSettingsDialog(parent);
        currentDlg->show();
        return currentDlg;
    }
    static void closeDlg()
    {
        currentDlg = nullptr;
    }

signals:
    void updateVideoSettings(bool glchange);

private slots:
    void on_VideoSettingsDialog_accepted();
    void on_VideoSettingsDialog_rejected();

    void onChange3DRenderer(int renderer);
    void on_cbGLDisplay_stateChanged(int state);
    void on_cbVSync_stateChanged(int state);
    void on_sbVSyncInterval_valueChanged(int val);

    void on_cbxGLResolution_currentIndexChanged(int idx);
    void on_cbBetterPolygons_stateChanged(int state);

    void on_cbSoftwareThreaded_stateChanged(int state);
private:
    void setVsyncControlEnable(bool hasOGL);

    Ui::VideoSettingsDialog* ui;

    QButtonGroup* grp3DRenderer;

    int oldRenderer;
    int oldGLDisplay;
    int oldVSync;
    int oldVSyncInterval;
    int oldSoftThreaded;
    int oldGLScale;
    int oldGLBetterPolygons;
};

#endif // VIDEOSETTINGSDIALOG_H

