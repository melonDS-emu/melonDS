/* Copyright (c) 2013-2024 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LibraryCoverManager.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QUrl>



static const QStringList kImageExts = {"png", "jpg", "jpeg"};

LibraryCoverManager::LibraryCoverManager(const QString& coversDir, QObject* parent)
	: QObject(parent)
	, m_coversDir(coversDir)
{
	QDir().mkpath(coversDir);
}

QPixmap LibraryCoverManager::cover(const QString& internalCode,
                                   const QString& title,
                                   const QString& filename) const
{
	// Cache key: filename stem is most stable (user sets covers by stem via right-click)
	const QString stem = !filename.isEmpty() ? QFileInfo(filename).completeBaseName() : QString();
	const QString cacheKey = !stem.isEmpty() ? stem : (!internalCode.isEmpty() ? internalCode : title);

	auto it = m_cache.find(cacheKey);
	if (it != m_cache.end()) {
		return it.value(); // may be null pixmap (= "no cover found")
	}

	const QString path = findCoverPath(internalCode, title, filename);
	QPixmap pix;
	if (!path.isEmpty()) {
		QPixmap raw(path);
		if (!raw.isNull()) {
			pix = raw.scaled(COVER_WIDTH, COVER_HEIGHT,
			                 Qt::KeepAspectRatio,
			                 Qt::SmoothTransformation);
		}
	}
	m_cache[cacheKey] = pix; // cache even if null so we don't re-stat missing files
	return pix;
}

void LibraryCoverManager::openCoversDir() const {
	QDesktopServices::openUrl(QUrl::fromLocalFile(m_coversDir));
}

void LibraryCoverManager::refresh() {
	m_cache.clear();
	emit coversChanged();
}

QString LibraryCoverManager::findCoverPath(const QString& internalCode,
                                           const QString& title,
                                           const QString& filename) const
{
	QDir dir(m_coversDir);
	if (!dir.exists()) {
		return {};
	}

	// Build candidate base names in priority order
	QStringList candidates;
	// Filename stem first — right-click "Set Cover Art" saves by stem
	if (!filename.isEmpty()) {
		candidates << QFileInfo(filename).completeBaseName();
	}
	// Fall back to internal code (useful for official ROMs placed manually)
	if (!internalCode.isEmpty()) {
		candidates << internalCode;
	}
	// Fall back to title
	if (!title.isEmpty()) {
		candidates << title;
	}

	for (const QString& base : candidates) {
		for (const QString& ext : kImageExts) {
			QString path = dir.filePath(base + "." + ext);
			if (QFileInfo::exists(path)) {
				return path;
			}
		}
	}
	return {};
}
