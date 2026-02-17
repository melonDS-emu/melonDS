#include "AchievementItemWidget.h"
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QStyleOption>
#include <QPainter>

AchievementItemWidget::AchievementItemWidget(
    const QString& title,
    const QString& description,
    bool unlocked,
    bool measured,
    const QString& progressText,
    float progressPercent,
    float uiScale,
    QWidget* parent
) : QWidget(parent)
{
    uiScale = std::clamp(uiScale, 0.65f, 1.0f);
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::StrongFocus);

    setStyleSheet(R"(
        AchievementItemWidget {
            background: transparent;
            border-bottom: 1px solid rgba(255, 255, 255, 15);
        }
        AchievementItemWidget:focus {
            background: rgba(255, 255, 255, 25);
            border: 1px solid rgba(255, 255, 255, 50);
        }
    )");

    iconLabel = new QLabel(this);
    int iconSize = int(64 * uiScale);
    iconLabel->setFixedSize(iconSize, iconSize);
    iconLabel->setScaledContents(true);
    iconLabel->setStyleSheet("background: transparent; border: none;");
    iconLabel->setPixmap(QPixmap(unlocked ? ":/ra/unlocked.png" : ":/ra/locked.png"));

    auto* rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(int(4 * uiScale));
    rightLayout->setContentsMargins(0, int(5 * uiScale), 0, int(5 * uiScale));

    titleLabel = new QLabel(title, this);
    titleLabel->setStyleSheet(QString(
    "font-weight: bold; color: white; font-size: %1px; background: transparent; border: none;"
    ).arg(int(15 * uiScale)));
    titleLabel->setWordWrap(true);
    rightLayout->addWidget(titleLabel);

    descLabel = new QLabel(description, this);
    descLabel->setStyleSheet(QString(
    "color: #ccc; font-size: %1px; background: transparent; border: none;"
    ).arg(int(13 * uiScale)));
    descLabel->setWordWrap(true);
    rightLayout->addWidget(descLabel);

    progressBar = new QProgressBar(this);
    int barHeight = int(14 * uiScale);
    int barFont   = int(10 * uiScale);

    progressBar->setStyleSheet(QString(R"(
        QProgressBar {
            border: none;
            background: rgba(0, 0, 0, 100);
            height: %1px;
            border-radius: %2px;
            color: white;
            font-weight: bold;
            font-size: %3px;
            text-align: center;
        }
        QProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #005c99, stop:1 #0099ff);
            border-radius: %2px;
        }
    )").arg(barHeight).arg(barHeight / 2).arg(barFont));
    
    bool showBar = measured || !progressText.isEmpty();
    progressBar->setVisible(showBar);

    if (showBar) {
        progressBar->setRange(0, 100);
        progressBar->setValue(static_cast<int>(progressPercent));
        
        if (progressText.isEmpty()) {
            progressBar->setFormat("%p%");
        } else {
            progressBar->setFormat(progressText);
        }
    }

    rightLayout->addWidget(progressBar);

    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(
        int(10 * uiScale),
        int(5 * uiScale),
        int(10 * uiScale),
        int(5 * uiScale)
    );
    mainLayout->addWidget(iconLabel);
    mainLayout->addLayout(rightLayout);
}

void AchievementItemWidget::paintEvent(QPaintEvent* event)
{
    QStyleOption opt;
    opt.init(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void AchievementItemWidget::updateIcon(const QPixmap& pix) {
    if (!pix.isNull()) {
        iconLabel->setPixmap(pix.scaled(
        iconSize, iconSize,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    ));
    }
}