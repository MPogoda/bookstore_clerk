#pragma once

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class LoginDialog;
class FillRequestDialog;
class QSqlQueryModel;
class QButtonGroup;
class QItemSelectionModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = NULL);
    ~MainWindow();
    
private:
    Ui::MainWindow *ui;
    /**
     * @brief m_clerkID ID number of connected clerk
     */
    uint m_clerkID;
    /**
     * @brief m_login Login dialog form
     */
    LoginDialog *m_login;
    /**
     * @brief m_fillRequest Fill Request form
     */
    FillRequestDialog *m_fillRequest;
    /**
     * @brief m_inputModel Model that will hold data for input view
     */
    QSqlQueryModel *m_inputModel;
    /**
     * @brief m_inputSelectionModel Selection model for m_inputModel
     */
    QItemSelectionModel *m_inputSelectionModel;
    /**
     * @brief m_filterButtons group of radio buttons for filters
     */
    QButtonGroup *m_filterButtons;

    /**
     * @brief Setup database connection: login, host, etc
     */
    void setup_connection() const;

    /**
     * @brief disconnect_filters disconnects all signals from filter controls
     */
    void disconnect_filters() const;
    /**
     * @brief connect_filters connects all signals back to filter controls
     */
    void connect_filters() const;
private slots:
    /**
     * @brief processLogin (re)login into system
     * asks login information (clerkID & password) and tries to find such credentials in DB
     */
    void processLogin();
    /**
     * @brief redrawView Run again select query (possibly with new parameters) and show results in main view
     */
    void redrawView();

    /**
     * @brief Dummy slots that will maintain filter controls in usable state
     */
    void boughtLessTrigger(const int val);
    void boughtLessBoxTrigger(const bool isOn );
    void boughtMoreTrigger(const int val);
    void boughtMoreBoxTrigger(const bool val);
    void instockLessTrigger(const int val);
    void instockLessBoxTrigger(const bool val);
    void instockMoreTrigger(const int val);
    void instockMoreBoxTrigger(const bool val);

    /**
     * @brief filterChanged do some action when filter has changed
     * @param buttonId number of filter selected
     */
    void filterChanged(const int buttonId);

    /**
     * @brief fillRequest Show information about selected entry and asks about how many books to request
     */
    void fillRequest();

    /**
     * @brief disconnect_clerk Disconnect current clerk (clear all tables, etc, etc)
     */
    void disconnect_clerk();

    /**
     * @brief connect_clerk enables all widgets after succesful login of clerk
     */
    void connect_clerk();
signals:
    void connected();
};
