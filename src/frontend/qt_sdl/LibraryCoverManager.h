/* Copyright (c) 2013-2024 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QHash>
#include <QObject>
#include <QPixmap>
#include <QString>



// Manages cover art stored in <configDir>/covers/.
// Cover images are matched against a game entry using (in priority order):
//   1. internalCode  e.g. "AXVE" -> covers/AXVE.png / .jpg
//   2. title         e.g. "Pokemon - FireRed Version" -> covers/Pokemon - FireRed Version.png
//   3. filename stem e.g. "PokemonFireRed.gba" -> covers/PokemonFireRed.png
//
// Both .png and .jpg are accepted. Covers are cached as scaled QPixmaps.
class LibraryCoverManager : public QObject {
Q_OBJECT

public:
	static constexpr int COVER_WIDTH  = 160;
	static constexpr int COVER_HEIGHT = 144;

	explicit LibraryCoverManager(const QString& coversDir, QObject* parent = nullptr);

	// Returns the cover for this entry, or a null QPixmap if none found.
	// Results are cached; call invalidate() or refresh() to reload from disk.
	QPixmap cover(const QString& internalCode,
	              const QString& title,
	              const QString& filename) const;

	QString coversDir() const { return m_coversDir; }

	// Open the covers directory in the system file manager
	void openCoversDir() const;

public slots:
	// Clear the pixmap cache so covers are reloaded from disk on next access
	void refresh();

signals:
	void coversChanged();

private:
	QString findCoverPath(const QString& internalCode,
	                      const QString& title,
	                      const QString& filename) const;

	QString                  m_coversDir;
	mutable QHash<QString, QPixmap> m_cache; // key = internalCode or title
};


