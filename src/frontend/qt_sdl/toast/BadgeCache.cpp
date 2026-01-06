#include "BadgeCache.h"
#include <QNetworkReply>
#include <QUrl>

BadgeCache& BadgeCache::Get()
{
    static BadgeCache inst;
    return inst;
}

BadgeCache::BadgeCache(QObject* parent)
    : QObject(parent)
{}

QPixmap BadgeCache::GetBadge(const QString& url)
{
    if (m_cache.contains(url))
        return m_cache[url];

    DownloadBadge(url, [](const QPixmap&){});

    return QPixmap(":/ra/placeholder.png");
}

void BadgeCache::DownloadBadge(const QString& url, std::function<void(const QPixmap&)> callback)
{
    if (m_cache.contains(url)) {
        callback(m_cache[url]);
        return;
    }

    QNetworkRequest req{ QUrl(url) };
    auto* reply = m_net.get(req);

    QObject::connect(reply, &QNetworkReply::finished, [this, reply, url, callback]() {
        QPixmap pix;
        if (reply->error() == QNetworkReply::NoError)
            pix.loadFromData(reply->readAll());

        if (pix.isNull())
            pix = QPixmap(":/ra/placeholder.png");

        m_cache[url] = pix;
        emit BadgeReady(url, pix);
        callback(pix);
        reply->deleteLater();
    });
}