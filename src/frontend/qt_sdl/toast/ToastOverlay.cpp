#include "ToastOverlay.h"
#include <QPainter>
#include <QEvent>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QFontMetrics>
#include "Config.h"
#include "EmuInstance.h"
#include "Window.h"

static float ComputeToastScale(const QSize& size)
{
    constexpr float refShortSide = 384.0f;
    float shortSide = std::min(size.width(), size.height());
    float scale = shortSide / refShortSide;
    return std::clamp(scale, 0.65f, 1.25f);
}

static int ConfigureLabelAutoFit(QLabel* label, const QString& text, int maxWidth)
{
    if (text.isEmpty()) return 0;

    label->setText(text);
    QFont font = label->font();
    QFontMetrics fm(font);
    int textWidth = fm.horizontalAdvance(text);

    if (textWidth <= maxWidth) {
        label->setFont(font);
        label->setWordWrap(false);
        return textWidth;
    }

    float originalSize = font.pointSizeF();
    if (originalSize <= 0) originalSize = 10.0f;

    QFont smallerFont = font;
    smallerFont.setPointSizeF(originalSize * 0.85f);
    QFontMetrics fmSmall(smallerFont);
    
    if (fmSmall.horizontalAdvance(text) <= maxWidth) {
        label->setFont(smallerFont);
        label->setWordWrap(false);
        return fmSmall.horizontalAdvance(text);
    }

    label->setFont(font); 
    label->setWordWrap(true);
    label->setMinimumWidth(maxWidth); 
    return maxWidth;
}

ToastWidget::ToastWidget(
    const QString& title,
    const QString& description,
    const QPixmap& icon,
    float scale,
    int maxWidth, 
    QWidget* parent
) : QWidget(parent)
{
    const int iconSize = int(44 * scale);
    const int marginsH = int(10 * scale) + int(12 * scale) + int(8 * scale);
    const int maxTextWidth = maxWidth - iconSize - marginsH;

    auto* layout = new QHBoxLayout(this);
    layout->setSpacing(int(8 * scale)); 
    layout->setContentsMargins(int(10 * scale), int(8 * scale), int(12 * scale), int(8 * scale));
    
    auto* iconLabel = new QLabel;
    iconLabel->setPixmap(icon.scaled(iconSize, iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLabel->setFixedSize(iconSize, iconSize);
    iconLabel->setAlignment(Qt::AlignCenter); 
    layout->addWidget(iconLabel);

    if (!title.isEmpty() || !description.isEmpty()) {
        auto* textLayout = new QVBoxLayout();
        textLayout->setSpacing(int(0 * scale));
        textLayout->setContentsMargins(0, 0, 0, 0);

        int actualTitleW = 0;
        int actualDescW = 0;

        if (!title.isEmpty()) {
            auto* titleLabel = new QLabel;
            titleLabel->setStyleSheet("font-weight: bold; color: white; background: transparent; padding-left: 2px;");
            actualTitleW = ConfigureLabelAutoFit(titleLabel, title, maxTextWidth);
            titleLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
            titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            textLayout->addWidget(titleLabel);
        }

        if (!description.isEmpty()) {
            auto* descLabel = new QLabel;
            descLabel->setStyleSheet("color: #bbb; background: transparent; padding-left: 2px;");
            actualDescW = ConfigureLabelAutoFit(descLabel, description, maxTextWidth);
            descLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
            descLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            textLayout->addWidget(descLabel);
        }

        layout->addLayout(textLayout);

        setMinimumWidth(int(120 * scale));
        setMaximumWidth(maxWidth);
        adjustSize();
    }
    else {
        setFixedWidth(iconSize + marginsH);
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
    QRect r = rect().adjusted(1, 1, -1, -1);
    p.setBrush(QColor(40, 40, 40, 230));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(r, 8, 8);
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

void ToastOverlay::ShowToast(const QString& title, const QString& description, const QPixmap& icon, bool playSound)
{
    MainWindow* mainWin = qobject_cast<MainWindow*>(parentWidget());
    if (mainWin) {
        if (mainWin->getEmuInstance()->getLocalConfig().GetBool("RetroAchievements.HideToasts")) {
            return;
        }
    }
    if (!parentWidget()) return;
    float scale = ComputeToastScale(parentWidget()->size());
    const int maxToastWidth = int(parentWidget()->width() * 0.75f);

    auto* toast = new ToastWidget(title, description, icon, scale, maxToastWidth);
    QGraphicsProxyWidget* proxy = m_scene->addWidget(toast);
    m_toastProxies.append(proxy);
    if (playSound) {
        m_soundEffect.play();
    }

    RepositionToasts();

    QTimer::singleShot(3000, this, [this, proxy]() {
        m_toastProxies.removeOne(proxy);
        m_scene->removeItem(proxy);
        if (proxy->widget()) proxy->widget()->deleteLater();
        delete proxy;
        RepositionToasts();
    });
}

void ToastOverlay::ShowChallenge(const QPixmap& icon, const QString& badgeName)
{
    MainWindow* mainWin = qobject_cast<MainWindow*>(parentWidget());
    if (mainWin) {
        if (mainWin->getEmuInstance()->getLocalConfig().GetBool("RetroAchievements.HideChallengeIndicators")) {
            return;
        }
    }

    if (!parentWidget() || icon.isNull()) return;
    
    float scale = ComputeToastScale(parentWidget()->size());
    int scaledSize = int(48 * scale); 

    QLabel* label = new QLabel();
    label->setPixmap(icon.scaled(scaledSize, scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    label->setStyleSheet("background: transparent; border: none; padding: 0px;");
    label->setAttribute(Qt::WA_TransparentForMouseEvents);
    label->adjustSize();

    QGraphicsProxyWidget* proxy = m_scene->addWidget(label);
    
    proxy->setProperty("badgeName", badgeName); 
    
    m_challengeProxies.append(proxy);
    RepositionChallenge();
}

void ToastOverlay::ShowLeaderboardToast(
    const QString& title,
    const QString& description,
    const QPixmap& icon,
    bool playSound
)
{
    MainWindow* mainWin = qobject_cast<MainWindow*>(parentWidget());
    if (mainWin)
    {
        if (mainWin->getEmuInstance()->getLocalConfig().GetBool("RetroAchievements.HideLeaderboardToasts")) {
            return;
        }
    }

    ShowToast(title, description, icon, playSound);
}

void ToastOverlay::HideChallenge(const QString& badgeName)
{
    if (badgeName.isEmpty()) {
        for (auto* proxy : m_challengeProxies) {
            m_scene->removeItem(proxy);
            if (proxy->widget()) proxy->widget()->deleteLater();
            delete proxy;
        }
        m_challengeProxies.clear();
    } else {
        for (int i = 0; i < m_challengeProxies.size(); ++i) {
            if (m_challengeProxies[i]->property("badgeName").toString() == badgeName) {
                auto* proxy = m_challengeProxies.takeAt(i);
                m_scene->removeItem(proxy);
                if (proxy->widget()) proxy->widget()->deleteLater();
                delete proxy;
                break;
            }
        }
    }
    RepositionChallenge();
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
    if (m_challengeProxies.isEmpty() || !parentWidget()) return;

    int rotMode = Config::GetLocalTable(0).GetInt("Window0.ScreenRotation");
    int angle = rotMode * 90;
    QRect pRect = parentWidget()->rect();
    
    const int margin = 20;
    const int spacing = 8;
    int currentOffset = 0;

    for (auto* proxy : m_challengeProxies) {
        proxy->widget()->adjustSize();
        QSize s = proxy->widget()->size();
        
        proxy->setTransformOriginPoint(s.width() / 2.0, s.height() / 2.0);
        proxy->setRotation(angle);

        int finalW = (rotMode % 2 == 0) ? s.width() : s.height();
        int finalH = (rotMode % 2 == 0) ? s.height() : s.width();
        
        int centerX, centerY;

        if (rotMode == 0) {
            centerX = pRect.width() - margin - (finalW / 2) - currentOffset;
            centerY = pRect.height() - margin - (finalH / 2);
        } else if (rotMode == 1) {
            centerX = margin + (finalW / 2);
            centerY = pRect.height() - margin - (finalH / 2) - currentOffset;
        } else if (rotMode == 2) {
            centerX = margin + (finalW / 2) + currentOffset;
            centerY = margin + (finalH / 2);
        } else {
            centerX = pRect.width() - margin - (finalW / 2);
            centerY = margin + (finalH / 2) + currentOffset;
        }

        proxy->setPos(centerX - s.width() / 2.0, centerY - s.height() / 2.0);
        
        currentOffset += (rotMode % 2 == 0 ? finalW : finalH) + spacing;
    }
}

void ToastOverlay::resizeEvent(QResizeEvent* event)
{
    if (parentWidget()) setGeometry(parentWidget()->rect());
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