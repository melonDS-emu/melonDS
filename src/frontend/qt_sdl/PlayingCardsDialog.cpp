/*
    Copyright 2016-2020 Arisotura, Raphaël Zumer

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

#include "types.h"
#include "Platform.h"
#include "Config.h"
#include "PlatformConfig.h"

#include "PlayingCardsDialog.h"
#include "ui_PlayingCardsDialog.h"


PlayingCardsDialog *PlayingCardsDialog::currentDlg = nullptr;

QList<QString> CardDeck = QList<QString>();


PlayingCardsDialog::PlayingCardsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::PlayingCardsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    QDir directory = QDir(QString(Config::PlayingCardsPath));
    ui->controlGroup->setEnabled((this->processCardDirectory(QDir(directory))));
}

PlayingCardsDialog::~PlayingCardsDialog()
{
    closeDlg();
    delete ui;
}

bool PlayingCardsDialog::processCardDirectory(QDir directory)
{
    CardDeck.clear();
    QStringList image_filter;
    image_filter << "*.png" << "*.jpg" << "*.jpeg" << "*.gif" << "*.bmp";

    // Check that:
    // - the "front" and "back" directories exist
    // - the "front" directory contains at least 54 image files
    // - the "back" directory contains 54 files of the same names
    // Then build a list and return true if the directory is valid.
    bool res = true;
    if (directory.cd("front"))
    {
        QFileInfoList fileInfoList = directory.entryInfoList(image_filter, QDir::Files | QDir::NoDotAndDotDot);
        if (fileInfoList.length() >= 54)
        {
            // Take the first 54 image files in the directory and build card info based on them.
            for (int i = 0; i < 54; i++)
            {
                CardDeck.append(fileInfoList.takeFirst().fileName());
            }
        }
        else
        {
            QMessageBox::critical((QWidget*)this->parent(),
                "Invalid playing cards directory",
                "The \"front\" folder in the configured card directory must contain 54 images\n"
                "of a supported format (JPEG, PNG, GIF, or BMP).");
            res = false;
        }

        directory.cdUp();
    }
    else
    {
        QMessageBox::critical((QWidget*)this->parent(),
            "Invalid playing cards directory",
            "The card directory must contain folders named \"front\" and \"back\",\n"
            "each containing 54 playing card images in JPEG, PNG, GIF, or BMP format.");
        res = false;
    }

    if (res)
    {
        if (directory.cd("back"))
        {
            foreach (QString imageFile, CardDeck)
            {
                if (!directory.exists(imageFile))
                {
                    QMessageBox::critical((QWidget*)this->parent(),
                        "Invalid playing cards directory",
                        "The \"back\" folder in the configured card directory is missing an image\n"
                        "to match those found in the \"front\" folder (" + imageFile + ").");
                    res = false;
                }
            }

            directory.cdUp();
        }
        else
        {
            QMessageBox::critical((QWidget*)this->parent(),
                "Invalid playing cards directory",
                "The card directory must contain folders named \"front\" and \"back\",\n"
                "each containing 54 playing card images in JPEG, PNG, GIF, or BMP format.");
            res = false;
        }
    }

    return res;
}

void PlayingCardsDialog::on_browse()
{
    QString current_directory = QString(Config::PlayingCardsPath);
    QString new_directory = QFileDialog::getExistingDirectory(this,
                                                            "Select playing cards root directory...",
                                                            current_directory);

    if (new_directory != current_directory)
    {
        bool valid = this->processCardDirectory(QDir(new_directory));
        ui->controlGroup->setEnabled(valid);

        if (valid)
        {
            strncpy(Config::PlayingCardsPath, new_directory.toStdString().c_str(), 1023);
            Config::PlayingCardsPath[1023] = '\0';
            Config::Save();
        }
    }
}

void PlayingCardsDialog::on_draw()
{
    // TODO
}

void PlayingCardsDialog::on_flip()
{
    // TODO
}

void PlayingCardsDialog::on_shuffle()
{
    // TODO
}
