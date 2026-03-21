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

#include "LibraryController.h"

#include "LibraryCoverManager.h"
#include "LibraryGridDelegate.h"
#include "LibraryModel.h"
#include "Window.h"
#include "main.h"

#include <QAction>
#include <QDirIterator>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QListView>
#include <QMenu>
#include <QPainter>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSortFilterProxyModel>
#include <QThreadPool>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

static constexpr char kKeyViewMode[]       = "Library.ViewMode";
static constexpr char kKeyCoverSize[]      = "Library.CoverSize";
static constexpr char kKeyROMDirs[]        = "Library.ROMDirs";
static constexpr char kKeyFavorites[]      = "Library.Favorites";
static constexpr char kKeyRecentlyPlayed[] = "Library.RecentlyPlayed";
static constexpr char kKeyHeaderState[]    = "Library.HeaderState";  // base64 QHeaderView state
static constexpr char kKeyCustomNames[]    = "Library.CustomNames";

// ---------------------------------------------------------------------------
// LibraryBackground
// ---------------------------------------------------------------------------

LibraryBackground::LibraryBackground(QWidget* content, QWidget* parent)
    : QWidget(parent)
    , m_logo(QStringLiteral(":/melon-logo"))
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(content);
    setAttribute(Qt::WA_StyledBackground, false);
}

void LibraryBackground::paintEvent(QPaintEvent*)
{
    if (m_logo.isNull()) return;
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const int maxDim = qMin(width(), height()) * 2 / 5;
    const QPixmap scaled = m_logo.scaled(maxDim, maxDim,
                                          Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation);
    const int x = (width()  - scaled.width())  / 2;
    const int y = (height() - scaled.height()) / 2;
    p.setOpacity(0.08);
    p.drawPixmap(x, y, scaled);
}

// ---------------------------------------------------------------------------
// LibraryController
// ---------------------------------------------------------------------------

LibraryController::LibraryController(MainWindow* mainWindow,
                                     Config::Table& globalCfg,
                                     QWidget* parent)
    : QSplitter(Qt::Horizontal, parent)
    , m_mainWindow(mainWindow)
    , m_globalCfg(globalCfg)
{
    setChildrenCollapsible(false);
    setHandleWidth(1);

    auto* mainPane   = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(mainPane);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    m_toolBar = new LibraryToolBar(mainPane);
    mainLayout->addWidget(m_toolBar);
    m_viewStack = new QStackedWidget(mainPane);
    mainLayout->addWidget(m_viewStack, 1);
    addWidget(mainPane);

    const QString coversDir = emuDirectory + "/library/covers";
    m_coverManager = new LibraryCoverManager(coversDir, this);

    m_libraryModel = new melonds::LibraryModel(this);
    m_proxyModel   = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_libraryModel);
    m_proxyModel->setSortRole(Qt::EditRole);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setFilterKeyColumn(melonds::LibraryModel::COL_NAME);
    m_proxyModel->sort(melonds::LibraryModel::COL_NAME, Qt::AscendingOrder);

    // ---- List view -------------------------------------------------------
    m_listView = new QTreeView(m_viewStack);
    m_listView->setModel(m_proxyModel);
    m_listView->setRootIsDecorated(false);
    m_listView->setItemsExpandable(false);
    m_listView->setUniformRowHeights(true);
    m_listView->setAlternatingRowColors(true);
    m_listView->setMouseTracking(true);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listView->setSortingEnabled(true);
    // Row height: give a bit of padding so the 32×32 icon fits nicely
    m_listView->setIconSize(QSize(24, 24));
    m_listView->header()->setSectionsMovable(true);
    m_listView->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    m_listView->setStyleSheet("QTreeView { background: transparent; }");
    connect(m_listView->header(), &QHeaderView::customContextMenuRequested,
            this, &LibraryController::onHeaderContextMenu);

    auto* listBg = new LibraryBackground(m_listView, m_viewStack);
    m_viewStack->addWidget(listBg);  // index 0

    // ---- Grid view -------------------------------------------------------
    m_gridDelegate = new LibraryGridDelegate(m_coverManager, this);
    m_gridView = new QListView(m_viewStack);
    m_gridView->setViewMode(QListView::IconMode);
    m_gridView->setResizeMode(QListView::Adjust);
    m_gridView->setUniformItemSizes(true);
    m_gridView->setSpacing(0);
    m_gridView->setWordWrap(true);
    m_gridView->setMouseTracking(true);
    m_gridView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_gridView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_gridView->setItemDelegate(m_gridDelegate);
    m_gridView->setGridSize(m_gridDelegate->cardSize());
    m_gridView->setModel(m_proxyModel);
    m_gridView->setStyleSheet("QListView { background: transparent; }");
    m_gridView->viewport()->installEventFilter(this);

    auto* gridBg = new LibraryBackground(m_gridView, m_viewStack);
    m_viewStack->addWidget(gridBg);  // index 1

    m_viewStack->setCurrentIndex(0);
    m_currentView = m_listView;

    // ---- Toolbar ---------------------------------------------------------
    connect(m_toolBar, &LibraryToolBar::filterChanged,
            this, &LibraryController::setFilter);
    connect(m_toolBar, &LibraryToolBar::viewModeChanged,
            this, [this](int mode)
    {
        setViewMode(mode);
        m_globalCfg.SetInt(kKeyViewMode, mode);
        Config::Save();
        emit viewModeChanged(mode);
    });
    connect(m_toolBar, &LibraryToolBar::coverSizeChanged,
            this, [this](int size)
    {
        if (m_gridDelegate && m_gridView)
        {
            m_gridDelegate->setCoverSize(size);
            updateGridSize();
            m_gridView->reset();
        }
        m_globalCfg.SetInt(kKeyCoverSize, size);
        Config::Save();
    });

    // ---- Activation ------------------------------------------------------
    auto onActivated = [this](const QModelIndex& idx)
    {
        const QString path = idx.data(melonds::LibraryModel::FullPathRole).toString();
        if (path.isEmpty() || !m_mainWindow) return;
        recordGameLaunched(path);
        m_mainWindow->preloadROMs(QStringList{path}, {}, true);
    };
    connect(m_listView, &QAbstractItemView::activated, this, onActivated);
    connect(m_gridView, &QAbstractItemView::activated, this, onActivated);

    setupContextMenu(m_listView);
    setupContextMenu(m_gridView);

    connect(m_coverManager, &LibraryCoverManager::coversChanged, this, [this]()
    { m_gridView->viewport()->update(); });

    connect(m_libraryModel, &QAbstractItemModel::modelReset,
            this, &LibraryController::updateCountBadges);
    connect(m_libraryModel, &QAbstractItemModel::rowsInserted,
            this, &LibraryController::updateCountBadges);
    connect(m_libraryModel, &QAbstractItemModel::rowsRemoved,
            this, &LibraryController::updateCountBadges);
}

LibraryController::~LibraryController() = default;

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

void LibraryController::loadConfig()
{
    const QString favStr = m_globalCfg.GetQString(kKeyFavorites);
    for (const QString& p : favStr.split("|", Qt::SkipEmptyParts))
        m_favorites.insert(p);

    const QString rpStr = m_globalCfg.GetQString(kKeyRecentlyPlayed);
    if (!rpStr.isEmpty())
        m_recentlyPlayed = rpStr.split("|", Qt::SkipEmptyParts);

    const int mode = m_globalCfg.GetInt(kKeyViewMode);
    setViewMode(mode);
    m_toolBar->setViewMode(mode);

    const int coverSize = m_globalCfg.GetInt(kKeyCoverSize);
    if (coverSize > 0)
    {
        m_gridDelegate->setCoverSize(coverSize);
        m_toolBar->setCoverSize(coverSize);
    }

    restoreColumnVisibility();
    connectHeaderSave();   // must come after restoreColumnVisibility
    loadCustomNames();

    QStringList romPaths;
    const QString dirStr = m_globalCfg.GetQString(kKeyROMDirs);
    for (const QString& d : dirStr.split("|", Qt::SkipEmptyParts))
        if (!d.isEmpty()) scanDirectory(d, true, &romPaths);
    refresh(true);  // first populate — auto-resize columns

    // CRC computation is deferred so the UI appears immediately
    startCRCWorkers(romPaths);
}

void LibraryController::saveConfig()
{
    m_globalCfg.SetQString(kKeyFavorites,      m_favorites.values().join("|"));
    m_globalCfg.SetQString(kKeyRecentlyPlayed, m_recentlyPlayed.join("|"));
    Config::Save();
}

// ---------------------------------------------------------------------------
// Custom names  — stored as "path\x01name|path\x01name|..."
// \x01 (SOH) is used as the key/value separator because it won't appear in
// paths or user-entered names.
// ---------------------------------------------------------------------------

void LibraryController::saveCustomNames()
{
    QStringList pairs;
    const int rows = m_libraryModel->rowCount();
    for (int i = 0; i < rows; ++i)
    {
        const QModelIndex idx = m_libraryModel->rowIndex(i);
        const QString fp = m_libraryModel->data(idx, melonds::LibraryModel::FullPathRole).toString();
        const QString cn = m_libraryModel->customName(fp);
        if (!cn.isEmpty())
            pairs << (fp + '\x01' + cn);
    }
    m_globalCfg.SetQString(kKeyCustomNames, pairs.join("|"));
    Config::Save();
}

void LibraryController::loadCustomNames()
{
    const QString raw = m_globalCfg.GetQString(kKeyCustomNames);
    for (const QString& pair : raw.split("|", Qt::SkipEmptyParts))
    {
        const int sep = pair.indexOf('\x01');
        if (sep < 1) continue;
        m_libraryModel->setCustomName(pair.left(sep), pair.mid(sep + 1));
    }
}

// ---------------------------------------------------------------------------
// Column / header state — width, order, visibility in one QByteArray
// ---------------------------------------------------------------------------

void LibraryController::saveColumnVisibility()
{
    if (!m_listView) return;
    // Save full header state (widths + order + visibility) as base64
    const QByteArray state = m_listView->header()->saveState();
    m_globalCfg.SetQString(kKeyHeaderState, QString::fromLatin1(state.toBase64()));
    Config::Save();
}

void LibraryController::restoreColumnVisibility()
{
    if (!m_listView) return;
    const QString b64 = m_globalCfg.GetQString(kKeyHeaderState);

    if (b64.isEmpty())
    {
        // First-run defaults: Name + Header Title + Size visible; others hidden
        m_listView->setColumnHidden(melonds::LibraryModel::COL_NAME,         false);
        m_listView->setColumnHidden(melonds::LibraryModel::COL_HEADER_TITLE, false);
        m_listView->setColumnHidden(melonds::LibraryModel::COL_GAME_CODE,    true);
        m_listView->setColumnHidden(melonds::LibraryModel::COL_SIZE,         false);
        m_listView->setColumnHidden(melonds::LibraryModel::COL_CRC,          true);
        m_listView->setColumnHidden(melonds::LibraryModel::COL_LOCATION,     true);
        return;
    }

    const QByteArray state = QByteArray::fromBase64(b64.toLatin1());
    m_listView->header()->restoreState(state);

    // COL_NAME must always be visible regardless of saved state
    m_listView->setColumnHidden(melonds::LibraryModel::COL_NAME, false);
}

// Save whenever the user resizes or reorders columns
void LibraryController::connectHeaderSave()
{
    connect(m_listView->header(), &QHeaderView::sectionResized,
            this, [this](int, int, int) { saveColumnVisibility(); });
    connect(m_listView->header(), &QHeaderView::sectionMoved,
            this, [this](int, int, int) { saveColumnVisibility(); });
}

void LibraryController::onHeaderContextMenu(const QPoint& pos)
{
    if (!m_listView) return;
    QMenu menu(m_listView->header());
    const int cols = m_libraryModel->columnCount();
    for (int i = 0; i < cols; ++i)
    {
        if (i == melonds::LibraryModel::COL_NAME) continue;
        const QString label = m_libraryModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();
        QAction* act = menu.addAction(label);
        act->setCheckable(true);
        act->setChecked(!m_listView->isColumnHidden(i));
        connect(act, &QAction::triggered, this, [this, i](bool checked)
        {
            m_listView->setColumnHidden(i, !checked);
            saveColumnVisibility();
        });
    }
    menu.exec(m_listView->header()->mapToGlobal(pos));
}

// ---------------------------------------------------------------------------
// Async CRC computation
// ---------------------------------------------------------------------------

void LibraryController::startCRCWorkers(const QStringList& paths)
{
    for (const QString& path : paths)
    {
        // Capture model as a raw pointer — safe because LibraryModel outlives
        // all workers (it's owned by LibraryController which is never destroyed
        // while workers are running; and setCRC is a no-op if the row is gone).
        melonds::LibraryModel* model = m_libraryModel;
        const QString pathCopy = path;

        QThreadPool::globalInstance()->start([model, pathCopy]()
        {
            const quint32 crc = melonds::LibraryModel::computeFileCRC32(pathCopy);
            QMetaObject::invokeMethod(model, [model, pathCopy, crc]()
            {
                model->setCRC(pathCopy, crc);
            }, Qt::QueuedConnection);
        });
    }
}

void LibraryController::setViewMode(int mode)
{
    m_viewMode = mode;
    m_viewStack->setCurrentIndex(mode == 2 ? 1 : 0);
    m_currentView = (mode == 2) ? static_cast<QAbstractItemView*>(m_gridView)
                                : static_cast<QAbstractItemView*>(m_listView);
    if (m_toolBar) m_toolBar->setViewMode(mode);
}

// ---------------------------------------------------------------------------
// Directory scanning
// ---------------------------------------------------------------------------

void LibraryController::addDirectory(const QString& dir, bool recursive)
{
    QStringList newPaths;
    scanDirectory(dir, recursive, &newPaths);
    refresh(false);
    QStringList existing = m_globalCfg.GetQString(kKeyROMDirs).split("|", Qt::SkipEmptyParts);
    if (!existing.contains(dir))
    {
        existing.append(dir);
        m_globalCfg.SetQString(kKeyROMDirs, existing.join("|"));
        Config::Save();
    }
    startCRCWorkers(newPaths);
}

void LibraryController::scanDirectory(const QString& dir, bool recursive, QStringList* outPaths)
{
    const QDirIterator::IteratorFlags flags = recursive
        ? (QDirIterator::Subdirectories | QDirIterator::FollowSymlinks)
        : QDirIterator::NoIteratorFlags;

    QDirIterator it(dir, {"*.nds", "*.NDS"}, QDir::Files, flags);
    QList<melonds::LibraryEntry> newEntries;

    while (it.hasNext())
    {
        const QString path = it.next();
        if (m_knownPaths.contains(path)) continue;

        melonds::LibraryEntry e = melonds::LibraryModel::readNDSHeader(path);
        if (e.isNull())
        {
            const QFileInfo fi(path);
            e.fullpath = path;
            e.base     = fi.absolutePath();
            e.filename = fi.fileName();
            e.platform = QStringLiteral("NDS");
            e.fileSize = fi.size();
        }
        m_knownPaths.insert(path);
        newEntries.append(e);
        if (outPaths) outPaths->append(path);
    }

    if (!newEntries.isEmpty())
        m_libraryModel->addEntries(newEntries);
}

// ---------------------------------------------------------------------------
// Refresh / clear
// ---------------------------------------------------------------------------

void LibraryController::refresh(bool firstPopulate)
{
    QStringList stale;
    for (const QString& path : m_knownPaths)
        if (!QFileInfo::exists(path)) stale.append(path);
    if (!stale.isEmpty())
    {
        for (const QString& p : stale) m_knownPaths.remove(p);
        m_libraryModel->removeEntries(stale);
    }


    // Auto-resize columns only on first populate (not on every subsequent refresh)
    if (firstPopulate && m_listView)
    {
        QTimer::singleShot(0, this, [this]()
        {
            // Only resize columns that have no saved state yet
            if (m_globalCfg.GetQString(kKeyHeaderState).isEmpty())
                for (int i = 0; i < m_libraryModel->columnCount(); ++i)
                    if (!m_listView->isColumnHidden(i))
                        m_listView->resizeColumnToContents(i);
        });
    }

    emit doneLoading();
}

void LibraryController::clear()
{
    m_knownPaths.clear();
    m_libraryModel->clear();
    m_globalCfg.SetQString(kKeyROMDirs, "");
    Config::Save();
}

// ---------------------------------------------------------------------------
// Filter
// ---------------------------------------------------------------------------

void LibraryController::setFilter(const LibraryFilter& filter)
{
    m_activeFilter = filter;

    auto applyAllowSet = [this](const QSet<QString>& allowed)
    {
        m_proxyModel->setFilterRole(melonds::LibraryModel::FullPathRole);
        if (allowed.isEmpty())
            m_proxyModel->setFilterRegularExpression(QRegularExpression("^$(?!.)"));
        else
        {
            QStringList escaped;
            for (const QString& p : allowed)
                escaped << QRegularExpression::escape(p);
            m_proxyModel->setFilterRegularExpression(
                QRegularExpression("^(" + escaped.join("|") + ")$"));
        }
    };

    if (filter.section == LibraryFilter::Section::RecentlyPlayed)
        applyAllowSet(QSet<QString>(m_recentlyPlayed.begin(), m_recentlyPlayed.end()));
    else if (filter.section == LibraryFilter::Section::Favorites)
        applyAllowSet(m_favorites);
    else
    {
        m_proxyModel->setFilterRole(Qt::DisplayRole);
        m_proxyModel->setFilterKeyColumn(melonds::LibraryModel::COL_NAME);
        m_proxyModel->setFilterFixedString(filter.searchTerm);
    }
}

// ---------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------

void LibraryController::selectEntry(const QString& fullpath)
{
    if (!m_currentView) return;
    const QModelIndex src = m_libraryModel->index(fullpath);
    const QModelIndex idx = m_proxyModel->mapFromSource(src);
    if (idx.isValid())
    {
        m_currentView->selectionModel()->setCurrentIndex(
            idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
        m_currentView->scrollTo(idx);
    }
}

melonds::LibraryEntry LibraryController::selectedEntry() const
{
    if (!m_currentView) return {};
    const QModelIndex idx = m_currentView->selectionModel()->currentIndex();
    if (!idx.isValid()) return {};
    return m_libraryModel->entry(idx.data(melonds::LibraryModel::FullPathRole).toString());
}

QString LibraryController::selectedPath() const { return selectedEntry().fullpath; }

// ---------------------------------------------------------------------------
// Favorites / recently played
// ---------------------------------------------------------------------------

void LibraryController::recordGameLaunched(const QString& fullpath)
{
    if (fullpath.isEmpty()) return;
    m_recentlyPlayed.removeAll(fullpath);
    m_recentlyPlayed.prepend(fullpath);
    while (m_recentlyPlayed.size() > 50) m_recentlyPlayed.removeLast();
    saveConfig();
    if (m_activeFilter.section == LibraryFilter::Section::RecentlyPlayed)
        setFilter(m_activeFilter);
}

void LibraryController::toggleFavorite(const QString& fullpath)
{
    if (fullpath.isEmpty()) return;
    if (m_favorites.contains(fullpath)) m_favorites.remove(fullpath);
    else m_favorites.insert(fullpath);
    saveConfig();
    emit entryFavorited(fullpath, m_favorites.contains(fullpath));
    if (m_activeFilter.section == LibraryFilter::Section::Favorites)
        setFilter(m_activeFilter);
}

void LibraryController::refreshCovers() { if (m_coverManager) m_coverManager->refresh(); }

// ---------------------------------------------------------------------------
// Context menu (row right-click)
// ---------------------------------------------------------------------------

void LibraryController::setupContextMenu(QAbstractItemView* view)
{
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(view, &QWidget::customContextMenuRequested,
            this, [this, view](const QPoint& pos)
    {
        const QModelIndex idx = view->indexAt(pos);
        if (!idx.isValid()) return;

        const QString fullpath    = idx.data(melonds::LibraryModel::FullPathRole).toString();
        if (fullpath.isEmpty()) return;

        const QString filename    = fullpath.section('/', -1);
        const QString stem        = QFileInfo(filename).completeBaseName();
        const QString displayName = idx.data(melonds::LibraryModel::DisplayNameRole).toString();
        const bool    fav         = m_favorites.contains(fullpath);

        QMenu menu(view);

        // ---- Play --------------------------------------------------------
        QAction* playAction = menu.addAction(tr("Play"));
        connect(playAction, &QAction::triggered, this, [this, fullpath]()
        {
            if (!m_mainWindow) return;
            recordGameLaunched(fullpath);
            m_mainWindow->preloadROMs(QStringList{fullpath}, {}, true);
        });

        menu.addSeparator();

        // ---- Rename ------------------------------------------------------
        QAction* renameAction = menu.addAction(tr("Rename..."));
        connect(renameAction, &QAction::triggered, this, [this, fullpath, displayName, view]()
        {
            bool ok = false;
            const QString newName = QInputDialog::getText(
                view,
                tr("Rename Game"),
                tr("Display name:"),
                QLineEdit::Normal,
                displayName,
                &ok);
            if (!ok) return;

            // Empty input = revert to filename stem
            m_libraryModel->setCustomName(fullpath, newName.trimmed());
            saveCustomNames();
        });

        menu.addSeparator();

        // ---- Favorites ---------------------------------------------------
        QAction* favAction = menu.addAction(
            fav ? tr("Remove from Favorites") : tr("Add to Favorites"));
        connect(favAction, &QAction::triggered, this,
                [this, fullpath]() { toggleFavorite(fullpath); });

        // ---- Cover art ---------------------------------------------------
        if (m_coverManager)
        {
            menu.addSeparator();
            const QPixmap existing = m_coverManager->cover(QString(), QString(), filename);
            QAction* coverAction = menu.addAction(
                existing.isNull() ? tr("Set Cover Art...") : tr("Change Cover Art..."));
            connect(coverAction, &QAction::triggered, this, [this, stem, view]()
            {
                const QString src = QFileDialog::getOpenFileName(
                    view, tr("Select Cover Image for %1").arg(stem),
                    QString(), tr("Images (*.png *.jpg *.jpeg)"));
                if (src.isEmpty()) return;
                const QString dest     = m_coverManager->coversDir() + "/" + stem;
                const QString ext      = QFileInfo(src).suffix().toLower();
                const QString destPath = dest + "." + ext;
                for (const QString& e : {QString("png"), QString("jpg"), QString("jpeg")})
                    QFile::remove(dest + "." + e);
                if (!QFile::copy(src, destPath)) { QPixmap p(src); if (!p.isNull()) p.save(destPath); }
                m_coverManager->refresh();
            });

            if (!existing.isNull())
            {
                QAction* removeAct = menu.addAction(tr("Remove Cover Art"));
                connect(removeAct, &QAction::triggered, this, [this, stem]()
                {
                    const QString dest = m_coverManager->coversDir() + "/" + stem;
                    for (const QString& e : {QString("png"), QString("jpg"), QString("jpeg")})
                        QFile::remove(dest + "." + e);
                    m_coverManager->refresh();
                });
            }

            QAction* openDirAct = menu.addAction(tr("Open Covers Folder"));
            connect(openDirAct, &QAction::triggered,
                    m_coverManager, &LibraryCoverManager::openCoversDir);
        }

        menu.exec(view->viewport()->mapToGlobal(pos));
    });
}

// ---------------------------------------------------------------------------
// Count badges / grid sizing / event filter
// ---------------------------------------------------------------------------

void LibraryController::updateCountBadges()
{
    if (!m_toolBar) return;
    m_toolBar->setGameCounts(m_libraryModel->rowCount(), m_libraryModel->rowCount(), 0, 0, 0);
}

void LibraryController::updateGridSize()
{
    if (!m_gridView || !m_gridDelegate) return;
    const int viewportW = m_gridView->viewport()->width();
    const int minCardW  = m_gridDelegate->cardSize().width();
    if (minCardW <= 0 || viewportW <= 0) return;
    const int safeW = viewportW - 1;
    const int cols  = qMax(1, safeW / minCardW);
    m_gridView->setUpdatesEnabled(false);
    m_gridView->setGridSize(QSize(safeW / cols, m_gridDelegate->cardSize().height()));
    m_gridView->setUpdatesEnabled(true);
}

bool LibraryController::eventFilter(QObject* obj, QEvent* event)
{
    if (m_gridView && obj == m_gridView->viewport() && event->type() == QEvent::Resize)
        updateGridSize();
    return QSplitter::eventFilter(obj, event);
}

void LibraryController::resizeEvent(QResizeEvent* event)
{
    QSplitter::resizeEvent(event);
    QTimer::singleShot(0, this, [this]() { updateGridSize(); });
}
