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
#include <algorithm>
#include <ctime>
#include <QFileDialog>
#include <QImageReader>
#include <QMessageBox>
#include <QPixmap>

#include "types.h"
#include "Platform.h"
#include "Config.h"
#include "PlatformConfig.h"

#include "PlayingCardsDialog.h"
#include "ui_PlayingCardsDialog.h"


PlayingCardsDialog *PlayingCardsDialog::currentDlg = nullptr;

QList<QString> CardDeck;
QList<QString> DrawnCards;
bool CardIsFlipped;


PlayingCardsDialog::PlayingCardsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::PlayingCardsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    QDir directory = QDir(QString(Config::PlayingCardsPath));
    ui->controlGroup->setEnabled((this->processCardDirectory(QDir(directory))));
    this->paintCard();
}

PlayingCardsDialog::~PlayingCardsDialog()
{
    closeDlg();
    delete ui;
}

bool PlayingCardsDialog::processCardDirectory(QDir directory)
{
    CardDeck = QList<QString>();;
    DrawnCards = QList<QString>();
    CardIsFlipped = false;

    // Check that:
    // - the "front" and "back" directories exist
    // - the "front" directory contains at least 54 image files
    // - the "back" directory contains 54 files of the same names
    // Then build a list and return true if the directory is valid.
    bool res = true;
    if (directory.cd("front"))
    {
        QFileInfoList fileInfoList = directory.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        while (!fileInfoList.isEmpty())
        {
            QFileInfo fileInfo = fileInfoList.takeFirst();
            if (QImageReader(fileInfo.absoluteFilePath()).canRead()) CardDeck.append(fileInfo.fileName());
            if (CardDeck.length() == 54) break;
        }

        if (CardDeck.length() < 54)
        {
            QMessageBox::critical((QWidget*)this->parent(),
                "Invalid playing cards directory",
                "The \"front\" folder in the configured card directory must contain 54 images "
                "of a supported format.");
            res = false;
        }

        directory.cdUp();
    }
    else
    {
        QMessageBox::critical((QWidget*)this->parent(),
            "Invalid playing cards directory",
            "The card directory must contain folders named \"front\" and \"back\", "
            "each containing 54 playing card images in a supported format.");
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
                        "The \"back\" folder in the configured card directory is missing an image "
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
                "The card directory must contain folders named \"front\" and \"back\", "
                "each containing 54 playing card images in a supported format.");
            res = false;
        }
    }

    printf("PlayingCardsDialog: processed %d cards\n", CardDeck.length());
    return res;
}

void PlayingCardsDialog::paintCard()
{
    QPixmap pixmap = QPixmap();

    if (!CardDeck.isEmpty())
    {
        QDir directory = QDir(QString(Config::PlayingCardsPath));
        QString current_card = CardDeck.first();
        if (CardIsFlipped)
        {
            directory.cd("front");
        }
        else
        {
            directory.cd("back");
        }

        QString file_path = directory.absoluteFilePath(current_card);
        if (!pixmap.load(file_path))
        {
            QMessageBox::critical((QWidget*)this->parent(),
                "Failed to read an image",
                "The card image at " + file_path + "could not be read.\n"
                "It may be missing, access-restricted, or stored in an"
                "unsupported format.");
            return;
        }
        else
        {
            printf("PlayingCardsDialog: painting %s\n", file_path.toStdString().c_str());
        }
    }

    ui->playingCardLabel->setPixmap(pixmap.scaled(ui->playingCardLabel->width(),
                                ui->playingCardLabel->height(), Qt::KeepAspectRatio));
}

void PlayingCardsDialog::on_browse()
{
    QList<QString> deckBackup;

    if (!CardDeck.isEmpty() || !DrawnCards.isEmpty())
    {
        deckBackup.append(CardDeck);
        deckBackup.append(DrawnCards);
    }

    QString current_directory = QString(Config::PlayingCardsPath);
    QString new_directory = QFileDialog::getExistingDirectory(this,
                                                            "Select playing cards root directory...",
                                                            current_directory);

    if (!new_directory.isEmpty() && QFileInfo(new_directory) != QFileInfo(current_directory))
    {
        bool valid = this->processCardDirectory(QDir(new_directory));

        if (valid)
        {
            strncpy(Config::PlayingCardsPath, new_directory.toStdString().c_str(), 1023);
            Config::PlayingCardsPath[1023] = '\0';
            Config::Save();
            ui->controlGroup->setEnabled(true);
        }
        else if (!deckBackup.isEmpty())
        {
            // Restore the previously-loaded deck.
            CardDeck.append(deckBackup);
        }

        this->paintCard();
    }
}

void PlayingCardsDialog::on_draw()
{
    if (!CardDeck.isEmpty())
    {
        DrawnCards.append(CardDeck.takeFirst());
        CardIsFlipped = false;
        this->paintCard();
    }
}

void PlayingCardsDialog::on_flip()
{
    CardIsFlipped = !CardIsFlipped;
    this->paintCard();
}

void PlayingCardsDialog::on_shuffle()
{
    CardDeck.append(DrawnCards); // Add any drawn cards back to the main deck
    DrawnCards.clear(); // Clear duplicate drawn cards
    srand(time(0)); // Generate a new "random" seed
    std::random_shuffle(CardDeck.begin(), CardDeck.end()); // Shuffle the deck
    CardIsFlipped = false;
    this->paintCard();
}
