#pragma once
#include <QDate>
#include <QHash>
#include <QMainWindow>
#include <QPointer>
#include <QSet>

#include "Database.h"

class QCheckBox;
class QComboBox;
class QDateEdit;
class QFrame;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QScrollArea;
class QTimer;
class QVBoxLayout;
class ChatInput;
class LinkFetcher;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private:
    Filter currentFilter() const;
    void reload();
    void clearFilters();
    void clearList();
    void updateCount();
    void appendMessage(qint64 id);
    void keepBottom();

    QWidget* makeBubble(const Message& m);
    QWidget* makeImage(const Attachment& a);
    QWidget* makeFile(const Attachment& a);
    QWidget* makeCard(const LinkInfo& li);
    QString toHtml(const QString& plain) const;

    void send();
    void addPendingImage(const QImage& img);
    void addPendingFiles(const QStringList& paths);
    void refreshPending();
    void onLinkUpdated(qint64 id);

    void copyImage(qint64 id);
    void saveAttachment(qint64 id, const QString& name);
    void openAttachment(qint64 id, const QString& name);
    void showImage(qint64 id, const QString& name);

    QLineEdit* search_ = nullptr;
    QComboBox* type_ = nullptr;
    QCheckBox* dateOn_ = nullptr;
    QDateEdit* from_ = nullptr;
    QDateEdit* to_ = nullptr;
    QLabel* count_ = nullptr;
    QScrollArea* scroll_ = nullptr;
    QWidget* list_ = nullptr;
    QVBoxLayout* listLay_ = nullptr;
    QScrollArea* pendingScroll_ = nullptr;
    QWidget* pendingBar_ = nullptr;
    QHBoxLayout* pendingLay_ = nullptr;
    ChatInput* input_ = nullptr;
    QTimer* debounce_ = nullptr;
    LinkFetcher* fetcher_ = nullptr;

    QList<Attachment> pending_;
    QHash<qint64, QPointer<QFrame>> cards_;
    QSet<qint64> tried_;
    QStringList terms_;
    QDate lastDay_;
    QPointer<QLabel> emptyLabel_;
    int shown_ = 0;
    bool stick_ = false;
};
