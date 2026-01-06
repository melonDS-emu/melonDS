#include "ToastOverlay.h"
#include <QPainter>
#include <QPropertyAnimation>
#include <QTimer>
#include <QEvent>
#include <QResizeEvent>

ToastWidget::ToastWidget(const QString& title, const QString& description, const QPixmap& icon, QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    
    auto* iconLabel = new QLabel;
    iconLabel->setPixmap(icon.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    layout->addWidget(iconLabel);

    auto* textLayout = new QVBoxLayout;
    auto* titleLabel = new QLabel(title);
    titleLabel->setStyleSheet("font-weight: bold; color: white;");
    auto* descLabel = new QLabel(description);
    descLabel->setStyleSheet("color: #ddd;");
    
    textLayout->addWidget(titleLabel);
    textLayout->addWidget(descLabel);
    layout->addLayout(textLayout);

    setAttribute(Qt::WA_StyledBackground, false);
    setAttribute(Qt::WA_TranslucentBackground);
}

QSize ToastWidget::sizeHint() const
{
    return layout()->sizeHint();
}

void ToastWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(40, 40, 40, 230));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(rect(), 8, 8);
}

ToastOverlay::ToastOverlay(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);

    m_soundEffect.setSource(QUrl("qrc:/ra/sounds/unlock.wav"));
    m_soundEffect.setVolume(0.7f);
}

void ToastOverlay::ShowToast(const QString& title, const QString& description, const QPixmap& icon)
{
    auto* toast = new ToastWidget(title, description, icon, this);
    toast->show();
    m_toasts.append(toast);

    m_soundEffect.play();

    RepositionToasts();

    QTimer::singleShot(3000, toast, [this, toast]() {
        m_toasts.removeOne(toast);
        toast->deleteLater();
        RepositionToasts();
    });
}

void ToastOverlay::resizeEvent(QResizeEvent* event)
{
    if (parentWidget())
        setGeometry(parentWidget()->rect());
        
    RepositionToasts();
    QWidget::resizeEvent(event);
}

void ToastOverlay::paintEvent(QPaintEvent*)
{
}

void ToastOverlay::RepositionToasts()
{
    if (!parentWidget())
        return;

    const int margin = 20;
    const int topOffset = 60;

    int y = topOffset;
    const int rightEdge = width() - margin;

    for (auto* toast : m_toasts)
    {
        toast->adjustSize();

        int x = rightEdge - toast->width();
        toast->move(x, y);

        y += toast->height() + 10;
    }
}
bool ToastOverlay::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == parentWidget())
    {
        if (event->type() == QEvent::Resize ||
            event->type() == QEvent::WindowStateChange)
        {
            setGeometry(parentWidget()->rect());
            RepositionToasts();
        }
    }

    return QWidget::eventFilter(obj, event);
}