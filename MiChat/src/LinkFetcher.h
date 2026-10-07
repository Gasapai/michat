#pragma once
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

// Descarga título, descripción y miniatura de un link y los guarda en la base de datos.
class LinkFetcher : public QObject {
    Q_OBJECT
public:
    explicit LinkFetcher(QObject* parent = nullptr) : QObject(parent) {}
    void fetch(qint64 linkId, const QUrl& url);

signals:
    void updated(qint64 linkId);

private:
    QNetworkReply* get(const QUrl& u);
    void downloadThumb(qint64 id, const QUrl& imgUrl, QString title, QString desc, QString site);
    void finish(qint64 id, const QString& title, const QString& desc, const QString& site, const QByteArray& thumb);
    static QString youtubeId(const QUrl& u);

    QNetworkAccessManager nam_;
};
