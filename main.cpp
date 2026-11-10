#include "common.h"
#include "mainwindow.h"

#include <QApplication>
#include <QFileInfo>
#include <QMessageBox>
#include <QSqlError>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    const QString path = configPath();
    if (!QFileInfo(path).isFile()) {
        QMessageBox::critical(nullptr, "Настройки", "Файл настроек не найден:\n" + path);
        return 1;
    }

    const auto db = connectDatabase(path);
    if (!db.isOpen()) {
        QMessageBox::critical(nullptr, "Ошибка подключения",
                              "Проверь настройки PostgreSQL в config.ini.\n\n" + db.lastError().text());
        return 1;
    }

    MainWindow window(db);
    window.show();
    return app.exec();
}
