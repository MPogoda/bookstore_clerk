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
class QStringList;
class QModelIndex;
class QAction;
class QStringListModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget * const parent = NULL);
    ~MainWindow();
    
private:
    Ui::MainWindow *ui;
    /**
     * @brief m_clerkID ID number of connected clerk
     */
    uint m_clerkID;
    /**
     * @brief fillRequestAction Action for filling new request for book
     */
    QAction *m_fillRequestAction;
    /**
     * @brief m_modifyRequestAction Action for modifying request for book
     */
    QAction *m_modifyRequestAction;
    /**
     * @brief m_addToBundleAction Action that adds book to bundle under construction
     */
    QAction *m_addToBundleAction;
    QAction *m_removeBookFromBundle;
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
     * @brief m_bundledISBNs ISBN numbers of books that are currently in bundle under construction (modification)
     */
    QStringList m_bundledISBNs;
    /**
     * @brief m_bundledDiscounts discounts for books that are currently in bundle under construction (modification)
     */
    QList< qreal > m_bundledDiscounts;
    QList< qreal > m_bundledPrices;

    QStringListModel *m_bundleBookModel;
    QItemSelectionModel *m_bundleBookSelectionModel;
    /**
     * @brief m_isBundleUnderConstruction is there any bundle under construction right now
     */
    bool m_isBundleUnderConstruction;

    /**
     * @brief Setup database connection: login, host, etc
     */
    void setupConnection() const;

    /**
     * @brief disconnects all signals from filter controls
     */
    void disconnectFilters() const;
    /**
     * @brief connects all signals back to filter controls
     */
    void connectFilters() const;
    /**
     * @brief configureActions configures actions
     */
    void configureActions();
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
     * @brief fillRequest about how many books to request for selected ISBN
     */
    void fillRequest();
    /**
     * @brief modifyRequest allows to change previously filled request
     */
    void modifyRequest();

    /**
     * @brief disconnect_clerk Disconnect current clerk (clear all tables, etc, etc)
     */
    void disconnectClerk();

    /**
     * @brief connect_clerk enables all widgets after succesful login of clerk
     */
    void connectClerk();

    /**
     * @brief show_about Shows simple about messageBox
     */
    void showAbout();

    /**
     * @brief show_about_qt Show simple aboutqt messageBox
     */
    void showAboutQt();

    /**
     * @brief add_to_bundle add selected to bundle that is currently under construction (modification)
     */
    void addToBundle();
    void removeFromBundle();

    /**
     * @brief selectionChanged is executed every time selection changed in input view
     * @param current current index in input view
     * @param previous previous index in input view
     */
    void inputViewSelectionChanged( const QModelIndex& current, const QModelIndex& previous );

    void bundledBookViewSelectionChanged( const QModelIndex& current, const QModelIndex& previous );

    void currentTabChanged( const int index);

    void discountReset();
    void discountSave();
signals:
    void connected();
};
