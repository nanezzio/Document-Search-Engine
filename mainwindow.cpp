#include "mainwindow.h"
#include "common.h"
#include "ui_mainwindow.h"

#include <QFileInfo>
#include <QMessageBox>
#include <QSqlError>
#include <QSqlQuery>

MainWindow::MainWindow(const QSqlDatabase &database, QWidget *parent)
    : QMainWindow(parent), ui(std::make_unique<Ui::MainWindow>()), db(database)
{
    ui->setupUi(this);
    connect(ui->searchButton, &QPushButton::clicked, this, &MainWindow::search);
    connect(ui->queryEdit, &QLineEdit::returnPressed, this, &MainWindow::search);
    connect(ui->wordsButton, &QPushButton::clicked, this, &MainWindow::showWordFrequencies);
    connect(ui->relevanceButton, &QPushButton::clicked, this, &MainWindow::showRelevance);
}

MainWindow::~MainWindow() = default;

void MainWindow::search()
{
    QStringList words = wordsFromText(ui->queryEdit->text());
    if (words.isEmpty() || words.size() > 4) {
        QMessageBox::information(this, "Поиск", "Введи от одного до четырёх слов длиной от 3 до 32 символов.");
        return;
    }
    words.removeDuplicates();

    QStringList placeholders;
    for (qsizetype i = 0; i < words.size(); ++i)
        placeholders.append("?");

    QSqlQuery query(db);
    query.prepare("SELECT d.path, SUM(dw.frequency) AS score "
                  "FROM documents d JOIN document_words dw ON dw.document_id = d.id "
                  "JOIN words w ON w.id = dw.word_id "
                  "WHERE w.word IN (" + placeholders.join(',') + ") "
                  "GROUP BY d.id, d.path HAVING COUNT(*) = ? "
                  "ORDER BY score DESC, d.path");
    for (const QString &word : words)
        query.addBindValue(word);
    query.addBindValue(static_cast<int>(words.size()));
    if (!query.exec()) {
        QMessageBox::critical(this, "Ошибка поиска",
                              "Сначала запусти индексатор для этой базы.\n\n" + query.lastError().text());
        return;
    }

    QStringList paths;
    lastResults.clear();
    while (query.next()) {
        const QString path = query.value(0).toString();
        paths.append(path);
        lastResults.append(QString("%1 — %2").arg(path, query.value(1).toString()));
    }
    ui->resultsEdit->setPlainText(paths.isEmpty() ? "Документы не найдены." : paths.join('\n'));
    ui->relevanceButton->setEnabled(true);
}

void MainWindow::showWordFrequencies()
{
    QSqlQuery query(db);
    if (!query.exec("SELECT w.word, COALESCE(SUM(dw.frequency), 0) FROM words w "
                    "LEFT JOIN document_words dw ON dw.word_id = w.id GROUP BY w.word ORDER BY w.word")) {
        QMessageBox::critical(this, "Ошибка статистики", query.lastError().text());
        return;
    }

    QStringList lines;
    while (query.next())
        lines.append(QString("%1 — %2").arg(query.value(0).toString(), query.value(1).toString()));
    ui->statisticsEdit->setPlainText(lines.isEmpty() ? "В индексе пока нет слов." : lines.join('\n'));
}

void MainWindow::showRelevance()
{
    ui->statisticsEdit->setPlainText(lastResults.isEmpty() ? "Документы не найдены." : lastResults.join('\n'));
}
