#pragma once

#include <QSqlDatabase>
#include <QStringList>

QString configPath();
QSqlDatabase connectDatabase(const QString &path);
QStringList wordsFromText(const QString &text);
