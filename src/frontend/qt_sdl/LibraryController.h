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

#pragma once

#include <QSet>
#include <QSplitter>
#include <QStackedWidget>
#include <QStringList>
#include <QTimer>
#include <QWidget>

#include "Config.h"
#include "LibraryModel.h"
#include "LibrarySideBar.h"
#include "LibraryToolBar.h"

class QAbstractItemView;
class QListView;
class QTreeView;
class QSortFilterProxyModel;
class MainWindow;
class LibraryCoverManager;
class LibraryGridDelegate;

// ---------------------------------------------------------------------------
// LibraryBackground — draws the melonDS logo watermark behind list/grid views.
// ---------------------------------------------------------------------------
class LibraryBackground : public QWidget
{
Q_OBJECT
public:
    explicit LibraryBackground(QWidget* content, QWidget* parent = nullptr);
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    QPixmap m_logo;
};

// ---------------------------------------------------------------------------
// LibraryController
// ---------------------------------------------------------------------------
class LibraryController final : public QSplitter
{
Q_OBJECT

public:
    explicit LibraryController(MainWindow* mainWindow,
                               Config::Table& globalCfg,
                               QWidget* parent = nullptr);
    ~LibraryController();

    void loadConfig();

    // 0 = list, 2 = grid
    void setViewMode(int mode);
    int  viewMode() const { return m_viewMode; }

    void        selectEntry(const QString& fullpath);
    melonds::LibraryEntry selectedEntry() const;
    QString     selectedPath() const;

    void addDirectory(const QString& dir, bool recursive = true);

    LibraryCoverManager* coverManager() const { return m_coverManager; }
    QAbstractItemView*   activeView()   const { return m_currentView; }

public slots:
    void clear();
    void setFilter(const LibraryFilter& filter);
    void refreshCovers();
    void recordGameLaunched(const QString& fullpath);
    void toggleFavorite(const QString& fullpath);
    bool isFavorite(const QString& fullpath) const { return m_favorites.contains(fullpath); }

signals:
    void doneLoading();
    void entryFavorited(const QString& fullpath, bool isFavorite);
    void viewModeChanged(int mode);

private slots:
    void refresh(bool firstPopulate = false);
    void updateCountBadges();
    void updateGridSize();
    void onHeaderContextMenu(const QPoint& pos);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void scanDirectory(const QString& dir, bool recursive, QStringList* outPaths = nullptr);
    void setupContextMenu(QAbstractItemView* view);
    void saveConfig();
    void saveColumnVisibility();
    void restoreColumnVisibility();
    void connectHeaderSave();
    void saveCustomNames();
    void loadCustomNames();
    void startCRCWorkers(const QStringList& paths);

    MainWindow*            m_mainWindow   = nullptr;
    Config::Table&         m_globalCfg;

    melonds::LibraryModel*      m_libraryModel = nullptr;
    QSortFilterProxyModel* m_proxyModel   = nullptr;

    QStackedWidget*        m_viewStack    = nullptr;
    QTreeView*             m_listView     = nullptr;
    QListView*             m_gridView     = nullptr;
    QAbstractItemView*     m_currentView  = nullptr;

    LibraryToolBar*        m_toolBar      = nullptr;
    LibraryCoverManager*   m_coverManager = nullptr;
    LibraryGridDelegate*   m_gridDelegate = nullptr;

    int                    m_viewMode     = 0;

    LibraryFilter          m_activeFilter;
    QSet<QString>          m_favorites;
    QStringList            m_recentlyPlayed;

    QSet<QString>          m_knownPaths;
};
