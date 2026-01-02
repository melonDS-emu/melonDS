#pragma once
#include <QObject>
#include <QPixmap>
#include <QHash>
#include <QNetworkAccessManager>
#include <functional>

class BadgeCache : public QObject
{
    Q_OBJECT
public:
    static BadgeCache& Get();

    QPixmap GetBadge(const QString& url);
    void DownloadBadge(const QString& url, std::function<void(const QPixmap&)> callback);

private:
    BadgeCache(QObject* parent = nullptr);
    QNetworkAccessManager m_net;
    QHash<QString, QPixmap> m_cache;
signals:
    void BadgeReady(const QString& url, const QPixmap& pix); // <-- dodaj tu
};