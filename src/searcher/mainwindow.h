#pragma once

#include <QFutureWatcher>
#include <QMainWindow>
#include <QStringList>
#include <QVariant>
#include <memory>

namespace Ui { class MainWindow; }

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &configPath, QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    struct QueryResult {
        QString error;
        QStringList labels;
        QStringList scores;
        bool search = false;
    };

    void search();
    void showWordFrequencies();
    void showRelevance();
    void runQuery(const QString &sql, const QVariantList &parameters, bool search);
    void showQueryResult();

    std::unique_ptr<Ui::MainWindow> ui;
    QString settingsPath;
    QFutureWatcher<QueryResult> queryWatcher;
    QStringList lastResults;
};
