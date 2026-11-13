#include "common.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QSettings>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringConverter>
#include <QTextStream>

static bool createTables(const QSqlDatabase &db, QTextStream &err)
{
    const QStringList statements = {
        "CREATE TABLE IF NOT EXISTS documents (id BIGSERIAL PRIMARY KEY, path TEXT UNIQUE NOT NULL)",
        "CREATE TABLE IF NOT EXISTS words (id BIGSERIAL PRIMARY KEY, word TEXT UNIQUE NOT NULL)",
        "CREATE TABLE IF NOT EXISTS document_words ("
        "document_id BIGINT REFERENCES documents(id) ON DELETE CASCADE, "
        "word_id BIGINT REFERENCES words(id), frequency INTEGER NOT NULL CHECK (frequency > 0), "
        "PRIMARY KEY (document_id, word_id))",
        "CREATE INDEX IF NOT EXISTS document_words_word_idx ON document_words (word_id)"
    };
    QSqlQuery query(db);
    for (const QString &sql : statements) {
        if (!query.exec(sql)) {
            err << query.lastError().text() << Qt::endl;
            return false;
        }
    }
    return true;
}

static bool saveDocument(const QSqlDatabase &db, const QString &path,
                         const QHash<QString, int> &frequencies, QString &error)
{
    QSqlQuery document(db);
    document.prepare("INSERT INTO documents (path) VALUES (?) "
                     "ON CONFLICT (path) DO UPDATE SET path = EXCLUDED.path RETURNING id");
    document.addBindValue(path);
    if (!document.exec() || !document.next()) {
        error = document.lastError().text();
        return false;
    }
    const auto documentId = document.value(0);
    document.finish();

    QSqlQuery remove(db);
    remove.prepare("DELETE FROM document_words WHERE document_id = ?");
    remove.addBindValue(documentId);
    if (!remove.exec()) {
        error = remove.lastError().text();
        return false;
    }

    QSqlQuery word(db), link(db);
    word.prepare("INSERT INTO words (word) VALUES (?) "
                 "ON CONFLICT (word) DO UPDATE SET word = EXCLUDED.word RETURNING id");
    link.prepare("INSERT INTO document_words (document_id, word_id, frequency) VALUES (?, ?, ?)");
    for (auto it = frequencies.cbegin(); it != frequencies.cend(); ++it) {
        word.bindValue(0, it.key());
        if (!word.exec() || !word.next()) {
            error = word.lastError().text();
            return false;
        }
        link.bindValue(0, documentId);
        link.bindValue(1, word.value(0));
        link.bindValue(2, it.value());
        word.finish();
        if (!link.exec()) {
            error = link.lastError().text();
            return false;
        }
    }
    return true;
}

static bool indexFile(QSqlDatabase &db, const QString &path, QString &error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = file.errorString();
        return false;
    }
    QTextStream input(&file);
    input.setEncoding(QStringConverter::Utf8);
    input.setAutoDetectUnicode(false);
    constexpr qsizetype blockSize = 64 * 1024;
    WordTokenizer tokenizer;
    QHash<QString, int> frequencies;
    while (!input.atEnd()) {
        QString block = input.read(blockSize);
        if (block.isEmpty())
            break;
        if (block.back().isHighSurrogate())
            block += input.read(1);
        for (const QString &word : tokenizer.takeWords(block))
            ++frequencies[word];
    }
    if (file.error() != QFileDevice::NoError) {
        error = file.errorString();
        return false;
    }
    for (const QString &word : tokenizer.takeWords({}, true))
        ++frequencies[word];

    if (!db.transaction()) {
        error = db.lastError().text();
        return false;
    }
    if (!saveDocument(db, path, frequencies, error)) {
        db.rollback();
        return false;
    }
    if (!db.commit()) {
        error = db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}

static bool isInside(const QString &path, QString directory)
{
    if (!directory.endsWith('/'))
        directory += '/';
#ifdef Q_OS_WIN
    return path.startsWith(directory, Qt::CaseInsensitive);
#else
    return path.startsWith(directory);
#endif
}

static bool removeStaleDocuments(QSqlDatabase &db, const QStringList &roots,
                                 const QSet<QString> &seen, qsizetype &removed, QString &error)
{
    if (!db.transaction()) {
        error = db.lastError().text();
        return false;
    }

    QSqlQuery query(db);
    auto fail = [&](const QString &message) {
        error = message;
        query.finish();
        db.rollback();
        return false;
    };
    if (!query.exec("SELECT path FROM documents"))
        return fail(query.lastError().text());

    QStringList stale;
    while (query.next()) {
        const QString path = query.value(0).toString();
        if (seen.contains(path))
            continue;
        for (const QString &root : roots) {
            if (isInside(path, root)) {
                stale.append(path);
                break;
            }
        }
    }
    if (query.lastError().isValid())
        return fail(query.lastError().text());
    query.finish();

    if (!query.prepare("DELETE FROM documents WHERE path = ?"))
        return fail(query.lastError().text());
    for (const QString &path : stale) {
        query.bindValue(0, path);
        if (!query.exec())
            return fail(query.lastError().text());
    }
    query.finish();
    if (!db.commit())
        return fail(db.lastError().text());
    removed = stale.size();
    return true;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout), err(stderr);
#ifdef Q_OS_WIN
    // Qt Creator's Application Output reads the Windows system encoding.
    out.setEncoding(QStringConverter::System);
    err.setEncoding(QStringConverter::System);
#endif
    const QString path = configPath();
    if (!QFileInfo(path).isFile()) {
        err << "Файл настроек не найден: " << path << Qt::endl;
        return 1;
    }
    const QSettings settings(path, QSettings::IniFormat);
    const QStringList directories = settings.value("indexer/directories").toString().split('|', Qt::SkipEmptyParts);
    QSet<QString> extensions;
    const auto configuredExtensions = settings.value("indexer/extensions").toStringList();
    for (const QString &part : configuredExtensions) {
        for (const QString &item : part.split(',', Qt::SkipEmptyParts)) {
            QString extension = item.trimmed().toLower();
            if (extension.startsWith('.'))
                extension.remove(0, 1);
            if (!extension.isEmpty())
                extensions.insert(extension);
        }
    }
    if (directories.isEmpty() || extensions.isEmpty()) {
        err << "Укажи каталоги и расширения в config.ini." << Qt::endl;
        return 1;
    }

    auto db = connectDatabase(path);
    if (!db.isOpen()) {
        err << "Не удалось подключиться к PostgreSQL. Проверь имя базы, пользователя и пароль в config.ini."
            << Qt::endl << db.lastError().text() << Qt::endl;
        return 1;
    }
    // Prevent overlapping scans of the same database from pruning each other's results.
    QSqlQuery lock(db);
    if (!lock.exec("SELECT pg_try_advisory_lock(1842193061)") || !lock.next()) {
        err << lock.lastError().text() << Qt::endl;
        return 1;
    }
    if (!lock.value(0).toBool()) {
        err << "Индексатор уже работает с этой базой." << Qt::endl;
        return 1;
    }
    lock.finish();
    if (!createTables(db, err))
        return 1;

    QSet<QString> seen;
    QStringList scannedRoots;
    bool scanComplete = true;
    int indexed = 0;
    int errors = 0;
    const QDir configDir = QFileInfo(path).absoluteDir();
    for (const QString &directory : directories) {
        const QFileInfo root(configDir.absoluteFilePath(directory.trimmed()));
        const QString rootPath = root.canonicalFilePath();
        if (!root.isDir() || !root.isReadable() || rootPath.isEmpty()) {
            err << "Каталог недоступен: " << root.absoluteFilePath() << Qt::endl;
            ++errors;
            scanComplete = false;
            continue;
        }
        scannedRoots.append(rootPath);
        QDirIterator files(rootPath, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks,
                           QDirIterator::Subdirectories);
        while (files.hasNext()) {
            const QFileInfo info(files.next());
            if (info.isDir()) {
                if (!info.isReadable()) {
                    err << "Подкаталог недоступен: " << info.absoluteFilePath() << Qt::endl;
                    ++errors;
                    scanComplete = false;
                }
                continue;
            }
            const QString filePath = info.canonicalFilePath();
            if (!extensions.contains(info.suffix().toLower()) || filePath.isEmpty() || seen.contains(filePath))
                continue;
            seen.insert(filePath);
            QString error;
            if (indexFile(db, filePath, error)) {
                ++indexed;
                out << "Готово: " << filePath << Qt::endl;
            } else {
                ++errors;
                err << filePath << ": " << error << Qt::endl;
            }
        }
    }
    qsizetype removed = 0;
    QString cleanupError;
    if (!scanComplete) {
        err << "Очистка индекса пропущена: обход завершён не полностью." << Qt::endl;
    } else if (!removeStaleDocuments(db, scannedRoots, seen, removed, cleanupError)) {
        err << "Не удалось очистить индекс: " << cleanupError << Qt::endl;
        ++errors;
    }
    out << "Удалено из индекса: " << removed << Qt::endl;
    out << "Проиндексировано: " << indexed << ", ошибок: " << errors << Qt::endl;
    return errors == 0 ? 0 : 1;
}
