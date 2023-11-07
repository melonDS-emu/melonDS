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

#ifndef QPATHINPUT_H
#define QPATHINPUT_H

#include <QLineEdit>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>

class QPathInput : public QLineEdit
{
    Q_OBJECT

public:
    QPathInput(QWidget* parent = nullptr) : QLineEdit(parent)
    {
        setAcceptDrops(true);
    }

    ~QPathInput()
    {
    }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override
    {
        if (!event->mimeData()->hasUrls()) return QLineEdit::dragEnterEvent(event);

        QList<QUrl> urls = event->mimeData()->urls();
        if (urls.count() > 1) return QLineEdit::dragEnterEvent(event);

        QString filename = urls.at(0).toLocalFile();

        event->acceptProposedAction();
    }

    void dropEvent(QDropEvent* event) override
    {
        if (!event->mimeData()->hasUrls()) return QLineEdit::dropEvent(event);

        QList<QUrl> urls = event->mimeData()->urls();
        if (urls.count() > 1) return QLineEdit::dropEvent(event);

        QString filename = urls.at(0).toLocalFile();
        setText(filename);
    }
};

#endif // QPATHINPUT_H
