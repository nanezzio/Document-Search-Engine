#include "mainwindow.h"
#include "common.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QSqlError>
#include <QSqlQuery>
#include <QtConcurrentRun>

MainWindow::MainWindow(const QString &configPath, QWidget *parent)
    : QMainWindow(parent), ui(std::make_unique<Ui::MainWindow>()), settingsPath(configPath)
{
    ui->setupUi(this);
    connect(ui->searchButton, &QPushButton::clicked, this, &MainWindow::search);
    connect(ui->queryEdit, &QLineEdit::returnPressed, this, &MainWindow::search);
    connect(ui->wordsButton, &QPushButton::clicked, this, &MainWindow::showWordFrequencies);
    connect(ui->relevanceButton, &QPushButton::clicked, this, &MainWindow::showRelevance);
    connect(&queryWatcher, &QFutureWatcher<QueryResult>::finished, this, &MainWindow::showQueryResult);
}

MainWindow::~MainWindow()
{
    queryWatcher.waitForFinished();
}

void MainWindow::search()
{
    if (!ui->searchButton->isEnabled())
        return;

    QStringList words = wordsFromText(ui->queryEdit->text());
    if (words.isEmpty() || words.size() > 4) {
        QMessageBox::information(this, "Поиск", "Введи от одного до четырёх слов длиной от 3 до 32 символов.");
        return;
    }
    words.removeDuplicates();

    QStringList placeholders;
    QVariantList parameters;
    for (const QString &word : words) {
        placeholders.append("?");
        parameters.append(word);
    }
    parameters.append(static_cast<int>(words.size()));
    runQuery("SELECT d.path, SUM(dw.frequency) AS score "
             "FROM documents d JOIN document_words dw ON dw.document_id = d.id "
             "JOIN words w ON w.id = dw.word_id "
             "WHERE w.word IN (" + placeholders.join(',') + ") "
             "GROUP BY d.id, d.path HAVING COUNT(*) = ? "
             "ORDER BY score DESC, d.path", parameters, true);
}

void MainWindow::showWordFrequencies()
{
    runQuery("SELECT w.word, COALESCE(SUM(dw.frequency), 0) FROM words w "
             "LEFT JOIN document_words dw ON dw.word_id = w.id GROUP BY w.word ORDER BY w.word", {}, false);
}

void MainWindow::runQuery(const QString &sql, const QVariantList &parameters, bool search)
{
    ui->searchButton->setEnabled(false);
    ui->wordsButton->setEnabled(false);
    ui->statusbar->showMessage("Запрос к базе данных…");

    queryWatcher.setFuture(QtConcurrent::run([path = settingsPath, sql, parameters, search] {
        QueryResult result;
        result.search = search;
        const QString connectionName = "searcher";
        {
            const auto db = connectDatabase(path, connectionName);
            if (!db.isOpen()) {
                result.error = "Проверь настройки PostgreSQL в config.ini.\n\n" + db.lastError().text();
            } else {
                QSqlQuery query(db);
                if (query.prepare(sql)) {
                    for (const QVariant &parameter : parameters)
                        query.addBindValue(parameter);
                    if (query.exec()) {
                        while (query.next()) {
                            const QString label = query.value(0).toString();
                            result.labels.append(label);
                            result.scores.append(QString("%1 — %2").arg(label, query.value(1).toString()));
                        }
                    }
                }
                if (query.lastError().isValid())
                    result.error = query.lastError().text();
            }
        }
        QSqlDatabase::removeDatabase(connectionName);
        return result;
    }));
}

void MainWindow::showQueryResult()
{
    const QueryResult result = queryWatcher.result();
    ui->searchButton->setEnabled(true);
    ui->wordsButton->setEnabled(true);
    ui->statusbar->clearMessage();
    if (!result.error.isEmpty()) {
        QMessageBox::critical(this, result.search ? "Ошибка поиска" : "Ошибка статистики", result.error);
        return;
    }

    if (result.search) {
        lastResults = result.scores;
        ui->resultsEdit->setPlainText(result.labels.isEmpty() ? "Документы не найдены." : result.labels.join('\n'));
        ui->relevanceButton->setEnabled(true);
    } else {
        ui->statisticsEdit->setPlainText(result.scores.isEmpty() ? "В индексе пока нет слов." : result.scores.join('\n'));
    }
}

void MainWindow::showRelevance()
{
    ui->statisticsEdit->setPlainText(lastResults.isEmpty() ? "Документы не найдены." : lastResults.join('\n'));
}
