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

#include <cctype>
#include <set>

#include <QListWidgetItem>
#include <QPushButton>

#include "SaveConversionDialog.h"
#include "ui_SaveConversionDialog.h"

namespace fs = std::filesystem;

SaveConversionDialog::SaveConversionDialog(QWidget* parent, const QString& dir,
                                           const QString& fromext, const QString& toext,
                                           bool willResetEmu)
    : QDialog(parent), ui(new Ui::SaveConversionDialog),
      dirPath(dir), fromExt(fromext), toExt(toext), resetsEmu(willResetEmu)
{
    ui->setupUi(this);

    // no WA_DeleteOnClose: the caller reads renamedCount() after exec() returns

    QFont warnfont = ui->lblResetWarning->font();
    warnfont.setBold(true);
    warnfont.setUnderline(true);
    ui->lblResetWarning->setFont(warnfont);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setText("Rename");

    // Enter must not rename a whole directory unread
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setDefault(true);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &SaveConversionDialog::onRename);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    scan();
    showPreview();
}

SaveConversionDialog::~SaveConversionDialog()
{
    delete ui;
}

void SaveConversionDialog::addHeading(const QString& text)
{
    QListWidgetItem* item = new QListWidgetItem(text, ui->lstFiles);
    QFont font = item->font();
    font.setBold(true);
    item->setFont(font);
}

void SaveConversionDialog::addLine(const QString& text)
{
    new QListWidgetItem("    " + text, ui->lstFiles);
}

// Collects candidates and their destinations. Nothing is renamed here.
void SaveConversionDialog::scan()
{
    std::string olde = fromExt.toStdString();
    std::string newe = toExt.toStdString();

    std::error_code ec;
    fs::path base = fs::u8path(dirPath.toStdString());

    std::set<fs::path> pending;  // destinations already claimed by an earlier candidate

    fs::directory_iterator it(base, ec);
    if (ec)
    {
        skipped.push_back("(could not read the save directory - "
                          + QString::fromLocal8Bit(ec.message().c_str()) + ")");
        return;
    }

    for (fs::directory_iterator end; it != end; it.increment(ec))
    {
        if (ec)
        {
            // report rather than silently truncating the listing
            skipped.push_back("(directory listing stopped early - "
                              + QString::fromLocal8Bit(ec.message().c_str()) + ")");
            break;
        }

        const fs::directory_entry& entry = *it;

        std::string name = entry.path().filename().u8string();

        // match <name><fromExt>, optionally followed by ".<digits>" (instance suffix)
        size_t pos = name.rfind(olde);
        if (pos == std::string::npos) continue;

        std::string tail = name.substr(pos + olde.length());
        if (!tail.empty())
        {
            // capped so a dated backup like game.sav.20240101 isn't matched
            if (tail[0] != '.') continue;
            if (tail.length() < 2 || tail.length() > 3) continue;
            bool alldigits = true;
            for (size_t i = 1; i < tail.length(); i++)
                if (!isdigit((unsigned char)tail[i])) { alldigits = false; break; }
            if (!alldigits) continue;
        }

        std::string newname = name.substr(0, pos) + newe + tail;
        fs::path to = base / fs::u8path(newname);

        QString qname = QString::fromStdString(name);
        QString qnew = QString::fromStdString(newname);

        // error_code overloads throughout: a filesystem_error would escape this slot
        std::error_code stc;
        fs::file_status st = entry.symlink_status(stc);
        if (stc || !fs::status_known(st))
        {
            skipped.push_back(qname + " - could not read file type");
            continue;
        }

        // symlinks are left alone: renaming a link moves the link, not its target
        if (fs::is_symlink(st))
        {
            skipped.push_back(qname + " - is a symlink");
            continue;
        }

        if (!fs::is_regular_file(st))
        {
            skipped.push_back(qname + " - not a regular file");
            continue;
        }

        // symlink_status, not exists(): a dangling link reports false from exists()
        // and fs::rename would replace it. Unknown status counts as occupied too.
        std::error_code dec;
        fs::file_status dst = fs::symlink_status(to, dec);
        if (!fs::status_known(dst) || fs::exists(dst))
        {
            skipped.push_back(qname + " - " + qnew + " already exists");
            continue;
        }

        if (pending.count(to))
        {
            skipped.push_back(qname + " - another file would be renamed to " + qnew);
            continue;
        }

        renames.push_back({entry.path(), to});
        pending.insert(to);
        if (!tail.empty()) touchesInstanceFiles = true;
    }
}

void SaveConversionDialog::showPreview()
{
    ui->lblHeader->setText(QString("Rename %1 save files to %2?\n\n"
                                   "These files will be renamed immediately:\n%3")
                           .arg(fromExt, toExt, dirPath));

    ui->lblResetWarning->setText(resetsEmu
        ? "The running game will be restarted so it picks up the new file names. "
          "Save in-game first if you have unsaved progress."
        : "");
    ui->lblResetWarning->setVisible(resetsEmu);

    ui->lstFiles->clear();

    if (renames.empty() && skipped.empty())
    {
        addHeading(QString("No %1 save files were found in this directory.").arg(fromExt));
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        return;
    }

    if (!renames.empty())
    {
        addHeading(QString("Will be renamed (%1)").arg(renames.size()));
        for (const Rename& r : renames)
            addLine(QString::fromStdString(r.from.filename().u8string())
                    + "  ->  " + QString::fromStdString(r.to.filename().u8string()));
    }

    if (!skipped.empty())
    {
        addHeading(QString("Will be skipped (%1)").arg(skipped.size()));
        for (const QString& s : skipped)
            addLine(s);
    }

    // the extension setting is per-instance, so renaming another instance's file
    // leaves that instance still configured for the old one
    if (touchesInstanceFiles)
    {
        addHeading("Note");
        addLine("Files ending in .2, .3 and so on belong to other melonDS instances.");
        addLine("Their own save file extension setting is not changed by this.");
    }

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!renames.empty());
}

void SaveConversionDialog::onRename()
{
    for (const Rename& r : renames)
    {
        QString qold = QString::fromStdString(r.from.filename().u8string());
        QString qnew = QString::fromStdString(r.to.filename().u8string());

        // re-check: the preview has no time bound and fs::rename replaces silently
        std::error_code dec;
        fs::file_status dst = fs::symlink_status(r.to, dec);
        if (!fs::status_known(dst) || fs::exists(dst))
        {
            skipped.push_back(qold + " - " + qnew + " appeared while this window was open");
            continue;
        }

        std::error_code rec;
        fs::rename(r.from, r.to, rec);
        if (rec)
            skipped.push_back(qold + " - " + QString::fromLocal8Bit(rec.message().c_str()));
        else
            renamed.push_back(qold + "  ->  " + qnew);
    }

    showResults();
}

void SaveConversionDialog::showResults()
{
    ui->lblHeader->setText(QString("Renamed %1, skipped %2, of %3 file(s) found in:\n%4")
                           .arg(renamed.size())
                           .arg(skipped.size())
                           .arg(renamed.size() + skipped.size())
                           .arg(dirPath));

    bool willreset = resetsEmu && !renamed.empty();
    ui->lblResetWarning->setText(willreset
        ? "The running game will be restarted when path settings closes."
        : "");
    ui->lblResetWarning->setVisible(willreset);

    ui->lstFiles->clear();

    // results can hold error text worth copying; the preview had nothing to act on
    ui->lstFiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->lstFiles->setFocusPolicy(Qt::StrongFocus);

    if (!renamed.empty())
    {
        addHeading(QString("Renamed (%1)").arg(renamed.size()));
        for (const QString& r : renamed)
            addLine(r);
    }

    if (!skipped.empty())
    {
        addHeading(QString("Skipped (%1)").arg(skipped.size()));
        for (const QString& s : skipped)
            addLine(s);
    }

    ui->buttonBox->setStandardButtons(QDialogButtonBox::Close);
}
