#include "common.h"
#include "mainwindow.h"

#include <QApplication>
#include <QFileInfo>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    const QString path = configPath();
    if (!QFileInfo(path).isFile()) {
        QMessageBox::critical(nullptr, "Настройки", "Файл настроек не найден:\n" + path);
        return 1;
    }

    MainWindow window(path);
    window.show();
    return app.exec();
}
