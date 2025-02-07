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

#include "AboutDialog.h"

#include <QDesktopServices>
#include <QUrl>

#include "ui_AboutDialog.h"

#include "version.h"

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent), ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    ui->lblVersionInfo->setText("Version " MELONDS_VERSION);
#ifdef MELONDS_EMBED_BUILD_INFO
    ui->lblBuildInfo->setText(
        "Branch: " MELONDS_GIT_BRANCH "\n"
        "Commit: " MELONDS_GIT_HASH "\n"
        "Built by: " MELONDS_BUILD_PROVIDER
    );
#else
    ui->lblBuildInfo->hide();
#endif
    adjustSize();
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

void AboutDialog::openWebsite()
{
    QDesktopServices::openUrl(QUrl(MELONDS_URL));
}

void AboutDialog::openGitHub()
{
    QDesktopServices::openUrl(QUrl("https://github.com/melonDS-emu/melonDS"));;
}
