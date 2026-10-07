#include "Database.h"

#include <QBuffer>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>
#include <algorithm>

QByteArray makeThumb(const QImage& src, int maxW, int maxH) {
    if (src.isNull()) return {};
    QImage img = src;
    if (img.width() > maxW || img.height() > maxH)
        img = img.scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray out;
    QBuffer buf(&out);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return out;
}

Database& Database::instance() {
    static Database d;
    return d;
}

QString Database::path() const {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/chat.db";
}

bool Database::open(QString* err) {
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(path());
    if (!db.open()) {
        if (err) *err = db.lastError().text();
        return false;
    }
    QSqlQuery q;
    q.exec("PRAGMA journal_mode=WAL");
    q.exec("PRAGMA foreign_keys=ON");
    const QStringList ddl = {
        "CREATE TABLE IF NOT EXISTS messages("
        " id INTEGER PRIMARY KEY AUTOINCREMENT, ts INTEGER NOT NULL, text TEXT NOT NULL DEFAULT '')",
        "CREATE TABLE IF NOT EXISTS attachments("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " message_id INTEGER NOT NULL REFERENCES messages(id) ON DELETE CASCADE,"
        " name TEXT, kind TEXT, mime TEXT, size INTEGER, data BLOB, thumb BLOB)",
        "CREATE TABLE IF NOT EXISTS links("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " message_id INTEGER NOT NULL REFERENCES messages(id) ON DELETE CASCADE,"
        " url TEXT, title TEXT, description TEXT, site TEXT, thumb BLOB,"
        " fetched INTEGER NOT NULL DEFAULT 0)",
        "CREATE INDEX IF NOT EXISTS idx_messages_ts ON messages(ts)",
        "CREATE INDEX IF NOT EXISTS idx_att_msg ON attachments(message_id)",
        "CREATE INDEX IF NOT EXISTS idx_links_msg ON links(message_id)"};
    for (const QString& s : ddl) {
        if (!q.exec(s)) {
            if (err) *err = q.lastError().text();
            return false;
        }
    }
    return true;
}

QString Database::normalize(const QString& s) {
    const QString d = s.normalized(QString::NormalizationForm_D);
    QString out;
    out.reserve(d.size());
    for (QChar c : d)
        if (c.category() != QChar::Mark_NonSpacing) out += c;
    return out.toCaseFolded();
}

qint64 Database::addMessage(const QString& text, const QList<Attachment>& atts, const QStringList& urls) {
    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();
    QSqlQuery q;
    q.prepare("INSERT INTO messages(ts,text) VALUES(?,?)");
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    q.addBindValue(text);
    if (!q.exec()) {
        db.rollback();
        return 0;
    }
    const qint64 id = q.lastInsertId().toLongLong();
    for (const Attachment& a : atts) {
        QSqlQuery qa;
        qa.prepare("INSERT INTO attachments(message_id,name,kind,mime,size,data,thumb) VALUES(?,?,?,?,?,?,?)");
        qa.addBindValue(id);
        qa.addBindValue(a.name);
        qa.addBindValue(a.kind);
        qa.addBindValue(a.mime);
        qa.addBindValue(a.size);
        qa.addBindValue(a.data);
        qa.addBindValue(a.thumb);
        if (!qa.exec()) {
            db.rollback();
            return 0;
        }
    }
    for (const QString& u : urls) {
        QSqlQuery ql;
        ql.prepare("INSERT INTO links(message_id,url) VALUES(?,?)");
        ql.addBindValue(id);
        ql.addBindValue(u);
        ql.exec();
    }
    db.commit();
    return id;
}

QList<qint64> Database::search(const Filter& f, int limit) {
    QStringList where;
    QVariantList args;
    if (f.type == "text")
        where << "TRIM(m.text) <> ''";
    else if (f.type == "image")
        where << "EXISTS(SELECT 1 FROM attachments a WHERE a.message_id=m.id AND a.kind='image')";
    else if (f.type == "file")
        where << "EXISTS(SELECT 1 FROM attachments a WHERE a.message_id=m.id AND a.kind='file')";
    else if (f.type == "link")
        where << "EXISTS(SELECT 1 FROM links l WHERE l.message_id=m.id)";
    if (f.useDates) {
        where << "m.ts >= ? AND m.ts < ?";
        args << f.from.startOfDay().toMSecsSinceEpoch() << f.to.addDays(1).startOfDay().toMSecsSinceEpoch();
    }

    // Texto + nombres de archivos + url/título/descripción/sitio de los links
    QString sql =
        "SELECT m.id, m.text || ' ' ||"
        " IFNULL((SELECT group_concat(name,' ') FROM attachments WHERE message_id=m.id),'') || ' ' ||"
        " IFNULL((SELECT group_concat(IFNULL(url,'')||' '||IFNULL(title,'')||' '||IFNULL(description,'')||' '||IFNULL(site,''),' ')"
        "         FROM links WHERE message_id=m.id),'')"
        " FROM messages m";
    if (!where.isEmpty()) sql += " WHERE " + where.join(" AND ");
    sql += " ORDER BY m.ts DESC";

    const QStringList terms = normalize(f.text).split(' ', Qt::SkipEmptyParts);

    QSqlQuery q;
    q.prepare(sql);
    for (const QVariant& a : args) q.addBindValue(a);
    q.exec();

    QList<qint64> ids;
    while (q.next()) {
        if (!terms.isEmpty()) {
            const QString hay = normalize(q.value(1).toString());
            bool ok = true;
            for (const QString& t : terms)
                if (!hay.contains(t)) { ok = false; break; }
            if (!ok) continue;
        }
        ids << q.value(0).toLongLong();
        if (ids.size() >= limit) break;
    }
    std::reverse(ids.begin(), ids.end());
    return ids;
}

Message Database::load(qint64 id) {
    Message m;
    QSqlQuery q;
    q.prepare("SELECT ts,text FROM messages WHERE id=?");
    q.addBindValue(id);
    q.exec();
    if (!q.next()) return m;
    m.id = id;
    m.ts = QDateTime::fromMSecsSinceEpoch(q.value(0).toLongLong());
    m.text = q.value(1).toString();

    QSqlQuery qa;
    qa.prepare("SELECT id,name,kind,mime,size,thumb FROM attachments WHERE message_id=? ORDER BY id");
    qa.addBindValue(id);
    qa.exec();
    while (qa.next()) {
        Attachment a;
        a.id = qa.value(0).toLongLong();
        a.name = qa.value(1).toString();
        a.kind = qa.value(2).toString();
        a.mime = qa.value(3).toString();
        a.size = qa.value(4).toLongLong();
        a.thumb = qa.value(5).toByteArray();
        m.atts << a;
    }

    QSqlQuery ql;
    ql.prepare("SELECT id FROM links WHERE message_id=? ORDER BY id");
    ql.addBindValue(id);
    ql.exec();
    while (ql.next()) m.links << link(ql.value(0).toLongLong());
    return m;
}

LinkInfo Database::link(qint64 id) {
    LinkInfo l;
    QSqlQuery q;
    q.prepare("SELECT url,title,description,site,thumb,fetched FROM links WHERE id=?");
    q.addBindValue(id);
    q.exec();
    if (q.next()) {
        l.id = id;
        l.url = q.value(0).toString();
        l.title = q.value(1).toString();
        l.desc = q.value(2).toString();
        l.site = q.value(3).toString();
        l.thumb = q.value(4).toByteArray();
        l.fetched = q.value(5).toInt() != 0;
    }
    return l;
}

QByteArray Database::attachmentData(qint64 attId) {
    QSqlQuery q;
    q.prepare("SELECT data FROM attachments WHERE id=?");
    q.addBindValue(attId);
    q.exec();
    return q.next() ? q.value(0).toByteArray() : QByteArray();
}

void Database::updateLink(qint64 id, const QString& title, const QString& desc, const QString& site,
                          const QByteArray& thumb) {
    QSqlQuery q;
    q.prepare("UPDATE links SET title=?,description=?,site=?,thumb=?,fetched=1 WHERE id=?");
    q.addBindValue(title);
    q.addBindValue(desc);
    q.addBindValue(site);
    q.addBindValue(thumb);
    q.addBindValue(id);
    q.exec();
}

void Database::removeMessage(qint64 id) {
    QSqlQuery q;
    for (const char* t : {"attachments", "links"}) {
        q.prepare(QString("DELETE FROM %1 WHERE message_id=?").arg(t));
        q.addBindValue(id);
        q.exec();
    }
    q.prepare("DELETE FROM messages WHERE id=?");
    q.addBindValue(id);
    q.exec();
}
