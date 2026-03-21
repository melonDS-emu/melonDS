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

#include "LibraryGridDelegate.h"
#include "LibraryCoverManager.h"
#include "LibraryModel.h"

#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionViewItem>
#include <QModelIndex>
#include <QFontMetrics>

// Nintendo DS brand red
static const QColor kNDSRed(0xC8, 0x00, 0x00);

LibraryGridDelegate::LibraryGridDelegate(LibraryCoverManager* covers, QObject* parent)
    : QStyledItemDelegate(parent)
    , m_covers(covers)
{}

QSize LibraryGridDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const
{
    return cardSize();
}

QSize LibraryGridDelegate::cardSize() const
{
    return QSize(m_coverSize + 24, m_coverSize + 52);
}

void LibraryGridDelegate::paint(QPainter* painter,
                                const QStyleOptionViewItem& option,
                                const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    const QRect r       = option.rect.adjusted(4, 4, -4, -4);
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered  = option.state & QStyle::State_MouseOver;

    // ---- Card background -------------------------------------------------
    QPainterPath cardPath;
    cardPath.addRoundedRect(r, CORNER, CORNER);
    painter->setClipPath(cardPath);

    QColor cardBg = option.palette.color(QPalette::Base);
    if (selected)
        cardBg = option.palette.color(QPalette::Highlight).darker(130);
    else if (hovered)
        cardBg = option.palette.color(QPalette::AlternateBase);
    painter->fillPath(cardPath, cardBg);

    // ---- Art area --------------------------------------------------------
    const QRect artRect(r.left(), r.top(), r.width(), r.height() - 40);

    // Display name from model (custom > filename stem) — used in title row
    const QString displayName = index.data(melonds::LibraryModel::DisplayNameRole).toString();
    const QString fullpath    = index.data(melonds::LibraryModel::FullPathRole).toString();
    const QString gameCode    = index.data(melonds::LibraryModel::GameCodeRole).toString();
    const QString filename    = fullpath.section('/', -1);

    // Cover art: user-set image takes priority
    QPixmap cover;
    if (m_covers)
        cover = m_covers->cover(gameCode, displayName, filename);

    if (!cover.isNull())
    {
        // Scale-to-fill (crop centre)
        const QPixmap scaled = cover.scaled(artRect.size(),
                                            Qt::KeepAspectRatioByExpanding,
                                            Qt::SmoothTransformation);
        const int ox = (scaled.width()  - artRect.width())  / 2;
        const int oy = (scaled.height() - artRect.height()) / 2;
        painter->drawPixmap(artRect, scaled,
                            QRect(ox, oy, artRect.width(), artRect.height()));
    }
    else
    {
        // Fallback 1: NDS banner icon scaled up (nearest-neighbour for pixel crispness)
        const QPixmap icon = index.data(Qt::DecorationRole).value<QPixmap>();
        if (!icon.isNull())
        {
            // Fill background with a dark neutral
            painter->fillRect(artRect, option.palette.color(QPalette::Dark));

            // Scale the 32×32 icon to fill the art area, keeping aspect ratio,
            // centred — use nearest to keep the pixel look crisp
            const QPixmap scaledIcon = icon.scaled(artRect.size(),
                                                   Qt::KeepAspectRatio,
                                                   Qt::FastTransformation);
            const int ix = artRect.left() + (artRect.width()  - scaledIcon.width())  / 2;
            const int iy = artRect.top()  + (artRect.height() - scaledIcon.height()) / 2;
            painter->drawPixmap(ix, iy, scaledIcon);
        }
        else
        {
            // Fallback 2: plain NDS-red placeholder with watermark text
            painter->fillRect(artRect, kNDSRed.darker(120));
            QFont wf = painter->font();
            wf.setPixelSize(28);
            wf.setBold(true);
            painter->setFont(wf);
            painter->setPen(QColor(255, 255, 255, 55));
            painter->drawText(artRect, Qt::AlignCenter, QStringLiteral("NDS"));
        }
    }

    // ---- Platform badge (bottom-left of art) -----------------------------
    {
        const QString label = QStringLiteral("NDS");
        QFont pf = painter->font();
        pf.setPixelSize(9);
        pf.setBold(true);
        painter->setFont(pf);
        const QFontMetrics pfm(pf);
        const int pw = pfm.horizontalAdvance(label) + 10;
        const int ph = 16;
        const QRect badgeRect(artRect.left() + 6, artRect.bottom() - ph - 6, pw, ph);
        QPainterPath bp;
        bp.addRoundedRect(badgeRect, 4, 4);
        painter->fillPath(bp, kNDSRed);
        painter->setPen(Qt::white);
        painter->drawText(badgeRect, Qt::AlignCenter, label);
    }

    // ---- Title row -------------------------------------------------------
    const QRect titleRect(r.left(), r.top() + r.height() - 40, r.width(), 40);

    QFont tf = option.font;
    tf.setPixelSize(11);
    painter->setFont(tf);
    painter->setPen(selected ? option.palette.color(QPalette::HighlightedText)
                             : option.palette.color(QPalette::Text));

    const QFontMetrics tfm(tf);
    const QString elidedTitle = tfm.elidedText(displayName, Qt::ElideRight, titleRect.width() - 8);
    painter->drawText(titleRect.adjusted(4, 4, -4, -4),
                      Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                      elidedTitle);

    // ---- Selection border ------------------------------------------------
    painter->setClipping(false);
    if (selected)
    {
        painter->setPen(QPen(option.palette.color(QPalette::Highlight), 2));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(r.adjusted(1, 1, -1, -1), CORNER, CORNER);
    }

    painter->restore();
}
