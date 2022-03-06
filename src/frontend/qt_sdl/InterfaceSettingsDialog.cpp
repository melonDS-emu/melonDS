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

#include "InterfaceSettingsDialog.h"
#include "ui_InterfaceSettingsDialog.h"

#include "types.h"
#include "Platform.h"
#include "Config.h"

InterfaceSettingsDialog* InterfaceSettingsDialog::currentDlg = nullptr;

InterfaceSettingsDialog::InterfaceSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::InterfaceSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->cbMouseHide->setChecked(Config::MouseHide != 0);
    ui->spinMouseHideSeconds->setEnabled(Config::MouseHide != 0);
    ui->spinMouseHideSeconds->setValue(Config::MouseHideSeconds);
    ui->cbPauseLostFocus->setChecked(Config::PauseLostFocus != 0);
}

InterfaceSettingsDialog::~InterfaceSettingsDialog()
{
    delete ui;
}

void InterfaceSettingsDialog::on_cbMouseHide_clicked()
{
    if (ui->spinMouseHideSeconds->isEnabled())
    {
        ui->spinMouseHideSeconds->setEnabled(false);
    }
    else
    {
        ui->spinMouseHideSeconds->setEnabled(true);
    }
}

void InterfaceSettingsDialog::done(int r)
{
    if (r == QDialog::Accepted)
    {
        Config::MouseHide = ui->cbMouseHide->isChecked() ? 1:0;
        Config::MouseHideSeconds = ui->spinMouseHideSeconds->value();
        Config::PauseLostFocus = ui->cbPauseLostFocus->isChecked() ? 1:0;

        Config::Save();

        emit updateMouseTimer();
    }

    QDialog::done(r);

    closeDlg();
}
