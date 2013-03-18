#pragma once

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class LoginDialog;
class FillRequestDialog;
class QSqlQueryModel;
class QSqlQuery;
class QButtonGroup;
class QItemSelectionModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    
private:
    Ui::MainWindow *ui;
    uint           m_clerkID;
    LoginDialog    *m_login;
    FillRequestDialog *m_fillRequest;
    QSqlQueryModel *m_inputModel;
    QItemSelectionModel *m_inputSelectionModel;
    QSqlQuery      *m_simpleSearch;
    QSqlQuery      *m_searchBook;
    QSqlQuery      *m_searchAuthors;
    QSqlQuery      *m_searchSold;
    QSqlQuery      *m_searchPasswordHash;
    QSqlQuery      *m_insertRequest;
    QButtonGroup   *m_filterButtons;

    /**
     * @brief Setup database connection: login, host, etc
     */
    void setup_connection();

    void prepare_queries();
    void disconnect_filters();
    void connect_filters();
private slots:
    void processLogin();
    void redrawView();
    void boughtLessTrigger(const int val);
    void boughtLessBoxTrigger(const bool isOn );
    void boughtMoreTrigger(const int val);
    void boughtMoreBoxTrigger(const bool val);
    void instockLessTrigger(const int val);
    void instockLessBoxTrigger(const bool val);
    void instockMoreTrigger(const int val);
    void instockMoreBoxTrigger(const bool val);

    void filterChanged(const int buttonId);

    void fillRequest();
signals:
    void connected();
};

