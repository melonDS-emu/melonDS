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

#include <cstdlib>

#include <QStyleFactory>
#include "InterfaceSettingsDialog.h"
#include "ui_InterfaceSettingsDialog.h"

#include "types.h"
#include "Platform.h"
#include "Config.h"
#include "main.h"

InterfaceSettingsDialog* InterfaceSettingsDialog::currentDlg = nullptr;
InterfaceSettingsDialog::InterfaceSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::InterfaceSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    emuInstance = ((MainWindow*)parent)->getEmuInstance();

    auto& cfg = emuInstance->getGlobalConfig();

    ui->cbMouseHide->setChecked(cfg.GetBool("Mouse.Hide"));
    ui->spinMouseHideSeconds->setEnabled(ui->cbMouseHide->isChecked());
    ui->spinMouseHideSeconds->setValue(cfg.GetInt("Mouse.HideSeconds"));
    ui->cbPauseLostFocus->setChecked(cfg.GetBool("PauseLostFocus"));
    ui->cbFastForwardVolume->setChecked(cfg.GetBool("MuteFastForward"));
    ui->slFastForwardVolume->setValue(cfg.GetInt("FastForwardVolume"));
    ui->lblFastForwardVolume->setText(QString("%1%").arg(cfg.GetInt("FastForwardVolume")));
    ui->slFastForwardVolume->setEnabled(ui->cbFastForwardVolume->isChecked());
    ui->lblFastForwardVolume->setEnabled(ui->cbFastForwardVolume->isChecked());

    ui->cbSlowmoVolume->setChecked(cfg.GetBool("MuteSlowmo"));
    ui->slSlowmoVolume->setValue(cfg.GetInt("SlowmoVolume"));
    ui->lblSlowmoVolume->setText(QString("%1%").arg(cfg.GetInt("SlowmoVolume")));
    ui->slSlowmoVolume->setEnabled(ui->cbSlowmoVolume->isChecked());
    ui->lblSlowmoVolume->setEnabled(ui->cbSlowmoVolume->isChecked());
    ui->spinTargetFPS->setValue(cfg.GetDouble("TargetFPS"));
    ui->spinFFW->setValue(cfg.GetDouble("FastForwardFPS"));
    ui->spinSlow->setValue(cfg.GetDouble("SlowmoFPS"));

    const QList<QString> themeKeys = QStyleFactory::keys();
    const QString currentTheme = qApp->style()->objectName();
    QString cfgTheme = cfg.GetQString("UITheme");

    ui->cbxUITheme->addItem("System default", "");

    for (int i = 0; i < themeKeys.length(); i++)
    {
        ui->cbxUITheme->addItem(themeKeys[i], themeKeys[i]);
        if (!cfgTheme.isEmpty() && themeKeys[i].compare(currentTheme, Qt::CaseInsensitive) == 0)
            ui->cbxUITheme->setCurrentIndex(i + 1);
    }
}

InterfaceSettingsDialog::~InterfaceSettingsDialog()
{
    delete ui;
}

void InterfaceSettingsDialog::on_cbMouseHide_clicked()
{
    ui->spinMouseHideSeconds->setEnabled(ui->cbMouseHide->isChecked());
}

void InterfaceSettingsDialog::on_pbClean_clicked()
{
    ui->spinTargetFPS->setValue(60.0000);
}

void InterfaceSettingsDialog::on_pbAccurate_clicked()
{
    ui->spinTargetFPS->setValue(59.8261);
}

void InterfaceSettingsDialog::on_pb2x_clicked()
{
    ui->spinFFW->setValue(ui->spinTargetFPS->value() * 2.0);
}

void InterfaceSettingsDialog::on_pb3x_clicked()
{
    ui->spinFFW->setValue(ui->spinTargetFPS->value() * 3.0);
}

void InterfaceSettingsDialog::on_pbMAX_clicked()
{
    ui->spinFFW->setValue(1000.0);
}

void InterfaceSettingsDialog::on_pbHalf_clicked()
{
    ui->spinSlow->setValue(ui->spinTargetFPS->value() / 2.0);
}

void InterfaceSettingsDialog::on_pbQuarter_clicked()
{
    ui->spinSlow->setValue(ui->spinTargetFPS->value() / 4.0);
}

void InterfaceSettingsDialog::on_cbFastForwardVolume_clicked()
{
    ui->slFastForwardVolume->setEnabled(ui->cbFastForwardVolume->isChecked());
    ui->lblFastForwardVolume->setEnabled(ui->cbFastForwardVolume->isChecked());
}

void InterfaceSettingsDialog::on_cbSlowmoVolume_clicked()
{
    ui->slSlowmoVolume->setEnabled(ui->cbSlowmoVolume->isChecked());
    ui->lblSlowmoVolume->setEnabled(ui->cbSlowmoVolume->isChecked());
}

// pull a dragged slider onto a nearby tick. setValue() re-enters this while the
// handle is held, but the second pass is a no-op since the value already matches.
// only dragging snaps, so the arrow keys can still reach any value
static void snapSpeedVolume(QSlider* slider, int val)
{
    for (int tick = 0; tick <= slider->maximum(); tick += slider->tickInterval())
    {
        if (std::abs(val - tick) <= 4)
        {
            slider->setValue(tick);
            return;
        }
    }

    slider->setValue(val);
}

void InterfaceSettingsDialog::on_slFastForwardVolume_sliderMoved(int val)
{
    snapSpeedVolume(ui->slFastForwardVolume, val);
}

void InterfaceSettingsDialog::on_slSlowmoVolume_sliderMoved(int val)
{
    snapSpeedVolume(ui->slSlowmoVolume, val);
}

void InterfaceSettingsDialog::on_slFastForwardVolume_valueChanged(int val)
{
    ui->lblFastForwardVolume->setText(QString("%1%").arg(val));
}

void InterfaceSettingsDialog::on_slSlowmoVolume_valueChanged(int val)
{
    ui->lblSlowmoVolume->setText(QString("%1%").arg(val));
}

void InterfaceSettingsDialog::done(int r)
{
    if (!((MainWindow*)parent())->getEmuInstance())
    {
        QDialog::done(r);
        closeDlg();
        return;
    }

    if (r == QDialog::Accepted)
    {
        auto& cfg = emuInstance->getGlobalConfig();

        cfg.SetBool("Mouse.Hide", ui->cbMouseHide->isChecked());
        cfg.SetInt("Mouse.HideSeconds", ui->spinMouseHideSeconds->value());
        cfg.SetBool("PauseLostFocus", ui->cbPauseLostFocus->isChecked());
        cfg.SetBool("MuteFastForward", ui->cbFastForwardVolume->isChecked());
        cfg.SetInt("FastForwardVolume", ui->slFastForwardVolume->value());
        cfg.SetBool("MuteSlowmo", ui->cbSlowmoVolume->isChecked());
        cfg.SetInt("SlowmoVolume", ui->slSlowmoVolume->value());

        double val = ui->spinTargetFPS->value();
        if (val == 0.0) cfg.SetDouble("TargetFPS", 0.0001);
        else cfg.SetDouble("TargetFPS", val);
        
        val = ui->spinFFW->value();
        if (val == 0.0) cfg.SetDouble("FastForwardFPS", 0.0001);
        else cfg.SetDouble("FastForwardFPS", val);
        
        val = ui->spinSlow->value();
        if (val == 0.0) cfg.SetDouble("SlowmoFPS", 0.0001);
        else cfg.SetDouble("SlowmoFPS", val);

        QString themeName = ui->cbxUITheme->currentData().toString();
        cfg.SetQString("UITheme", themeName);

        Config::Save();

        if (!themeName.isEmpty())
            qApp->setStyle(themeName);
        else
            qApp->setStyle(*systemThemeName);

        emit updateInterfaceSettings();
    }

    QDialog::done(r);

    closeDlg();
}
