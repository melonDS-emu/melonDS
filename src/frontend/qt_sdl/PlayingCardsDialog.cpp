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


PlayingCardsDialog::PlayingCardsDialog(QWidget* parent) : QDialog(parent), ui(new Ui::PlayingCardsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    QDir directory = QDir(QString(Config::PlayingCardsPath));
    bool enableDeckControls = this->processCardDirectory(QDir(directory));
    Deck.ControlsGroupBox->setEnabled(enableDeckControls);
    Hand.ControlsGroupBox->setEnabled(enableDeckControls);
    this->updateUI();
}

PlayingCardsDialog::~PlayingCardsDialog()
{
    closeDlg();
    delete ui;
}

bool PlayingCardsDialog::processCardDirectory(QDir directory)
{
    Deck = CardPile { .Cards = QList<QString>(), .Flipped = false, .TextLabel = ui->mainDeckCardsLabel,
        .ImageLabel = ui->mainDeckImageLabel, .ControlsGroupBox = ui->mainDeckControlsGroupBox };
    Hand = CardPile { .Cards = QList<QString>(), .Flipped = false, .TextLabel = ui->drawnDeckCardsLabel,
        .ImageLabel = ui->drawnDeckImageLabel, .ControlsGroupBox = ui->drawnDeckControlsGroupBox };
    Stacks = QList<CardPile>();

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

void PlayingCardsDialog::paintCard(CardPile *stack)
{
    if (stack->ImageLabel == nullptr) return;

    QPixmap pixmap = QPixmap();

    if (!stack->Cards.isEmpty())
    {
        QDir directory = QDir(QString(Config::PlayingCardsPath));
        QString currentCard = stack->Cards.first();
        stack->Flipped ? directory.cd("front") : directory.cd("back");

        QString filePath = directory.absoluteFilePath(currentCard);
        if (!pixmap.load(filePath))
        {
            QMessageBox::critical((QWidget*)this->parent(),
                "Failed to read an image",
                "The card image at " + filePath + "could not be read.\n"
                "It may be missing, access-restricted, or stored in an"
                "unsupported format.");
            return;
        }
        else
        {
            printf("PlayingCardsDialog: painting %s\n", filePath.toStdString().c_str());
        }
    }

    stack->ImageLabel->setPixmap(pixmap.scaled(stack->ImageLabel->width(), stack->ImageLabel->height(),
        Qt::KeepAspectRatio));
}

void PlayingCardsDialog::updateUI()
{
    bool enableDeckControls = !Deck.Cards.isEmpty();
    ui->mainDeckDrawPushButton->setEnabled(enableDeckControls);
    ui->mainDeckFlipPushButton->setEnabled(enableDeckControls);
    Deck.TextLabel->setText(QString::number(Deck.Cards.length()) + (Deck.Cards.length() > 1 ? " Cards" : " Card"));
    this->paintCard(&Deck);

    Hand.ControlsGroupBox->setEnabled(!Hand.Cards.isEmpty());
    ui->drawnDeckAddStackPushButton->setEnabled(Stacks.length() < MAX_STACKS);
    Hand.TextLabel->setText(QString::number(Hand.Cards.length()) + (Hand.Cards.length() > 1 ? " Cards" : " Card"));
    this->paintCard(&Hand);

    foreach (CardPile stack, Stacks)
    {
        if (stack.ControlsGroupBox != nullptr) stack.ControlsGroupBox->setEnabled(!stack.Cards.isEmpty());
        if (stack.TextLabel != nullptr) stack.TextLabel->setText(QString::number(stack.Cards.length()) + (stack.Cards.length() > 1 ? " Cards" : " Card"));
        this->paintCard(&stack);
    }
}

void PlayingCardsDialog::createStack(CardPile *stack)
{
    int row = Stacks.length() / 5 * 3;
    int col = Stacks.length() % 5;

    QLabel *textLabel = new QLabel();
    textLabel->setAlignment(Qt::AlignHCenter);
    ui->stacksLayout->addWidget(textLabel, row, col);

    QLabel *imageLabel = new QLabel();
    imageLabel->setAlignment(Qt::AlignHCenter);
    imageLabel->setFixedSize(ui->mainDeckImageLabel->size() * 0.75);
    imageLabel->setFrameStyle(QFrame::StyledPanel);
    ui->stacksLayout->addWidget(imageLabel, row + 1, col);

    QGroupBox *controlsGroupBox = new QGroupBox("Stack Controls");
    QGridLayout *controlsGroupBoxGridLayout = new QGridLayout();
    QPushButton *stackFlipButton = new QPushButton("Flip", controlsGroupBox);
    QObject::connect(stackFlipButton, SIGNAL(clicked()), this, SLOT(on_flip()));
    controlsGroupBoxGridLayout->addWidget(stackFlipButton, 0, 0, 1, 2);
    QPushButton *stackDrawButton = new QPushButton("Draw from Hand", controlsGroupBox);
    QObject::connect(stackDrawButton, SIGNAL(clicked()), this, SLOT(on_draw()));
    controlsGroupBoxGridLayout->addWidget(stackDrawButton, 1, 1);
    QPushButton *stackReturnButton = new QPushButton("Return to Hand", controlsGroupBox);
    QObject::connect(stackReturnButton, SIGNAL(clicked()), this, SLOT(on_return()));
    controlsGroupBoxGridLayout->addWidget(stackReturnButton, 1, 0);
    QPushButton *stackShuffleButton = new QPushButton("Shuffle", controlsGroupBox);
    QObject::connect(stackShuffleButton, SIGNAL(clicked()), this, SLOT(on_shuffle()));
    controlsGroupBoxGridLayout->addWidget(stackShuffleButton, 2, 0);
    QPushButton *stackRotateButton = new QPushButton("Rotate", controlsGroupBox);
    QObject::connect(stackRotateButton, SIGNAL(clicked()), this, SLOT(on_rotate()));
    controlsGroupBoxGridLayout->addWidget(stackRotateButton, 2, 1);
    controlsGroupBox->setLayout(controlsGroupBoxGridLayout);
    ui->stacksLayout->addWidget(controlsGroupBox, row + 2, col);

    stack->TextLabel = textLabel;
    stack->ImageLabel = imageLabel;
    stack->ControlsGroupBox = controlsGroupBox;
}

void PlayingCardsDialog::deleteStack(CardPile *stack)
{
    delete stack->TextLabel;
    delete stack->ImageLabel;
    delete stack->ControlsGroupBox;
}

void PlayingCardsDialog::on_browse()
{
    CardPile deckBackup = Deck;
    CardPile handBackup = Hand;
    QList<CardPile> stacksBackup = Stacks;

    QString currentDirectory = QString(Config::PlayingCardsPath);
    QString newDirectory = QFileDialog::getExistingDirectory(this,
                                                            "Select playing cards root directory...",
                                                            currentDirectory);

    if (!newDirectory.isEmpty() && QFileInfo(newDirectory) != QFileInfo(currentDirectory))
    {
        bool valid = this->processCardDirectory(QDir(newDirectory));

        if (valid)
        {
            strncpy(Config::PlayingCardsPath, newDirectory.toStdString().c_str(), 1023);
            Config::PlayingCardsPath[1023] = '\0';
            Config::Save();
            Deck.ControlsGroupBox->setEnabled(true);
        }
        else
        {
            // Restore the previously-loaded deck, if any.
            Deck = deckBackup;
            Hand = handBackup;
            Stacks = stacksBackup;
        }

        this->updateUI();
    }
}

void PlayingCardsDialog::on_draw()
{
    QObject* obj = sender()->parent();
    if (obj == Hand.ControlsGroupBox) // draw from the hand to a new stack
    {
        CardPile newStack = CardPile { .Cards = QList<QString>(), .Flipped = Hand.Flipped };
        newStack.Cards.append(Hand.Cards.takeFirst());
        this->createStack(&newStack);
        Stacks.append(newStack);
        Hand.Flipped = false;
    }
    else if (obj == Deck.ControlsGroupBox) // draw from the deck to the hand
    {
        Hand.Cards.prepend(Deck.Cards.takeFirst());
        Hand.Flipped = Deck.Flipped;
        Deck.Flipped = false;
    }
    else if (!Hand.Cards.isEmpty()) // draw from the hand to the specified stack
    {
        for (int i = 0; i < Stacks.length(); i++)
        {
            if (obj == Stacks[i].ControlsGroupBox)
            {
                Stacks[i].Cards.prepend(Hand.Cards.takeFirst());
                Stacks[i].Flipped = Hand.Flipped;
                Hand.Flipped = false;
            }
        }
    }

    this->updateUI();
}

void PlayingCardsDialog::on_flip()
{
    QObject* obj = sender()->parent();
    if (obj == Hand.ControlsGroupBox) Hand.Flipped = !Hand.Flipped;
    else if (obj == Deck.ControlsGroupBox) Deck.Flipped = !Deck.Flipped;
    else
    {
        for (int i = 0; i < Stacks.length(); i++)
        {
            if (obj == Stacks[i].ControlsGroupBox)
            {
                Stacks[i].Flipped = !Stacks[i].Flipped;
                break;
            }
        }
    }

    this->updateUI();
}

void PlayingCardsDialog::on_shuffle()
{
    srand(time(0)); // Generate a new "random" seed

    QObject* obj = sender()->parent();
    if (obj == Hand.ControlsGroupBox) // shuffle the hand only
    {
        std::random_shuffle(Hand.Cards.begin(), Hand.Cards.end());
        Hand.Flipped = false;
    }
    else if (obj == Deck.ControlsGroupBox) // reset the entire deck state
    {
        // Add all cards back to the main deck, clear other piles, and shuffle
        Deck.Cards.append(Hand.Cards);
        Hand.Cards.clear();
        Hand.Flipped = false;
        Deck.Flipped = false;
        while (Stacks.length() > 0)
        {
            Deck.Cards.append(Stacks[0].Cards);
            this->deleteStack(&Stacks[0]);
            Stacks.removeAt(0);
        }

        std::random_shuffle(Deck.Cards.begin(), Deck.Cards.end());
    }
    else // shuffle one stack only
    {
        for (int i = 0; i < Stacks.length(); i++)
        {
            if (obj == Stacks[i].ControlsGroupBox)
            {
                std::random_shuffle(Stacks[i].Cards.begin(), Stacks[i].Cards.end());
                Stacks[i].Flipped = false;
                break;
            }
        }
    }

    this->updateUI();
}

void PlayingCardsDialog::on_return()
{
    QObject* obj = sender()->parent();
    if (obj == Hand.ControlsGroupBox) // return a card from the hand to the deck
    {
        Deck.Cards.prepend(Hand.Cards.takeFirst());
        Deck.Flipped = Hand.Flipped;
        Hand.Flipped = false;
    }
    else // return a card from a stack to the hand
    {
        for (int i = 0; i < Stacks.length(); i++)
        {
            if (obj == Stacks[i].ControlsGroupBox)
            {
                Hand.Cards.prepend(Stacks[i].Cards.takeFirst()); // return the card to the hand
                Hand.Flipped = Stacks[i].Flipped;
                Stacks[i].Flipped = false;
                if (Stacks[i].Cards.isEmpty()) // delete the stack if the last card was returned
                {
                    this->deleteStack(&Stacks[i]);
                    Stacks.removeAt(i);
                }

                break;
            }
        }
    }

    this->updateUI();
}

void PlayingCardsDialog::on_rotate()
{
    QObject* obj = sender()->parent();
    if (obj == Hand.ControlsGroupBox) // rotate the hand
    {
        Hand.Cards.append(Hand.Cards.takeFirst());
        Hand.Flipped = false;
    }
    else // rotate a stack
    {
        for (int i = 0; i < Stacks.length(); i++)
        {
            if (obj == Stacks[i].ControlsGroupBox)
            {
                Stacks[i].Cards.append(Stacks[i].Cards.takeFirst());
                Stacks[i].Flipped = false;
            }
        }
    }

    this->updateUI();
}
