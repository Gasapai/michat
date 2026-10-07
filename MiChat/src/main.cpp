#include <QApplication>
#include <QMessageBox>
#include <QPalette>

#include "Database.h"
#include "MainWindow.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setOrganizationName("MiChat");
    app.setApplicationName("MiChat");

    // Tema claro fijo (independiente del modo oscuro de Windows) para que los colores del chat sean consistentes
    app.setStyle("Fusion");
    QPalette p;
    p.setColor(QPalette::Window, QColor("#f0f2f5"));
    p.setColor(QPalette::WindowText, QColor("#111b21"));
    p.setColor(QPalette::Base, Qt::white);
    p.setColor(QPalette::AlternateBase, QColor("#f6f6f6"));
    p.setColor(QPalette::Text, QColor("#111b21"));
    p.setColor(QPalette::Button, QColor("#f0f2f5"));
    p.setColor(QPalette::ButtonText, QColor("#111b21"));
    p.setColor(QPalette::PlaceholderText, QColor("#8696a0"));
    p.setColor(QPalette::Highlight, QColor("#00a884"));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::ToolTipBase, QColor("#ffffe1"));
    p.setColor(QPalette::ToolTipText, Qt::black);
    app.setPalette(p);
    QFont f = app.font();
    f.setPointSize(10);
    app.setFont(f);
    app.setStyleSheet(R"(
        QWidget#chat { background:#efeae2; }
        QWidget#bar, QWidget#pending { background:#f0f2f5; }
        QFrame#bubble { background:#d9fdd3; border-radius:10px; }
        QFrame#bubble QLabel { color:#111b21; background:transparent; }
        QFrame#card { background:#ffffff; border:1px solid #c9d6c5; border-radius:8px; }
        QFrame#card QLabel { color:#111b21; background:transparent; }
        QFrame#file { background:#c4eebc; border-radius:8px; }
        QFrame#chip { background:#ffffff; border:1px solid #d1d7db; border-radius:8px; }
        QLabel#day { background:#e1f2fb; color:#54656f; border-radius:8px; padding:3px 12px; }
        QLabel#time { color:#667781; font-size:11px; }
        QLabel#count { color:#667781; }
        QLineEdit, QPlainTextEdit, QComboBox, QDateEdit { background:white; border:1px solid #d1d7db; border-radius:6px; padding:5px; }
        QPushButton { background:#00a884; color:white; border:none; border-radius:6px; padding:6px 14px; }
        QPushButton:hover { background:#019174; }
        QToolButton { border:none; font-size:18px; padding:4px; }
    )");

    QString err;
    if (!Database::instance().open(&err)) {
        QMessageBox::critical(nullptr, "Mi Chat", "No se pudo abrir la base de datos:\n" + err);
        return 1;
    }

    MainWindow w;
    w.show();
    return app.exec();
}
