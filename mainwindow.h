#pragma once

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class LoginDialog;
class QSqlQueryModel;
class QSqlQuery;
class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    
private:
    Ui::MainWindow *ui;
    bool           m_isOnline;
    LoginDialog    *m_login;
    QSqlQueryModel *m_model;
    QSqlQuery      *m_simpleSearch;

    bool connect_to_database();
private slots:
    void processLogin();
    void redrawView();
    void boughtLessTrigger(int val);
    void boughtMoreTrigger(int val);
    void instockLessTrigger(int val);
    void instockMoreTrigger(int val);
signals:
    void connected();
};

