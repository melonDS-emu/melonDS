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

#include <stdio.h>
#include <QFileDialog>
#include <QMessageBox>
#include <QTemporaryFile>

#include "types.h"
#include "Config.h"
#include "Platform.h"
#include "main.h"

#include "RichPresenceSettingsDialog.h"
#include "ui_RichPresenceSettingsDialog.h"

using namespace melonDS::Platform;
namespace Platform = melonDS::Platform;

RichPresenceSettingsDialog* RichPresenceSettingsDialog::currentDlg = nullptr;

RichPresenceSettingsDialog::RichPresenceSettingsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::RichPresenceSettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    emuInstance = ((MainWindow*)parent)->getEmuInstance();

    auto& cfg = emuInstance->getLocalConfig();
    ui->btnEnableRPC->setChecked(cfg.GetBool("EnableRichPresence"));

#undef SET_ORIGVAL
}

RichPresenceSettingsDialog::~RichPresenceSettingsDialog()
{
    delete ui;
}

void RichPresenceSettingsDialog::done(int r)
{
    if (!((MainWindow*)parent())->getEmuInstance())
    {
        QDialog::done(r);
        closeDlg();
        return;
    }

    if (r == QDialog::Accepted)
    {
        auto& cfg = emuInstance->getLocalConfig();
        cfg.SetBool("EnableRichPresence", ui->btnEnableRPC->isChecked());

        Config::Save();
    }

    QDialog::done(r);

    closeDlg();
}