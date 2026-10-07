#include "MainWindow.h"

#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDateEdit>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMimeDatabase>
#include <QMouseEvent>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QStandardPaths>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <functional>

#include "ChatInput.h"
#include "LinkFetcher.h"

namespace {

// ---------- helpers de links ----------
const QRegularExpression& urlRe() {
    static const QRegularExpression r("\\b(?:https?://|www\\.)[^\\s<>\"]+", QRegularExpression::CaseInsensitiveOption);
    return r;
}

QString cleanUrl(QString u) {
    const QString trail = ".,;:!?)]}'\"";
    while (!u.isEmpty() && trail.contains(u.back())) u.chop(1);
    if (u.startsWith("www.", Qt::CaseInsensitive)) u = "https://" + u;
    return u;
}

QStringList extractUrls(const QString& text) {
    QStringList out;
    auto it = urlRe().globalMatch(text);
    while (it.hasNext()) {
        const QString u = cleanUrl(it.next().captured(0));
        if (QUrl(u).isValid() && !out.contains(u)) out << u;
        if (out.size() >= 5) break;
    }
    return out;
}

// Texto que se muestra en la burbuja: sin los links "pelados" (para eso está la tarjeta).
QString displayText(const QString& t) {
    QString s = t;
    s.replace(urlRe(), "");
    return s.trimmed();
}

// Patrón de resaltado que ignora acentos y mayúsculas
QString accentPattern(const QString& term) {
    static const QHash<QChar, QString> m = {{'a', "aàáâãäå"}, {'e', "eèéêë"}, {'i', "iìíîï"}, {'o', "oòóôõö"},
                                            {'u', "uùúûü"},   {'n', "nñ"},    {'c', "cç"}};
    QString out;
    for (QChar c : Database::normalize(term)) {
        auto it = m.find(c);
        out += it != m.end() ? "[" + it.value() + "]" : QRegularExpression::escape(QString(c));
    }
    return out;
}

QString fileSafe(const QString& name) {
    const QString n = QFileInfo(name).fileName();
    return n.isEmpty() ? "archivo" : n;
}

// ---------- widgets simples ----------
struct ClickLabel : QLabel {
    std::function<void()> onClick;
    std::function<void(const QPoint&)> onMenu;
    using QLabel::QLabel;
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && onClick) onClick();
    }
    void contextMenuEvent(QContextMenuEvent* e) override {
        if (onMenu) onMenu(e->globalPos());
    }
};

}  // namespace

// Tarjeta de vista previa de un link (miniatura + título + sitio)
class LinkCard : public QFrame {
public:
    explicit LinkCard(const LinkInfo& li) {
        setObjectName("card");
        setCursor(Qt::PointingHandCursor);
        setFixedWidth(320);
        auto* v = new QVBoxLayout(this);
        v->setContentsMargins(1, 1, 1, 8);
        v->setSpacing(3);
        thumb_ = new QLabel;
        thumb_->setAlignment(Qt::AlignCenter);
        title_ = new QLabel;
        title_->setWordWrap(true);
        desc_ = new QLabel;
        desc_->setWordWrap(true);
        site_ = new QLabel;
        title_->setStyleSheet("font-weight:600; font-size:13px; padding:0 10px;");
        desc_->setStyleSheet("color:#54656f; font-size:12px; padding:0 10px;");
        site_->setStyleSheet("color:#8696a0; font-size:11px; padding:0 10px;");
        for (QLabel* l : {thumb_, title_, desc_, site_}) v->addWidget(l);
        setInfo(li);
    }

    void setInfo(const LinkInfo& li) {
        url_ = li.url;
        QString host = QUrl(li.url).host();
        if (host.startsWith("www.")) host = host.mid(4);

        QPixmap pm;
        if (!li.thumb.isEmpty()) pm.loadFromData(li.thumb);
        thumb_->setVisible(!pm.isNull());
        if (!pm.isNull()) {
            if (pm.width() > 318) pm = pm.scaledToWidth(318, Qt::SmoothTransformation);
            if (pm.height() > 200) pm = pm.copy(0, (pm.height() - 200) / 2, pm.width(), 200);
            thumb_->setPixmap(pm);
        }

        QString t = li.title;
        if (t.isEmpty()) t = fontMetrics().elidedText(li.url, Qt::ElideMiddle, 290);
        title_->setTextFormat(Qt::PlainText);
        title_->setText(t);

        desc_->setText(li.desc.left(140) + (li.desc.size() > 140 ? "…" : ""));
        desc_->setVisible(!li.desc.isEmpty());
        site_->setText((li.site.isEmpty() ? host : li.site).toUpper());
        setToolTip(li.url);
    }

protected:
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) QDesktopServices::openUrl(QUrl(url_));
    }
    void contextMenuEvent(QContextMenuEvent* e) override {
        QMenu m;
        QAction* open = m.addAction("Abrir en el navegador");
        QAction* copy = m.addAction("Copiar link");
        QAction* r = m.exec(e->globalPos());
        if (r == open) QDesktopServices::openUrl(QUrl(url_));
        else if (r == copy) QGuiApplication::clipboard()->setText(url_);
    }

private:
    QString url_;
    QLabel *thumb_, *title_, *desc_, *site_;
};

// =====================================================================

MainWindow::MainWindow() {
    setWindowTitle("Mi Chat");
    resize(920, 740);
    setAcceptDrops(true);

    fetcher_ = new LinkFetcher(this);
    connect(fetcher_, &LinkFetcher::updated, this, &MainWindow::onLinkUpdated);

    auto* central = new QWidget;
    setCentralWidget(central);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---------- barra de filtros ----------
    auto* bar = new QWidget;
    bar->setObjectName("bar");
    auto* bl = new QHBoxLayout(bar);
    bl->setContentsMargins(10, 8, 10, 8);
    search_ = new QLineEdit;
    search_->setPlaceholderText("Buscar en mensajes, archivos y links…  (Ctrl+F)");
    search_->setClearButtonEnabled(true);
    type_ = new QComboBox;
    type_->addItem("Todo", "all");
    type_->addItem("Solo texto", "text");
    type_->addItem("Imágenes", "image");
    type_->addItem("Archivos", "file");
    type_->addItem("Links", "link");
    dateOn_ = new QCheckBox("Fecha:");
    from_ = new QDateEdit(QDate::currentDate());
    to_ = new QDateEdit(QDate::currentDate());
    for (QDateEdit* d : {from_, to_}) {
        d->setCalendarPopup(true);
        d->setDisplayFormat("dd/MM/yyyy");
        d->setEnabled(false);
    }
    auto* clearBtn = new QPushButton("Limpiar");
    count_ = new QLabel;
    count_->setObjectName("count");
    bl->addWidget(search_, 1);
    bl->addWidget(type_);
    bl->addWidget(dateOn_);
    bl->addWidget(from_);
    bl->addWidget(new QLabel("a"));
    bl->addWidget(to_);
    bl->addWidget(clearBtn);
    bl->addWidget(count_);
    root->addWidget(bar);

    // ---------- lista de mensajes ----------
    scroll_ = new QScrollArea;
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list_ = new QWidget;
    list_->setObjectName("chat");
    listLay_ = new QVBoxLayout(list_);
    listLay_->setContentsMargins(16, 12, 16, 12);
    listLay_->setSpacing(6);
    listLay_->addStretch();
    scroll_->setWidget(list_);
    root->addWidget(scroll_, 1);
    connect(scroll_->verticalScrollBar(), &QScrollBar::rangeChanged, this, [this](int, int max) {
        if (stick_) scroll_->verticalScrollBar()->setValue(max);
    });

    // ---------- adjuntos pendientes ----------
    pendingScroll_ = new QScrollArea;
    pendingScroll_->setWidgetResizable(true);
    pendingScroll_->setFixedHeight(86);
    pendingScroll_->setFrameShape(QFrame::NoFrame);
    pendingScroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pendingBar_ = new QWidget;
    pendingBar_->setObjectName("pending");
    pendingLay_ = new QHBoxLayout(pendingBar_);
    pendingLay_->setContentsMargins(10, 4, 10, 4);
    pendingScroll_->setWidget(pendingBar_);
    pendingScroll_->hide();
    root->addWidget(pendingScroll_);

    // ---------- entrada ----------
    auto* inputRow = new QWidget;
    inputRow->setObjectName("bar");
    auto* il = new QHBoxLayout(inputRow);
    il->setContentsMargins(10, 8, 10, 8);
    auto* attach = new QToolButton;
    attach->setText("📎");
    attach->setToolTip("Adjuntar archivos");
    input_ = new ChatInput;
    input_->setPlaceholderText("Escribí un mensaje, pegá una imagen (Ctrl+V) o arrastrá archivos…");
    input_->setFixedHeight(64);
    auto* sendBtn = new QPushButton("Enviar");
    sendBtn->setFixedHeight(40);
    il->addWidget(attach);
    il->addWidget(input_, 1);
    il->addWidget(sendBtn);
    root->addWidget(inputRow);

    // ---------- conexiones ----------
    debounce_ = new QTimer(this);
    debounce_->setSingleShot(true);
    debounce_->setInterval(250);
    connect(debounce_, &QTimer::timeout, this, [this] { reload(); });
    connect(search_, &QLineEdit::textChanged, this, [this] { debounce_->start(); });
    connect(type_, &QComboBox::currentIndexChanged, this, [this] { reload(); });
    connect(dateOn_, &QCheckBox::toggled, this, [this](bool on) {
        from_->setEnabled(on);
        to_->setEnabled(on);
        reload();
    });
    connect(from_, &QDateEdit::dateChanged, this, [this] { if (dateOn_->isChecked()) reload(); });
    connect(to_, &QDateEdit::dateChanged, this, [this] { if (dateOn_->isChecked()) reload(); });
    connect(clearBtn, &QPushButton::clicked, this, [this] { clearFilters(); });
    new QShortcut(QKeySequence::Find, this, [this] {
        search_->setFocus();
        search_->selectAll();
    });

    connect(sendBtn, &QPushButton::clicked, this, [this] { send(); });
    connect(input_, &ChatInput::sendRequested, this, [this] { send(); });
    connect(input_, &ChatInput::imagePasted, this, [this](const QImage& i) { addPendingImage(i); });
    connect(input_, &ChatInput::filesPasted, this, [this](const QStringList& f) { addPendingFiles(f); });
    connect(attach, &QToolButton::clicked, this, [this] {
        const QStringList f = QFileDialog::getOpenFileNames(this, "Adjuntar archivos");
        if (!f.isEmpty()) addPendingFiles(f);
    });

    reload();
    input_->setFocus();
}

// ---------------------------------------------------------------------
// Filtros y carga
// ---------------------------------------------------------------------

Filter MainWindow::currentFilter() const {
    Filter f;
    f.text = search_->text();
    f.type = type_->currentData().toString();
    f.useDates = dateOn_->isChecked();
    f.from = from_->date();
    f.to = to_->date();
    if (f.to < f.from) std::swap(f.from, f.to);
    return f;
}

void MainWindow::clearFilters() {
    for (QWidget* w : QList<QWidget*>{search_, type_, dateOn_, from_, to_}) w->blockSignals(true);
    search_->clear();
    type_->setCurrentIndex(0);
    dateOn_->setChecked(false);
    from_->setEnabled(false);
    to_->setEnabled(false);
    for (QWidget* w : QList<QWidget*>{search_, type_, dateOn_, from_, to_}) w->blockSignals(false);
    reload();
}

void MainWindow::clearList() {
    while (listLay_->count() > 1) {
        QLayoutItem* it = listLay_->takeAt(0);
        if (QWidget* w = it->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete it;
    }
    cards_.clear();
    emptyLabel_ = nullptr;
}

void MainWindow::updateCount() {
    const bool active = currentFilter().active();
    count_->setText(QString("%1 %2").arg(shown_).arg(active ? "resultado(s)" : "mensaje(s)"));
}

void MainWindow::keepBottom() {
    stick_ = true;
    QTimer::singleShot(700, this, [this] { stick_ = false; });
}

void MainWindow::reload() {
    const Filter f = currentFilter();
    terms_.clear();
    for (const QString& t : f.text.split(' ', Qt::SkipEmptyParts))
        if (!accentPattern(t).isEmpty()) terms_ << t;

    clearList();
    lastDay_ = QDate();
    shown_ = 0;
    const QList<qint64> ids = Database::instance().search(f, f.active() ? 1000 : 500);
    keepBottom();
    for (qint64 id : ids) appendMessage(id);

    if (ids.isEmpty()) {
        emptyLabel_ = new QLabel(f.active() ? "No se encontró nada con ese filtro."
                                            : "Todavía no hay mensajes.\nEscribí algo o pegá una imagen para empezar.");
        emptyLabel_->setAlignment(Qt::AlignCenter);
        emptyLabel_->setStyleSheet("color:#667781; padding:40px;");
        listLay_->insertWidget(0, emptyLabel_);
    }
    updateCount();
}

void MainWindow::appendMessage(qint64 id) {
    const Message m = Database::instance().load(id);
    if (!m.id) return;
    if (emptyLabel_) {
        emptyLabel_->hide();
        emptyLabel_->deleteLater();
        emptyLabel_ = nullptr;
    }

    const QDate day = m.ts.date();
    if (day != lastDay_) {
        lastDay_ = day;
        QString txt;
        if (day == QDate::currentDate()) txt = "Hoy";
        else if (day == QDate::currentDate().addDays(-1)) txt = "Ayer";
        else txt = QLocale(QLocale::Spanish).toString(day, "dddd d 'de' MMMM yyyy");
        auto* chip = new QLabel(txt);
        chip->setObjectName("day");
        auto* row = new QWidget;
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 8, 0, 4);
        rl->addStretch();
        rl->addWidget(chip);
        rl->addStretch();
        listLay_->insertWidget(listLay_->count() - 1, row);
    }

    auto* row = new QWidget;
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->addStretch();
    rl->addWidget(makeBubble(m));
    listLay_->insertWidget(listLay_->count() - 1, row);
    ++shown_;
}

// ---------------------------------------------------------------------
// Burbujas
// ---------------------------------------------------------------------

QString MainWindow::toHtml(const QString& plain) const {
    QString h = plain.toHtmlEscaped();
    if (!terms_.isEmpty()) {
        QStringList pats;
        for (const QString& t : terms_) pats << accentPattern(t);
        const QRegularExpression re("(" + pats.join("|") + ")",
                                    QRegularExpression::CaseInsensitiveOption |
                                        QRegularExpression::UseUnicodePropertiesOption);
        h.replace(re, "<span style=\"background-color:#ffe066;\">\\1</span>");
    }
    h.replace("\n", "<br>");
    return h;
}

QWidget* MainWindow::makeBubble(const Message& m) {
    auto* b = new QFrame;
    b->setObjectName("bubble");
    b->setMaximumWidth(560);
    b->setContextMenuPolicy(Qt::CustomContextMenu);
    auto* v = new QVBoxLayout(b);
    v->setContentsMargins(10, 8, 10, 6);
    v->setSpacing(6);

    for (const Attachment& a : m.atts)
        if (a.kind == "image") v->addWidget(makeImage(a));
    for (const Attachment& a : m.atts)
        if (a.kind != "image") v->addWidget(makeFile(a));
    for (const LinkInfo& li : m.links) v->addWidget(makeCard(li));

    const QString txt = displayText(m.text);
    if (!txt.isEmpty()) {
        auto* t = new QLabel(toHtml(txt));
        t->setTextFormat(Qt::RichText);
        t->setWordWrap(true);
        t->setTextInteractionFlags(Qt::TextSelectableByMouse);
        t->setContextMenuPolicy(Qt::NoContextMenu);  // el menú lo da la burbuja
        t->setMaximumWidth(520);
        // QLabel con wordWrap suele elegir un ancho muy angosto: calculamos el natural.
        const int w = t->fontMetrics().boundingRect(QRect(0, 0, 520, 100000), Qt::TextWordWrap, txt).width() + 6;
        t->setMinimumWidth(qMin(w, 520));
        v->addWidget(t);
    }

    auto* time = new QLabel(m.ts.toString("HH:mm"));
    time->setObjectName("time");
    time->setAlignment(Qt::AlignRight);
    v->addWidget(time);

    const qint64 id = m.id;
    const QString text = m.text;
    connect(b, &QWidget::customContextMenuRequested, this, [this, b, id, text](const QPoint& p) {
        QMenu menu;
        QAction* copy = menu.addAction("Copiar texto");
        copy->setEnabled(!text.isEmpty());
        QAction* del = menu.addAction("Eliminar mensaje");
        QAction* r = menu.exec(b->mapToGlobal(p));
        if (r == copy) {
            QGuiApplication::clipboard()->setText(text);
        } else if (r == del) {
            if (QMessageBox::question(this, "Eliminar", "¿Eliminar este mensaje y sus adjuntos?") ==
                QMessageBox::Yes) {
                Database::instance().removeMessage(id);
                if (QWidget* row = b->parentWidget()) {
                    row->hide();
                    row->deleteLater();
                }
                --shown_;
                updateCount();
            }
        }
    });
    return b;
}

QWidget* MainWindow::makeImage(const Attachment& a) {
    auto* l = new ClickLabel;
    QPixmap pm;
    pm.loadFromData(a.thumb);
    if (pm.isNull()) l->setText("[imagen: " + a.name + "]");
    else l->setPixmap(pm);
    l->setCursor(Qt::PointingHandCursor);
    l->setToolTip(a.name + "\nClick para ampliar · clic derecho para copiar o guardar");
    const qint64 id = a.id;
    const QString name = a.name;
    l->onClick = [this, id, name] { showImage(id, name); };
    l->onMenu = [this, id, name](const QPoint& gp) {
        QMenu m;
        QAction* c = m.addAction("Copiar imagen");
        QAction* s = m.addAction("Guardar imagen como…");
        QAction* o = m.addAction("Ampliar");
        QAction* r = m.exec(gp);
        if (r == c) copyImage(id);
        else if (r == s) saveAttachment(id, name);
        else if (r == o) showImage(id, name);
    };
    return l;
}

QWidget* MainWindow::makeFile(const Attachment& a) {
    auto* f = new QFrame;
    f->setObjectName("file");
    auto* h = new QHBoxLayout(f);
    h->setContentsMargins(10, 8, 10, 8);
    auto* icon = new QLabel("📄");
    icon->setStyleSheet("font-size:22px;");
    auto* info = new QVBoxLayout;
    auto* name = new QLabel(f->fontMetrics().elidedText(a.name, Qt::ElideMiddle, 230));
    name->setToolTip(a.name);
    name->setStyleSheet("font-weight:600;");
    auto* size = new QLabel(QLocale().formattedDataSize(a.size));
    size->setStyleSheet("color:#667781; font-size:11px;");
    info->addWidget(name);
    info->addWidget(size);
    auto* open = new QPushButton("Abrir");
    auto* save = new QPushButton("Guardar…");
    h->addWidget(icon);
    h->addLayout(info, 1);
    h->addWidget(open);
    h->addWidget(save);
    const qint64 id = a.id;
    const QString n = a.name;
    connect(open, &QPushButton::clicked, this, [this, id, n] { openAttachment(id, n); });
    connect(save, &QPushButton::clicked, this, [this, id, n] { saveAttachment(id, n); });
    return f;
}

QWidget* MainWindow::makeCard(const LinkInfo& li) {
    auto* c = new LinkCard(li);
    cards_[li.id] = c;
    if (!li.fetched && !tried_.contains(li.id)) {  // un intento por sesión
        tried_.insert(li.id);
        fetcher_->fetch(li.id, QUrl(li.url));
    }
    return c;
}

void MainWindow::onLinkUpdated(qint64 id) {
    auto* c = static_cast<LinkCard*>(cards_.value(id).data());
    if (!c) return;
    QScrollBar* sb = scroll_->verticalScrollBar();
    const bool atBottom = sb->value() >= sb->maximum() - 4;
    c->setInfo(Database::instance().link(id));
    if (atBottom) {
        stick_ = true;
        QTimer::singleShot(150, this, [this] { stick_ = false; });
    }
}

// ---------------------------------------------------------------------
// Acciones sobre imágenes / archivos
// ---------------------------------------------------------------------

void MainWindow::copyImage(qint64 id) {
    const QImage img = QImage::fromData(Database::instance().attachmentData(id));
    if (!img.isNull()) QGuiApplication::clipboard()->setImage(img);
}

void MainWindow::saveAttachment(qint64 id, const QString& name) {
    const QString start =
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/" + fileSafe(name);
    const QString fn = QFileDialog::getSaveFileName(this, "Guardar", start);
    if (fn.isEmpty()) return;
    const QByteArray data = Database::instance().attachmentData(id);
    QFile f(fn);
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size())
        QMessageBox::warning(this, "Guardar", "No se pudo guardar el archivo.");
}

void MainWindow::openAttachment(qint64 id, const QString& name) {
    const QString dir = QDir::tempPath() + "/MiChat";
    QDir().mkpath(dir);
    const QString fn = dir + "/" + QString::number(id) + "_" + fileSafe(name);
    QFile f(fn);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(Database::instance().attachmentData(id));
        f.close();
        QDesktopServices::openUrl(QUrl::fromLocalFile(fn));
    }
}

void MainWindow::showImage(qint64 id, const QString& name) {
    const QImage img = QImage::fromData(Database::instance().attachmentData(id));
    if (img.isNull()) {
        openAttachment(id, name);
        return;
    }
    QDialog dlg(this);
    dlg.setWindowTitle(name);
    auto* v = new QVBoxLayout(&dlg);
    const QSize maxSz = screen()->availableSize() * 0.8;
    QPixmap pm = QPixmap::fromImage(img);
    if (pm.width() > maxSz.width() || pm.height() > maxSz.height())
        pm = pm.scaled(maxSz, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    auto* l = new QLabel;
    l->setPixmap(pm);
    v->addWidget(l, 0, Qt::AlignCenter);
    auto* h = new QHBoxLayout;
    auto* c = new QPushButton("Copiar");
    auto* s = new QPushButton("Guardar como…");
    auto* x = new QPushButton("Cerrar");
    h->addStretch();
    h->addWidget(c);
    h->addWidget(s);
    h->addWidget(x);
    v->addLayout(h);
    connect(c, &QPushButton::clicked, &dlg, [this, id] { copyImage(id); });
    connect(s, &QPushButton::clicked, &dlg, [this, id, name] { saveAttachment(id, name); });
    connect(x, &QPushButton::clicked, &dlg, &QDialog::accept);
    dlg.exec();
}

// ---------------------------------------------------------------------
// Envío y adjuntos pendientes
// ---------------------------------------------------------------------

void MainWindow::send() {
    const QString text = input_->toPlainText().trimmed();
    if (text.isEmpty() && pending_.isEmpty()) return;

    const qint64 id = Database::instance().addMessage(text, pending_, extractUrls(text));
    if (!id) {
        QMessageBox::warning(this, "Mi Chat", "No se pudo guardar el mensaje en la base de datos.");
        return;
    }
    input_->clear();
    pending_.clear();
    refreshPending();
    keepBottom();
    if (currentFilter().active()) clearFilters();  // vuelve a la vista completa y muestra el mensaje nuevo
    else {
        appendMessage(id);
        updateCount();
    }
    input_->setFocus();
}

void MainWindow::addPendingImage(const QImage& img) {
    static int counter = 0;
    Attachment a;
    a.kind = "image";
    a.mime = "image/png";
    a.name = "imagen_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + "_" +
             QString::number(++counter) + ".png";
    QBuffer buf(&a.data);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    a.size = a.data.size();
    a.thumb = makeThumb(img, 360, 360);
    pending_ << a;
    refreshPending();
}

void MainWindow::addPendingFiles(const QStringList& paths) {
    const qint64 kMax = 300LL * 1024 * 1024;
    for (const QString& p : paths) {
        const QFileInfo fi(p);
        if (!fi.isFile()) {
            QMessageBox::information(this, "Mi Chat", "Las carpetas no se pueden adjuntar: " + fi.fileName());
            continue;
        }
        if (fi.size() > kMax) {
            QMessageBox::warning(this, "Mi Chat", fi.fileName() + " supera los 300 MB y no se adjuntó.");
            continue;
        }
        QFile f(p);
        if (!f.open(QIODevice::ReadOnly)) continue;
        Attachment a;
        a.name = fi.fileName();
        a.data = f.readAll();
        a.size = a.data.size();
        a.mime = QMimeDatabase().mimeTypeForFile(p).name();
        a.kind = "file";
        if (a.mime.startsWith("image/") && !a.mime.contains("svg")) {
            const QImage img = QImage::fromData(a.data);
            if (!img.isNull()) {
                a.kind = "image";
                a.thumb = makeThumb(img, 360, 360);
            }
        }
        pending_ << a;
    }
    refreshPending();
}

void MainWindow::refreshPending() {
    while (QLayoutItem* it = pendingLay_->takeAt(0)) {
        if (QWidget* w = it->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete it;
    }
    for (int i = 0; i < pending_.size(); ++i) {
        const Attachment& a = pending_[i];
        auto* chip = new QFrame;
        chip->setObjectName("chip");
        auto* h = new QHBoxLayout(chip);
        h->setContentsMargins(6, 4, 6, 4);
        auto* pic = new QLabel;
        QPixmap pm;
        if (a.kind == "image" && pm.loadFromData(a.thumb)) {
            pic->setPixmap(pm.scaled(56, 56, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            pic->setText("📄 " + chip->fontMetrics().elidedText(a.name, Qt::ElideMiddle, 120));
        }
        auto* x = new QToolButton;
        x->setText("✕");
        x->setToolTip("Quitar");
        h->addWidget(pic);
        h->addWidget(x);
        connect(x, &QToolButton::clicked, this, [this, i] {
            pending_.removeAt(i);
            refreshPending();
        });
        pendingLay_->addWidget(chip);
    }
    pendingLay_->addStretch();
    pendingScroll_->setVisible(!pending_.isEmpty());
}

// ---------------------------------------------------------------------
// Arrastrar y soltar sobre la ventana
// ---------------------------------------------------------------------

void MainWindow::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls() || e->mimeData()->hasImage()) e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* e) {
    const QMimeData* md = e->mimeData();
    QStringList files;
    QStringList links;
    for (const QUrl& u : md->urls()) {
        if (u.isLocalFile()) files << u.toLocalFile();
        else links << u.toString();
    }
    if (!files.isEmpty()) addPendingFiles(files);
    else if (!links.isEmpty()) input_->appendPlainText(links.join("\n"));
    else if (md->hasImage()) addPendingImage(qvariant_cast<QImage>(md->imageData()));
    e->acceptProposedAction();
}
