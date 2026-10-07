#include "LinkFetcher.h"

#include "Database.h"

#include <QHash>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStringConverter>
#include <QStringList>
#include <QTextDocumentFragment>
#include <QUrlQuery>
#include <memory>

static QString cleanText(const QString& s) {
    return QTextDocumentFragment::fromHtml(s).toPlainText().simplified();
}

QNetworkReply* LinkFetcher::get(const QUrl& u) {
    QNetworkRequest r(u);
    r.setRawHeader("User-Agent",
                   "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
                   "Chrome/124.0 Safari/537.36");
    r.setRawHeader("Accept-Language", "es-AR,es;q=0.9,en;q=0.8");
    r.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    r.setTransferTimeout(12000);
    return nam_.get(r);
}

QString LinkFetcher::youtubeId(const QUrl& u) {
    QString h = u.host().toLower();
    if (h.startsWith("www.")) h = h.mid(4);
    if (h.startsWith("m.")) h = h.mid(2);
    if (h == "youtu.be") return u.path().section('/', 1, 1);
    if (h == "youtube.com" || h == "music.youtube.com" || h == "youtube-nocookie.com") {
        if (u.path() == "/watch") return QUrlQuery(u).queryItemValue("v");
        for (const QString& p : QStringList{"/shorts/", "/embed/", "/live/"})
            if (u.path().startsWith(p)) return u.path().mid(p.size()).section('/', 0, 0);
    }
    return {};
}

void LinkFetcher::finish(qint64 id, const QString& title, const QString& desc, const QString& site,
                         const QByteArray& thumb) {
    Database::instance().updateLink(id, title, desc, site, thumb);
    emit updated(id);
}

void LinkFetcher::downloadThumb(qint64 id, const QUrl& u, QString title, QString desc, QString site) {
    QNetworkReply* r = get(u);
    connect(r, &QNetworkReply::finished, this, [=]() {
        r->deleteLater();
        QByteArray png;
        if (r->error() == QNetworkReply::NoError) png = makeThumb(QImage::fromData(r->readAll()), 480, 270);
        finish(id, title, desc, site, png);
    });
}

void LinkFetcher::fetch(qint64 id, const QUrl& url) {
    // ---- YouTube: oEmbed (título + canal) y miniatura 16:9 ----
    const QString yt = youtubeId(url);
    if (!yt.isEmpty()) {
        QUrl o("https://www.youtube.com/oembed");
        QUrlQuery q;
        q.addQueryItem("url", "https://www.youtube.com/watch?v=" + yt);
        q.addQueryItem("format", "json");
        o.setQuery(q);
        QNetworkReply* r = get(o);
        connect(r, &QNetworkReply::finished, this, [=]() {
            r->deleteLater();
            QString title, author;
            const bool offline = r->error() != QNetworkReply::NoError &&
                                 !r->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid();
            if (offline) return;  // sin conexión: se reintenta la próxima vez
            if (r->error() == QNetworkReply::NoError) {
                const QJsonObject obj = QJsonDocument::fromJson(r->readAll()).object();
                title = obj["title"].toString();
                author = obj["author_name"].toString();
            }
            downloadThumb(id, QUrl("https://i.ytimg.com/vi/" + yt + "/mqdefault.jpg"), title, author, "YouTube");
        });
        return;
    }

    // ---- Cualquier otra página: etiquetas OpenGraph / Twitter / <title> ----
    QNetworkReply* r = get(url);
    auto buf = std::make_shared<QByteArray>();
    connect(r, &QNetworkReply::readyRead, this, [=]() {
        const QString ct = r->header(QNetworkRequest::ContentTypeHeader).toString().toLower();
        const bool img = ct.startsWith("image/");
        const bool ok = ct.isEmpty() || ct.contains("html") || ct.contains("xml") || img;
        if (!ok || buf->size() > (img ? 15000000 : 1500000)) {  // no bajamos archivos grandes
            r->abort();
            return;
        }
        buf->append(r->readAll());
    });
    connect(r, &QNetworkReply::finished, this, [=]() {
        r->deleteLater();
        buf->append(r->readAll());
        const int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 0 && buf->isEmpty()) return;  // sin respuesta: se reintenta luego

        const QString ct = r->header(QNetworkRequest::ContentTypeHeader).toString().toLower();
        QString host = url.host();
        if (host.startsWith("www.")) host = host.mid(4);

        if (ct.startsWith("image/")) {  // link directo a una imagen
            finish(id, url.fileName(), "", host, makeThumb(QImage::fromData(*buf), 480, 270));
            return;
        }
        if (buf->isEmpty() || (!ct.isEmpty() && !ct.contains("html"))) {
            finish(id, url.fileName(), "", host, {});
            return;
        }

        // charset
        static const QRegularExpression csRe("charset\\s*=\\s*[\"']?([A-Za-z0-9_-]+)",
                                             QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch cm = csRe.match(ct);
        if (!cm.hasMatch()) cm = csRe.match(QString::fromLatin1(buf->left(4096)));
        const QByteArray csName = cm.hasMatch() ? cm.captured(1).toLatin1() : QByteArray("UTF-8");
        QStringDecoder dec(csName.constData());
        const QString html = dec.isValid() ? QString(dec(*buf)) : QString::fromUtf8(*buf);

        // meta tags
        static const QRegularExpression metaRe("<meta\\s[^>]*>", QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression keyRe("(?:property|name)\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)')",
                                              QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression valRe("content\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)')",
                                              QRegularExpression::CaseInsensitiveOption);
        QHash<QString, QString> meta;
        auto it = metaRe.globalMatch(html);
        while (it.hasNext()) {
            const QString tag = it.next().captured(0);
            const auto k = keyRe.match(tag);
            const auto v = valRe.match(tag);
            if (!k.hasMatch() || !v.hasMatch()) continue;
            const QString key = (k.captured(1).isEmpty() ? k.captured(2) : k.captured(1)).toLower();
            const QString val = v.captured(1).isEmpty() ? v.captured(2) : v.captured(1);
            if (!meta.contains(key)) meta.insert(key, val);
        }
        auto pick = [&](std::initializer_list<const char*> keys) {
            for (const char* k : keys)
                if (!meta.value(k).trimmed().isEmpty()) return meta.value(k);
            return QString();
        };

        QString title = pick({"og:title", "twitter:title"});
        if (title.isEmpty()) {
            static const QRegularExpression tRe("<title[^>]*>(.*?)</title>",
                                                QRegularExpression::CaseInsensitiveOption |
                                                    QRegularExpression::DotMatchesEverythingOption);
            title = tRe.match(html).captured(1);
        }
        QString desc = pick({"og:description", "description", "twitter:description"});
        QString site = pick({"og:site_name"});
        if (site.isEmpty()) site = host;
        const QString img = pick({"og:image", "og:image:url", "twitter:image", "twitter:image:src"});

        title = cleanText(title).left(160);
        desc = cleanText(desc).left(220);
        site = cleanText(site);

        if (!img.isEmpty()) {
            downloadThumb(id, url.resolved(QUrl(cleanText(img))), title, desc, site);
        } else {
            finish(id, title, desc, site, {});
        }
    });
}
