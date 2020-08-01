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

#ifndef PLAYINGCARDSDIALOG_H
#define PLAYINGCARDSDIALOG_H

#include <QDialog>
#include <QDir>

#include "types.h"

namespace Ui { class PlayingCardsDialog; }
class PlayingCardsDialog;

class PlayingCardsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PlayingCardsDialog(QWidget* parent);
    ~PlayingCardsDialog();

    static PlayingCardsDialog* currentDlg;
    static PlayingCardsDialog* openDlg(QWidget* parent)
    {
        if (currentDlg)
        {
            currentDlg->activateWindow();
            return currentDlg;
        }

        currentDlg = new PlayingCardsDialog(parent);
        currentDlg->show();
        return currentDlg;
    }
    static void closeDlg()
    {
        currentDlg = nullptr;
    }

    static bool supportsCart(char gameCode[5])
    {
        const char PLAYING_CARDS_GAMECODES[3][5] =
        {
            "AJQJ", // Magic Taizen (Japan)
            "AJQE", // Master of Illusion (USA)
            "AJQP", // Magic Made Fun - Perform Tricks That Will Amaze Your Friends! (Europe)
        };

        for (int i = 0; i < sizeof(PLAYING_CARDS_GAMECODES)/sizeof(PLAYING_CARDS_GAMECODES[0]); i++)
        {
            if (strcmp(gameCode, PLAYING_CARDS_GAMECODES[i]) == 0) return true;
        }

        return false;
    }

private slots:
    bool processCardDirectory(QDir directory);
    void on_browse();

    void on_draw();
    void on_shuffle();
    void on_flip();

private:
    Ui::PlayingCardsDialog* ui;
};

#endif // PLAYINGCARDSDIALOG_H

