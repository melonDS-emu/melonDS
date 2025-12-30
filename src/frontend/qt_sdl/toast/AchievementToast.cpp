#include "AchievementToast.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTimer>

AchievementToast::AchievementToast(const QString& title,
                                   const QString& desc,
                                   const QPixmap& icon,
                                   QWidget* parent)
    : QWidget(parent)
{
    setFixedWidth(360);
    setAttribute(Qt::WA_TranslucentBackground);

    auto* root = new QHBoxLayout(this);
    root->setSpacing(12);
    root->setContentsMargins(12, 12, 12, 12);

    QLabel* img = new QLabel;
    img->setPixmap(icon.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    root->addWidget(img);

    auto* textCol = new QVBoxLayout;
    QLabel* titleLbl = new QLabel(title);
    QLabel* descLbl  = new QLabel(desc);

    titleLbl->setStyleSheet("color: white; font-weight: bold;");
    descLbl->setStyleSheet("color: #DDD;");

    textCol->addWidget(titleLbl);
    textCol->addWidget(descLbl);
    root->addLayout(textCol);

    setStyleSheet(R"(
        background-color: rgba(20,20,20,230);
        border-radius: 8px;
    )");

    m_anim = new QPropertyAnimation(this, "windowOpacity");
    m_anim->setDuration(250);
}

void AchievementToast::play()
{
    setWindowOpacity(0.0);
    m_anim->setStartValue(0.0);
    m_anim->setEndValue(1.0);
    m_anim->start();

    QTimer::singleShot(4000, this, [this] {
        m_anim->setDirection(QAbstractAnimation::Backward);
        m_anim->start();
        connect(m_anim, &QPropertyAnimation::finished, this, &QObject::deleteLater);
    });
}