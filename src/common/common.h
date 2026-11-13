#pragma once

#include <QSqlDatabase>
#include <QStringList>

QString configPath();
QSqlDatabase connectDatabase(const QString &path, const QString &connectionName = "indexer");
QStringList wordsFromText(const QString &text);

class WordTokenizer
{
public:
    QStringList takeWords(const QString &text, bool endOfInput = false);

private:
    QString pendingWord;
    bool wordTooLong = false;
};
