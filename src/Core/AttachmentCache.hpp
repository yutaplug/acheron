#pragma once

#include <QObject>
#include <QUrl>
#include <QSize>
#include <QPixmap>
#include <QTemporaryDir>
#include <QHash>
#include <QSet>

class QNetworkAccessManager;

namespace Acheron {
namespace Core {

class AttachmentCache : public QObject
{
    Q_OBJECT
public:
    explicit AttachmentCache(QObject *parent = nullptr);

    static constexpr int MaxWidth = 400;
    static constexpr int MaxHeight = 300;

    [[nodiscard]] QPixmap get(const QUrl &proxyUrl, const QSize &originalSize);
    [[nodiscard]] bool isCached(const QUrl &proxyUrl) const;
    [[nodiscard]] static QSize calculateDisplaySize(const QSize &original);

signals:
    void attachmentFetched(const QUrl &url, const QPixmap &pixmap);

private:
    void fetchFromNetwork(const QUrl &url, const QSize &displaySize);
    QString getCachePath(const QUrl &url) const;
    QPixmap placeholder(const QSize &size);

    QTemporaryDir tempDir;
    QNetworkAccessManager *networkManager;
    QSet<QUrl> pendingRequests;
    QHash<QUrl, QPixmap> memoryCache;
};

} // namespace Core
} // namespace Acheron
