#pragma once
#include <QImage>
#include <QKeyEvent>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QUrl>

// Caja de texto: Enter envía, Shift+Enter hace salto de línea.
// Ctrl+V con una imagen en el portapapeles (captura, copiada del navegador, etc.)
// o con archivos copiados desde el explorador los agrega como adjuntos.
class ChatInput : public QPlainTextEdit {
    Q_OBJECT
public:
    using QPlainTextEdit::QPlainTextEdit;

signals:
    void sendRequested();
    void imagePasted(const QImage& img);
    void filesPasted(const QStringList& paths);

protected:
    bool canInsertFromMimeData(const QMimeData* s) const override {
        return s->hasImage() || s->hasUrls() || QPlainTextEdit::canInsertFromMimeData(s);
    }

    void insertFromMimeData(const QMimeData* s) override {
        if (s->hasUrls()) {
            QStringList files;
            for (const QUrl& u : s->urls())
                if (u.isLocalFile()) files << u.toLocalFile();
            if (!files.isEmpty()) {
                emit filesPasted(files);
                return;
            }
        }
        if (s->hasImage() && s->text().trimmed().isEmpty()) {
            const QImage img = qvariant_cast<QImage>(s->imageData());
            if (!img.isNull()) {
                emit imagePasted(img);
                return;
            }
        }
        QPlainTextEdit::insertFromMimeData(s);
    }

    void keyPressEvent(QKeyEvent* e) override {
        if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && !(e->modifiers() & Qt::ShiftModifier)) {
            emit sendRequested();
            return;
        }
        QPlainTextEdit::keyPressEvent(e);
    }
};
