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


CardPile Deck;
CardPile Hand;
CardPile Discard;


PlayingCardsDialog::PlayingCardsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::PlayingCardsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    QDir directory = QDir(QString(Config::PlayingCardsPath));
    bool enableDeckControls = this->processCardDirectory(QDir(directory));
    ui->mainDeckControlsGroupBox->setEnabled(enableDeckControls);
    ui->drawnDeckControlsGroupBox->setEnabled(enableDeckControls);
    this->recountCards();
    this->paintAllCards();
}

PlayingCardsDialog::~PlayingCardsDialog()
{
    closeDlg();
    delete ui;
}

bool PlayingCardsDialog::processCardDirectory(QDir directory)
{
    Deck = CardPile { .Cards = QList<QString>(), .Flipped = false, .Label = ui->mainDeckImageLabel };
    Hand = CardPile { .Cards = QList<QString>(), .Flipped = false, .Label = ui->drawnDeckImageLabel };
    Discard = CardPile { .Cards = QList<QString>(), .Flipped = false, .Label = nullptr };

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
            if (QImageReader(fileInfo.absoluteFilePath()).canRead()) Deck.Cards.append(fileInfo.fileName());
            if (Deck.Cards.length() == 54) break;
        }

        if (Deck.Cards.length() < 54)
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
            foreach (QString imageFile, Deck.Cards)
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

    printf("PlayingCardsDialog: processed %d cards\n", Deck.Cards.length());
    return res;
}

void PlayingCardsDialog::paintCard(CardPile pile)
{
    if (pile.Label == nullptr) return;

    QPixmap pixmap = QPixmap();

    if (!pile.Cards.isEmpty())
    {
        QDir directory = QDir(QString(Config::PlayingCardsPath));
        QString current_card = pile.Cards.first();
        if (pile.Flipped)
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

    pile.Label->setPixmap(pixmap.scaled(pile.Label->width(), pile.Label->height(), Qt::KeepAspectRatio));
}

void PlayingCardsDialog::paintAllCards()
{
    this->paintCard(Deck);
    this->paintCard(Hand);
}

void PlayingCardsDialog::recountCards()
{
    bool enableDeckControls = !Deck.Cards.isEmpty();
    ui->mainDeckDrawPushButton->setEnabled(enableDeckControls);
    ui->mainDeckFlipPushButton->setEnabled(enableDeckControls);

    bool enableHandControls = !Hand.Cards.isEmpty();
    ui->drawnDeckControlsGroupBox->setEnabled(enableHandControls);

    ui->mainDeckCardsLabel->setText(QString::number(Deck.Cards.length()) + " Cards");
    ui->drawnDeckCardsLabel->setText(QString::number(Hand.Cards.length()) + " Cards");
}

void PlayingCardsDialog::on_browse()
{
    CardPile deckBackup = Deck;
    CardPile handBackup = Hand;
    CardPile discardBackup = Discard;

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
            ui->mainDeckControlsGroupBox->setEnabled(true);
            this->recountCards();
        }
        else
        {
            // Restore the previously-loaded deck, if any.
            Deck = deckBackup;
            Hand = handBackup;
            Discard = discardBackup;
            this->recountCards();
        }

        this->paintAllCards();
    }
}

void PlayingCardsDialog::on_draw()
{
    Hand.Cards.prepend(Deck.Cards.takeFirst());
    this->recountCards();
    Hand.Flipped = Deck.Flipped;
    Deck.Flipped = false;
    this->paintAllCards();
}

void PlayingCardsDialog::on_flip()
{
    QObject* obj = sender();
    if (obj->parent() == ui->drawnDeckControlsGroupBox) Hand.Flipped = !Hand.Flipped;
    else if (obj->parent() == ui->mainDeckControlsGroupBox) Deck.Flipped = !Deck.Flipped;

    this->paintAllCards();
}

void PlayingCardsDialog::on_shuffle()
{
    srand(time(0)); // Generate a new "random" seed

    QObject* obj = sender();
    if (obj->parent() == ui->drawnDeckControlsGroupBox)
    {
        std::random_shuffle(Hand.Cards.begin(), Hand.Cards.end());
    }
    else if (obj->parent() == ui->mainDeckControlsGroupBox)
    {
        // Add all cards back to the main deck, clear other piles, and shuffle
        Deck.Cards.append(Hand.Cards);
        Deck.Cards.append(Discard.Cards);
        Hand.Cards.clear();
        Discard.Cards.clear();
        std::random_shuffle(Deck.Cards.begin(), Deck.Cards.end());
        this->recountCards();
    }

    Deck.Flipped = false;
    Hand.Flipped = false;
    this->paintAllCards();
}

void PlayingCardsDialog::on_return()
{
    Deck.Cards.append(Hand.Cards.takeFirst());
    this->recountCards();
    Hand.Flipped = false;
    this->paintAllCards();
}

void PlayingCardsDialog::on_discard()
{
    Discard.Cards.append(Hand.Cards.takeFirst());
    this->recountCards();
    Hand.Flipped = false;
    this->paintAllCards();
}

void PlayingCardsDialog::on_rotate()
{
    Hand.Cards.append(Hand.Cards.takeFirst());
    Hand.Flipped = false;
    this->paintAllCards();
}
