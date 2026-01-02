#pragma once
#include <QWidget>
#include <QList>
#include <QTimer>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSoundEffect>

// 1. Definicja klasy ToastWidget (pojedynczy dymek)
class ToastWidget : public QWidget
{
    Q_OBJECT
public:
    ToastWidget(const QString& title, const QString& description, const QPixmap& icon, QWidget* parent = nullptr);
    QSize sizeHint() const override; // To naprawia błąd sizeHint

protected:
    void paintEvent(QPaintEvent* event) override;
};

// 2. Definicja klasy ToastOverlay (warstwa zarządzająca)
class ToastOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit ToastOverlay(QWidget* parent = nullptr); // Tylko jedna deklaracja!
    void ShowToast(const QString& title, const QString& description, const QPixmap& icon);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override; // To naprawia błąd resizeEvent
    void paintEvent(QPaintEvent* event) override;

private:
    QList<ToastWidget*> m_toasts;
    void RepositionToasts();
    QSoundEffect m_soundEffect;
};