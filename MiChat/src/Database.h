#pragma once
#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QImage>
#include <QList>
#include <QString>

struct Attachment {
    qint64 id = 0;
    QString name, kind, mime;   // kind: "image" | "file"
    qint64 size = 0;
    QByteArray data;            // contenido completo (solo al guardar)
    QByteArray thumb;           // miniatura PNG (solo imágenes)
};

struct LinkInfo {
    qint64 id = 0;
    QString url, title, desc, site;
    QByteArray thumb;
    bool fetched = false;
};

struct Message {
    qint64 id = 0;
    QDateTime ts;
    QString text;
    QList<Attachment> atts;
    QList<LinkInfo> links;
};

struct Filter {
    QString text;
    QString type = "all";       // all | text | image | file | link
    bool useDates = false;
    QDate from, to;
    bool active() const { return !text.trimmed().isEmpty() || type != "all" || useDates; }
};

// Escala una imagen a un máximo de maxW x maxH y la devuelve como PNG.
QByteArray makeThumb(const QImage& img, int maxW, int maxH);

class Database {
public:
    static Database& instance();
    bool open(QString* err);
    QString path() const;

    qint64 addMessage(const QString& text, const QList<Attachment>& atts, const QStringList& urls);
    QList<qint64> search(const Filter& f, int limit);     // del más viejo al más nuevo
    Message load(qint64 id);
    QByteArray attachmentData(qint64 attId);
    LinkInfo link(qint64 id);
    void updateLink(qint64 id, const QString& title, const QString& desc, const QString& site, const QByteArray& thumb);
    void removeMessage(qint64 id);

    // minúsculas + sin acentos, para buscar "nino" y encontrar "Niño"
    static QString normalize(const QString& s);
};
