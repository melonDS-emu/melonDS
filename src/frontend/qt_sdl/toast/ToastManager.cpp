#include "ToastManager.h"
#include "toast/ToastOverlay.h"
#include "toast/AchievementToast.h"

ToastManager& ToastManager::Get()
{
    static ToastManager inst;
    return inst;
}

void ToastManager::Init(QWidget* widget)
{
    if (m_overlay) return;

    // Używamy 'widget' zamiast 'parent'
    m_overlay = new ToastOverlay(widget);
    
    // Ustaw overlay na cały rozmiar okna
    m_overlay->setGeometry(widget->rect());
    widget->installEventFilter(m_overlay);
    m_overlay->show();
    m_overlay->raise(); // Ważne: musi być na wierzchu!
}
void ToastManager::ShowAchievement(const QString& title, const QString& description, const QPixmap& icon)
{
    // JEŚLI TU JEST CRASH, TO ZNACZY ŻE m_overlay TO NULL
    if (!m_overlay) {
        printf("ToastManager: Próba pokazania toasta przed Init()!\n");
        return; 
    }

    m_overlay->ShowToast(title, description, icon);
}
void TestAchievementToast()
{
    QPixmap icon(":/ra/placeholder.png");

    ToastManager::Get().ShowAchievement(
        "Test Achievement",
        "To jest testowy toast",
        icon
    );
}