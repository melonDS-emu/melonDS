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
    QWidget* parent
) : QWidget(parent)
{
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
    iconLabel->setFixedSize(64, 64);
    iconLabel->setScaledContents(true);
    iconLabel->setStyleSheet("background: transparent; border: none;");
    iconLabel->setPixmap(QPixmap(unlocked ? ":/ra/unlocked.png" : ":/ra/locked.png"));

    auto* rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(4);
    rightLayout->setContentsMargins(0, 5, 0, 5);

    titleLabel = new QLabel(title, this);
    titleLabel->setStyleSheet("font-weight: bold; color: white; font-size: 15px; background: transparent; border: none;");
    titleLabel->setWordWrap(true);
    rightLayout->addWidget(titleLabel);

    descLabel = new QLabel(description, this);
    descLabel->setStyleSheet("color: #ccc; font-size: 13px; background: transparent; border: none;");
    descLabel->setWordWrap(true);
    rightLayout->addWidget(descLabel);

    progressBar = new QProgressBar(this);
    progressBar->setStyleSheet(R"(
        QProgressBar {
            border: none;
            background: rgba(0, 0, 0, 100);
            height: 14px;
            border-radius: 7px;
            color: white;
            font-weight: bold;
            font-size: 10px;
            text-align: center;
        }
        QProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #005c99, stop:1 #0099ff);
            border-radius: 7px;
        }
    )");
    
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
    mainLayout->setContentsMargins(10, 5, 10, 5);
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
        iconLabel->setPixmap(pix.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}