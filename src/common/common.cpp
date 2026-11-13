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

QSqlDatabase connectDatabase(const QString &path, const QString &connectionName)
{
    const QSettings settings(path, QSettings::IniFormat);
    auto db = QSqlDatabase::addDatabase("QPSQL", connectionName);
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
    return WordTokenizer().takeWords(text, true);
}

QStringList WordTokenizer::takeWords(const QString &text, bool endOfInput)
{
    static const QRegularExpression wordPattern(QStringLiteral("[\\p{L}\\p{N}]+"));
    const QString lower = text.toLower();
    QStringList words;
    auto finishWord = [&] {
        if (pendingWord.size() >= 3)
            words.append(pendingWord);
        pendingWord.clear();
        wordTooLong = false;
    };

    qsizetype lastEnd = 0;
    auto matches = wordPattern.globalMatch(lower);
    while (matches.hasNext()) {
        const auto match = matches.next();
        if (match.capturedStart() > lastEnd)
            finishWord();
        if (!wordTooLong) {
            if (pendingWord.size() + match.capturedLength() <= 32) {
                pendingWord.append(match.capturedView());
            } else {
                pendingWord.clear();
                wordTooLong = true;
            }
        }
        lastEnd = match.capturedEnd();
    }
    if (lastEnd < lower.size() || endOfInput)
        finishWord();
    return words;
}
