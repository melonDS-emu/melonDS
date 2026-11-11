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

#ifndef SAVECONVERSIONDIALOG_H
#define SAVECONVERSIONDIALOG_H

#include <filesystem>
#include <vector>

#include <QDialog>
#include <QString>

namespace Ui { class SaveConversionDialog; }
class SaveConversionDialog;

// Renames cart save files in one directory from one extension to the other. Only ever
// renames onto a free destination: fs::rename would replace an existing file silently,
// which for a save means destroying it with no undo.
class SaveConversionDialog : public QDialog
{
    Q_OBJECT

public:
    SaveConversionDialog(QWidget* parent, const QString& dir,
                         const QString& fromext, const QString& toext,
                         bool willResetEmu);
    ~SaveConversionDialog();

    // renames are already on disk once exec() returns, whatever the user does next
    int renamedCount() const { return (int)renamed.size(); }

private slots:
    void onRename();

private:
    struct Rename
    {
        std::filesystem::path from;
        std::filesystem::path to;
    };

    void scan();
    void showPreview();
    void showResults();

    void addHeading(const QString& text);
    void addLine(const QString& text);

    Ui::SaveConversionDialog* ui;

    QString dirPath;
    QString fromExt;
    QString toExt;
    bool resetsEmu;

    std::vector<Rename> renames;
    bool touchesInstanceFiles = false;  // a rename target carries a .N instance suffix
    std::vector<QString> skipped;  // "file - reason"
    std::vector<QString> renamed;  // "old -> new", filled once the renames have run
};

#endif // SAVECONVERSIONDIALOG_H
