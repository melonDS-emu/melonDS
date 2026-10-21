#pragma once
#include <QPixmap>
#include <QNetworkAccessManager>

class BadgeCache : public QObject
{
    Q_OBJECT
public:
    explicit BadgeCache(QObject* parent = nullptr);

    QPixmap GetBadge(const QString& url);
    void DownloadBadge(const QString& url,
                       std::function<void(const QPixmap&)> callback);

signals:
    void BadgeReady(const QString& url, const QPixmap& pix);

private:
    QNetworkAccessManager m_net;
    QHash<QString, QPixmap> m_cache;
};