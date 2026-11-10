#include "common.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>

QString configPath()
{
    const QStringList args = QCoreApplication::arguments();
    if (args.size() == 3 && args.at(1) == "--config")
        return QFileInfo(args.at(2)).absoluteFilePath();

    const QString local = QDir::current().filePath("config.ini");
    if (QFileInfo(local).isFile())
        return local;

    return QDir(QString::fromUtf8(SOURCE_DIR)).filePath("config.ini");
}

QSqlDatabase connectDatabase(const QString &path)
{
    const QSettings settings(path, QSettings::IniFormat);
    auto db = QSqlDatabase::addDatabase("QPSQL");
    db.setHostName(settings.value("database/host", "localhost").toString());
    db.setPort(settings.value("database/port", 5432).toInt());
    db.setDatabaseName(settings.value("database/name", "document_search").toString());
    db.setUserName(settings.value("database/user", "postgres").toString());
    db.setPassword(settings.value("database/password").toString());
    db.setConnectOptions("connect_timeout=5");
    db.open();
    return db;
}

QStringList wordsFromText(const QString &text)
{
    static const QRegularExpression wordPattern(QStringLiteral("[\\p{L}\\p{N}]+"));
    QStringList words;
    auto matches = wordPattern.globalMatch(text.toLower());
    while (matches.hasNext()) {
        const QString word = matches.next().captured();
        if (word.size() >= 3 && word.size() <= 32)
            words.append(word);
    }
    return words;
}
