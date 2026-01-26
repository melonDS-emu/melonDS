#include "ToastOverlay.h"
#include <QPainter>
#include <QEvent>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include "Config.h"

ToastWidget::ToastWidget(const QString& title, const QString& description, const QPixmap& icon, QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    
    auto* iconLabel = new QLabel;
    iconLabel->setPixmap(icon.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    layout->addWidget(iconLabel);

    if (!title.isEmpty()) {
        auto* textLayout = new QVBoxLayout;
        auto* titleLabel = new QLabel(title);
        titleLabel->setStyleSheet("font-weight: bold; color: white; border: none; background: transparent;");
        textLayout->addWidget(titleLabel);

        if (!description.isEmpty()) {
            auto* descLabel = new QLabel(description);
            descLabel->setStyleSheet("color: #ddd; border: none; background: transparent;");
            textLayout->addWidget(descLabel);
        }
        layout->addLayout(textLayout);
    }

    setAttribute(Qt::WA_StyledBackground, false);
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet("background: transparent;");
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
    : QGraphicsView(parent)
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);
    setFrameShape(QFrame::NoFrame);
    setBackgroundRole(QPalette::NoRole);
    setStyleSheet("background: transparent; border: none;");
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setRenderHint(QPainter::Antialiasing);

    m_soundEffect.setSource(QUrl("qrc:/ra/sounds/unlock.wav"));
    m_soundEffect.setVolume(0.7f);
}

void ToastOverlay::ShowToast(const QString& title, const QString& description, const QPixmap& icon)
{
    auto* toast = new ToastWidget(title, description, icon);
    QGraphicsProxyWidget* proxy = m_scene->addWidget(toast);
    
    m_toastProxies.append(proxy);
    m_soundEffect.play();

    RepositionToasts();

    QTimer::singleShot(3000, this, [this, proxy]() {
        m_toastProxies.removeOne(proxy);
        m_scene->removeItem(proxy);
        if (proxy->widget()) proxy->widget()->deleteLater();
        delete proxy;
        RepositionToasts();
    });
}

void ToastOverlay::ShowChallenge(const QPixmap& icon)
{
    if (m_challengeProxy) {
        m_scene->removeItem(m_challengeProxy);
        if (m_challengeProxy->widget()) m_challengeProxy->widget()->deleteLater();
        delete m_challengeProxy;
    }
    
    auto* challenge = new ToastWidget("", "", icon);
    m_challengeProxy = m_scene->addWidget(challenge);
    
    RepositionChallenge();
}

void ToastOverlay::HideChallenge()
{
    if (m_challengeProxy) {
        m_scene->removeItem(m_challengeProxy);
        if (m_challengeProxy->widget()) m_challengeProxy->widget()->deleteLater();
        delete m_challengeProxy;
        m_challengeProxy = nullptr;
    }
}

void ToastOverlay::RepositionToasts()
{
    if (!parentWidget()) return;

    int rotMode = Config::GetLocalTable(0).GetInt("Window0.ScreenRotation");
    int angle = rotMode * 90;
    QRect pRect = parentWidget()->rect();
    m_scene->setSceneRect(0, 0, pRect.width(), pRect.height());

    const int margin = 20;
    int offset = 60; 

    for (auto* proxy : m_toastProxies)
    {
        proxy->widget()->adjustSize();
        QSize s = proxy->widget()->size();
        
        proxy->setTransformOriginPoint(s.width() / 2.0, s.height() / 2.0);
        proxy->setRotation(angle);

        int finalW = (rotMode % 2 == 0) ? s.width() : s.height();
        int finalH = (rotMode % 2 == 0) ? s.height() : s.width();

        int centerX, centerY;

        if (rotMode == 0) {
            centerX = pRect.width() - margin - finalW / 2;
            centerY = offset + finalH / 2;
        } else if (rotMode == 1) {
            centerX = pRect.width() - offset - finalW / 2;
            centerY = pRect.height() - margin - finalH / 2;
        } else if (rotMode == 2) {
            centerX = margin + finalW / 2;
            centerY = pRect.height() - offset - finalH / 2;
        } else {
            centerX = offset + finalW / 2;
            centerY = margin + finalH / 2;
        }

        proxy->setPos(centerX - s.width() / 2.0, centerY - s.height() / 2.0);
        offset += finalH + 10;
    }
}

void ToastOverlay::RepositionChallenge()
{
    if (!m_challengeProxy || !parentWidget()) return;

    int rotMode = Config::GetLocalTable(0).GetInt("Window0.ScreenRotation");
    int angle = rotMode * 90;
    QRect pRect = parentWidget()->rect();
    const int margin = 20;

    m_challengeProxy->widget()->adjustSize();
    QSize s = m_challengeProxy->widget()->size();

    m_challengeProxy->setTransformOriginPoint(s.width() / 2.0, s.height() / 2.0);
    m_challengeProxy->setRotation(angle);

    int finalW = (rotMode % 2 == 0) ? s.width() : s.height();
    int finalH = (rotMode % 2 == 0) ? s.height() : s.width();

    int centerX, centerY;

    if (rotMode == 0) {
        centerX = pRect.width() - margin - finalW / 2;
        centerY = pRect.height() - margin - finalH / 2;
    } else if (rotMode == 1) {
        centerX = margin + finalW / 2;
        centerY = pRect.height() - margin - finalH / 2;
    } else if (rotMode == 2) {
        centerX = margin + finalW / 2;
        centerY = margin + finalH / 2;
    } else {
        centerX = pRect.width() - margin - finalW / 2;
        centerY = margin + finalH / 2;
    }

    m_challengeProxy->setPos(centerX - s.width() / 2.0, centerY - s.height() / 2.0);
}

void ToastOverlay::resizeEvent(QResizeEvent* event)
{
    if (parentWidget())
        setGeometry(parentWidget()->rect());
        
    RepositionToasts();
    RepositionChallenge();
    QGraphicsView::resizeEvent(event);
}

bool ToastOverlay::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == parentWidget())
    {
        if (event->type() == QEvent::Resize || event->type() == QEvent::WindowStateChange)
        {
            setGeometry(parentWidget()->rect());
            RepositionToasts();
            RepositionChallenge();
        }
    }
    return QGraphicsView::eventFilter(obj, event);
}