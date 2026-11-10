#pragma once

#include <QMainWindow>
#include <QSqlDatabase>
#include <QStringList>
#include <memory>

namespace Ui { class MainWindow; }

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QSqlDatabase &database, QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void search();
    void showWordFrequencies();
    void showRelevance();

    std::unique_ptr<Ui::MainWindow> ui;
    QSqlDatabase db;
    QStringList lastResults;
};
