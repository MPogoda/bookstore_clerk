#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QDebug>
#include <QMessageBox>
#include <stdexcept>
#include <QTimer>
#include <QSqlError>
#include <QSettings>
#include <QItemSelectionModel>
#include <QSqlRecord>
#include <QSqlResult>
#include <QStringListModel>
#include <numeric>
#include <algorithm>

#include "logindialog.h"
#include "fillrequestdialog.h"

namespace
{
/**
 * @brief The DebugHelper struct is RAII-helper that helps to print Q_FUNC_INFO whenever you enter
 * function (if you've declared object of that type at beginning) and when you leave it.
 */
struct DebugHelper
{
    const QString m_funcInfo;
    DebugHelper( const QString funcInfo )
        : m_funcInfo(funcInfo)
    {
        qDebug() << "=====ENTERING " << m_funcInfo << "===========";
    }
    ~DebugHelper()
    {
        qDebug() << "=====LEAVING  " << m_funcInfo << "===========";
    }
};

/**
 * @brief The DBOpener struct RAII helper that tries to (re)open connection database and closes it afterwards.
 */
struct DBOpener
{
    MainWindow * const mw;
    DBOpener( MainWindow * const iMW)
        : mw( iMW )
    {
        bool isOpened = QSqlDatabase::database().open();
        qDebug() << "DBOpen: " << isOpened;
        if (!isOpened)
            QMessageBox::critical(iMW, "Database connection error", "Cannot establish connection to database");
    }
    ~DBOpener()
    {
        qDebug() << "~DBOpen: ";
        QSqlDatabase::database().close();
    }
};
}

MainWindow::MainWindow(QWidget * const parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_clerkID( 0 )
    , m_fillRequestAction( new QAction( tr("Add Request"), this) )
    , m_modifyRequestAction( new QAction( tr("Modify Request"), this) )
    , m_removeRequestAction( new QAction( tr("Remove Request"), this) )
    , m_addToBundleAction( new QAction( tr("Add to Bundle"), this ) )
    , m_removeBookFromBundle( new QAction( tr("Remove from Bundle"), this))
    , m_saveBundleAction( new QAction( tr("Save Bundle"), this))
    , m_login(new LoginDialog(this))
    , m_fillRequest( new FillRequestDialog( this ))
    , m_inputModel( new QSqlQueryModel( this ) )
    , m_inputSelectionModel( new QItemSelectionModel( m_inputModel, this ) )
    , m_filterButtons( new QButtonGroup( this ) )
    , m_bundleBookModel( new QStringListModel( this ))
    , m_bundleBookSelectionModel( new QItemSelectionModel( m_bundleBookModel, this ))
    , m_isBundleUnderConstruction( false )
{
    DebugHelper debugHelper( Q_FUNC_INFO);

    ui->setupUi(this);
    ui->filterGroupBox->hide();
    ui->discountBox->hide();

    configureActions();


    m_filterButtons->addButton( ui->trendingRadioButton, 0);
    m_filterButtons->addButton( ui->overstockedRadioButton, 1 );
    m_filterButtons->addButton( ui->customRadioButton, 2 );

    /* setup connection to database */
    setupConnection();

    connect(this, SIGNAL(connected()), this, SLOT(redrawView()));
    QTimer::singleShot(10, this, SLOT(processLogin()));

    ui->tableView->setModel(m_inputModel);
    ui->tableView->setSelectionModel( m_inputSelectionModel );

    ui->bundleBooksView->setModel( m_bundleBookModel );
    ui->bundleBooksView->setSelectionModel( m_bundleBookSelectionModel );

    connect( m_filterButtons, SIGNAL(buttonClicked(int)), this, SLOT(filterChanged(int)));
    connectFilters();

    connect( ui->filterButton, SIGNAL(clicked()), this, SLOT(redrawView()));
    connect( m_fillRequestAction, SIGNAL(triggered()), this, SLOT(fillRequest()));

    connect( ui->actionDisconnect, SIGNAL(triggered()), this, SLOT(disconnectClerk()) );
    connect( ui->actionReconnect, SIGNAL(triggered()), this, SLOT(processLogin()) );
    connect( this, SIGNAL(connected()), this, SLOT(connectClerk()) );

    connect( ui->actionAbou, SIGNAL(triggered()), this, SLOT(showAbout()) );
    connect( ui->actionAbout_Qt, SIGNAL(triggered()), this, SLOT(showAboutQt()) );

    connect( m_inputSelectionModel, SIGNAL(currentChanged(QModelIndex,QModelIndex)),
             this, SLOT(inputViewSelectionChanged(QModelIndex,QModelIndex)) );

    connect( ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(currentTabChanged(int)) );

    connect( m_modifyRequestAction, SIGNAL(triggered()), this, SLOT(modifyRequest()));
    connect( m_removeRequestAction, SIGNAL(triggered()), this, SLOT(removeRequest()));

    connect( m_addToBundleAction, SIGNAL(triggered()), this, SLOT(addToBundle()));

    connect( m_bundleBookSelectionModel, SIGNAL(currentChanged(QModelIndex,QModelIndex))
             , this, SLOT(bundledBookViewSelectionChanged(QModelIndex,QModelIndex)) );

    connect( ui->saveDiscountButton, SIGNAL(clicked()), this, SLOT(discountSave()) );
    connect( ui->resetDiscountButton, SIGNAL(clicked()), this, SLOT(discountReset()) );
    connect( m_removeBookFromBundle, SIGNAL(triggered()), this, SLOT(removeFromBundle()));
    connect( m_saveBundleAction, SIGNAL(triggered()), this, SLOT(saveBundle()));
    connect( ui->discountSpin, SIGNAL(valueChanged(int)), this, SLOT(discountChanged(int)));
}

namespace
{
void setShortcut( QAction *const action, const QKeySequence& sequence, const QKeySequence fallback = QKeySequence::UnknownKey)
{
    DebugHelper debugHelper( Q_FUNC_INFO );

    if (sequence.isEmpty())
    {
        qDebug() << "Given sequence is empty. Using fallback";
        action->setShortcut( fallback );
    }
    else
        action->setShortcut( sequence );
}
}

void MainWindow::currentTabChanged(const int index)
{
    DebugHelper debugHelper( Q_FUNC_INFO );
    ui->currentBookBox->hide();

    switch (index)
    {
    case 0:
        // do stuff for input pane
        ui->discountBox->hide();
        qDebug() << m_inputSelectionModel->currentIndex().row();
        m_removeBookFromBundle->setVisible( false );
        inputViewSelectionChanged( m_inputSelectionModel->currentIndex(), m_inputModel->index( -1, -1 ));
        break;
    case 1:
        if (!m_isBundleUnderConstruction)
        {
            ui->tabBundleMod->setEnabled( false );
            // do stuff (ask about new bundle? )
        }
        ui->discountBox->show();
        m_addToBundleAction->setVisible( false );
        m_modifyRequestAction->setVisible( false );
        m_modifyRequestAction->setVisible( false );
        m_fillRequestAction->setVisible( false );
        // do stuff for bundle modification pane
        break;
    case 2:
        // do stuff for bundle selection pane
        break;
    default:
        throw std::logic_error( "There shouldn't be that tab!");
    }

}

void MainWindow::configureActions()
{
    DebugHelper debugHelper( Q_FUNC_INFO );

    setShortcut( ui->actionQuit, QKeySequence::Quit, tr("Ctrl+Q") );
    setShortcut( ui->actionDisconnect, QKeySequence::Close, tr("Ctrl+W") );
    setShortcut( ui->actionReconnect, QKeySequence::New, tr("Ctrl+N") );
    setShortcut( ui->actionAbou, QKeySequence::HelpContents, tr("F1"));
    setShortcut( ui->actionAbout_Qt, QKeySequence::WhatsThis, tr("Shift+F1"));
    setShortcut( m_fillRequestAction, QKeySequence::Bold, tr("Ctrl+B") );
    setShortcut( m_modifyRequestAction, QKeySequence::Bold, tr("Ctrl+B") );
    setShortcut( m_removeRequestAction, QKeySequence::Underline, tr("Ctrl+U") );
    setShortcut( m_addToBundleAction, QKeySequence::Italic, tr("Ctrl+I"));
    setShortcut( m_removeBookFromBundle, QKeySequence::Italic, tr("Ctrl+I"));
    setShortcut( m_saveBundleAction, QKeySequence::Save, tr("Ctrl+S"));

    m_fillRequestAction->setToolTip( tr("Fill request for selected book") );
    ui->mainToolBar->addAction( m_fillRequestAction);
    ui->menuAction->addAction( m_fillRequestAction );
    m_fillRequestAction->setVisible( false );

    m_modifyRequestAction->setToolTip( tr("Modify your previous request" ));
    ui->mainToolBar->addAction( m_modifyRequestAction );
    ui->menuAction->addAction( m_modifyRequestAction );
    m_modifyRequestAction->setVisible( false );

    m_removeRequestAction->setToolTip( tr("Remove your previous request" ));
    ui->mainToolBar->addAction( m_removeRequestAction );
    ui->menuAction->addAction( m_removeRequestAction );
    m_removeRequestAction->setVisible( false );

    m_addToBundleAction->setToolTip( tr("Add selected book to bundle"));
    ui->mainToolBar->addAction( m_addToBundleAction );
    ui->menuAction->addAction( m_addToBundleAction );
    m_addToBundleAction->setVisible( false );

    m_removeBookFromBundle->setToolTip( tr("Remove selected book from bundle"));
    ui->mainToolBar->addAction( m_removeBookFromBundle );
    ui->menuAction->addAction( m_removeBookFromBundle );
    m_removeBookFromBundle->setVisible( false );

    m_saveBundleAction->setToolTip( tr("Save bundle"));
    ui->mainToolBar->addAction( m_saveBundleAction );
    ui->menuAction->addAction( m_saveBundleAction );
    m_saveBundleAction->setVisible( false );
}

MainWindow::~MainWindow()
{
    DebugHelper debugHelper( Q_FUNC_INFO);
    delete ui;
}

void MainWindow::setupConnection() const
{
    DebugHelper debugHelper( Q_FUNC_INFO);
    QSettings settings( "settings.ini", QSettings::IniFormat );

    settings.beginGroup( "database" );
    const QString dbDriver( settings.value( "driver",   "QOCI"      ).toString() );
    const QString hostName( settings.value( "hostname", "localhost" ).toString() );
    const QString dbName( settings.value( "database", "bookstore" ).toString() );
    const QString userName( settings.value( "user",     QString()   ).toString() );
    const QString password( settings.value( "password", QString()   ).toString() );
    settings.endGroup();

    qDebug() << "driver: " << dbDriver;
    qDebug() << "hostname: " << hostName;
    qDebug() << "database: " << dbName;
    qDebug() << "username: " << userName;
    qDebug() << "password: " << password;

    QSqlDatabase db = QSqlDatabase::addDatabase( dbDriver );
    db.setHostName(     hostName );
    db.setDatabaseName( dbName );
    db.setUserName(     userName );
    db.setPassword(     password );
}

namespace
{
/**
 * @brief find_authors_for_book get all authors that have written book
 * @param isbn ISBN number of book
 * @return StringList of authors
 */
const QStringList findAuthorsForBook( const QString& isbn )
{
    DebugHelper debugHelper( Q_FUNC_INFO );
    QSqlQuery searchAuthors;
    searchAuthors.setForwardOnly( true );
    qDebug() << "Prepare: " <<
        searchAuthors.prepare( "SELECT name "
                               "FROM book JOIN book_s_author "
                                         "ON book_s_author.isbn = book.isbn "
                                         "JOIN author "
                                         "ON author.author_id = book_s_author.author_id "
                               "WHERE book_s_author.isbn = :isbn ");

    searchAuthors.bindValue( ":isbn", isbn );
    qDebug() << "Exec: " << searchAuthors.exec();

    QStringList authors;
    while (searchAuthors.next())
        authors << searchAuthors.value( 0 ).toString();

    qDebug() << "Authors: " << authors;

    return authors;
}
}

void MainWindow::modifyRequest()
{
    DebugHelper debugHelper( Q_FUNC_INFO);

    const int row = m_inputSelectionModel->currentIndex().row();
    if (-1 == row)
    {
        qDebug() << "No row is selected";
        return;
    }

    m_fillRequest->prepareForm( ui->requestedLabel->text().toUInt() );

    if (QDialog::Accepted != m_fillRequest->exec())
    {
        qDebug() << "Request has been cancelled!";
        return;
    }

    const uint request = m_fillRequest->quantity();

    if (request == ui->requestedLabel->text().toUInt())
    {
        qDebug() << "Request has not been changed";
        return;
    }

    const QString isbn = ui->isbnLabel->text();
    qDebug() << "ISBN: " << isbn;

    DBOpener    dbopener( this );

    QSqlQuery updateQuery;
    qDebug() << "Prepare: " <<
                updateQuery.prepare( "UPDATE request "
                                     "SET quantity = :quantity "
                                     "where isbn = :isbn");

    updateQuery.bindValue( ":isbn", isbn );
    updateQuery.bindValue( ":quantity", request );

    qDebug() << "Transaction: " <<
                QSqlDatabase::database().transaction();
    const bool queryResult = updateQuery.exec();
    qDebug() << "Exec: " << queryResult;
    if (!queryResult) {
        qDebug() << "Rollback" <<
                  QSqlDatabase::database().rollback();
        return;
    }
    const bool commit = QSqlDatabase::database().commit();
    qDebug() << "Commit: " << commit;

    if (commit)
        ui->requestedLabel->setText( QString::number( request ));
    else
        qDebug() << "Rollback" <<
                  QSqlDatabase::database().rollback();

}

void MainWindow::removeRequest()
{
    DebugHelper debugHelper( Q_FUNC_INFO);

    const int row = m_inputSelectionModel->currentIndex().row();
    if (-1 == row)
    {
        qDebug() << "No row is selected";
        return;
    }

    const QString isbn = ui->isbnLabel->text();
    qDebug() << "ISBN: " << isbn;

    DBOpener    dbopener( this );

    QSqlQuery removeQuery;
    qDebug() << "Prepare: " <<
                removeQuery.prepare( "DELETE "
                                     "FROM request "
                                     "where isbn = :isbn");

    removeQuery.bindValue( ":isbn", isbn );

    qDebug() << "Transaction: " <<
                QSqlDatabase::database().transaction();
    const bool queryResult = removeQuery.exec();
    qDebug() << "Exec: " << queryResult;
    if (!queryResult) {
        qDebug() << "Rollback" <<
                  QSqlDatabase::database().rollback();
        return;
    }
    const bool commit = QSqlDatabase::database().commit();
    qDebug() << "Commit: " << commit;

    if (commit) {
        ui->requestedLabel->setText( "None" );
        m_modifyRequestAction->setVisible( false );
        m_removeRequestAction->setVisible( false );
        m_fillRequestAction->setVisible( true );
    }
    else
        qDebug() << "Rollback" <<
                  QSqlDatabase::database().rollback();

}

void MainWindow::fillRequest()
{
    DebugHelper debugHelper( Q_FUNC_INFO);

    const int row = m_inputSelectionModel->currentIndex().row();
    if (-1 == row)
    {
        qDebug() << "No row is selected";
        return;
    }

    m_fillRequest->prepareForm( 1 );

    if (QDialog::Accepted != m_fillRequest->exec())
    {
        qDebug() << "Request has been cancelled!";
        return;
    }

    const uint request = m_fillRequest->quantity();

    const QString isbn = m_inputModel->record( row ).value( "isbn" ).toString();
    qDebug() << "ISBN: " << isbn;

    DBOpener    dbopener( this );

    QSqlQuery insertRequest;
    qDebug() << "Prepare: " <<
                insertRequest.prepare( "INSERT INTO request( isbn, quantity, clerk_id ) VALUES "
                                       "( :isbn, :quantity, :clerkID )");

    insertRequest.bindValue( ":isbn", isbn );
    insertRequest.bindValue( ":quantity", request );
    insertRequest.bindValue( ":clerkID", m_clerkID );

    qDebug() << "Transaction: " <<
                QSqlDatabase::database().transaction();
    const bool insertResult = insertRequest.exec();
    qDebug() << "Exec: " << insertResult;
    if (!insertResult) {
        qDebug() << "Rollback" <<
                  QSqlDatabase::database().rollback();
        return;
    }
    const bool commit = QSqlDatabase::database().commit();
    qDebug() << "Commit: " << commit;

    if (commit)
    {
        ui->requestedLabel->setText( QString::number( request ));
        m_modifyRequestAction->setVisible( true );
        m_fillRequestAction->setVisible( false );
    }
    else  {
        qDebug() << "Rollback" <<
                  QSqlDatabase::database().rollback();
    }
}

void MainWindow::disconnectClerk()
{
    DebugHelper debugHelper( Q_FUNC_INFO );

    m_inputModel->clear();
//    ui->tabWidget->setEnabled( false );
    ui->tabWidget->hide();
    ui->mainToolBar->hide();
//    ui->mainToolBar->setEnabled( false );
    ui->filterGroupBox->hide();
    ui->currentBookBox->hide();
//    ui->filterToggleButton->setEnabled( false );
    ui->filterToggleButton->hide();
    ui->boughtLessThanBox->setChecked( false );
    ui->boughtMoreThanBox->setChecked( false );
    ui->boughtLessThanBox->setChecked( false );
    ui->boughtMoreThanBox->setChecked( false );

//    ui->actionDisconnect->setEnabled( false );
    ui->actionDisconnect->setVisible( false );

    m_clerkID = 0;
}

void MainWindow::connectClerk()
{
    DebugHelper debugHelper( Q_FUNC_INFO );

    if (0 == m_clerkID)
    {
        qDebug() << "Not connected.";
        return;
    }

    ui->tabWidget->show();
    ui->mainToolBar->show();
    ui->filterToggleButton->show();

    ui->actionDisconnect->setVisible( true );
}

void MainWindow::showAbout()
{
    QMessageBox about;
    about.setWindowTitle(tr( "Help / About" ));
    about.setTextFormat(Qt::RichText);
    about.setText( tr("<h4>Application for bookstore clerk</h4>"
                      "<p>It was developed with sole purpose of completion DB&IS course</p>"
                      "<p>You can use it to find out popular books and request more of them "
                      "or to find out less popular books and group them into bundles "
                      "so people will buy them quickly.</p>"
                      "<p>Author: <a href='http://about.me/michael.pogoda'>Michael Pogoda</a></p>"
                      "<p>Visit <a href='http://github.com/MPogoda/bookstore_clerk'>github</a> for more info</p><br/>"
                      "The program is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE "
                      "WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE."));
    about.exec();
}

void MainWindow::showAboutQt()
{
    QMessageBox::aboutQt( this, tr("Bookstore Clerk") );
}

namespace
{
void findBookInfo( const QString& isbn, QString& title, uint& quantity, qreal& price, uint& year, QString& publisherName)
{
    DebugHelper debugHelper( Q_FUNC_INFO );
    QSqlQuery searchBook;
    searchBook.setForwardOnly( true );
    qDebug() << "Prepare: " <<
                searchBook.prepare( "SELECT title, price, quantity, year, publisher.name "
                                    "FROM book JOIN publisher ON publisher.publisher_id = book.publisher_id "
                                    "WHERE isbn = :isbn" );

    searchBook.bindValue( ":isbn", isbn );

    qDebug() << "Exec: " << searchBook.exec();
    qDebug() << "First: "<< searchBook.first();

    title = searchBook.value( 0 ).toString();
    price = searchBook.value( 1 ).toFloat();
    quantity = searchBook.value( 2 ).toUInt();
    year = searchBook.value( 3 ).toUInt();
    publisherName = searchBook.value( 4 ).toString();
    qDebug() << "Title: " << title << "Quantity: " << quantity
             << "Price: " << price << "Year: " << year
             << "Publisher Name: " << publisherName;
}

uint findRequestedAmmount( const QString& isbn, uint& clerkID )
{
    DebugHelper debugHelper( Q_FUNC_INFO );
    QSqlQuery findRequest;
    findRequest.setForwardOnly( true );

    qDebug() << "Prepare: " <<
                findRequest.prepare( "SELECT quantity, clerk_id FROM request WHERE isbn = :isbn" );

    findRequest.bindValue( ":isbn", isbn );
    qDebug() << "Exec: " << findRequest.exec();

    if (0 == findRequest.size())
    {
        clerkID = 0;
        return 0;
    }

    qDebug() << "First: " << findRequest.first();
    clerkID = findRequest.value( 1 ).toUInt();
    const uint requested = findRequest.value( 0 ).toUInt();

    qDebug() << "ClerkID: " << clerkID << "Requested: " << requested;
    return requested;
}
}

void MainWindow::saveBundle()
{
    DebugHelper debugHelper( Q_FUNC_INFO );

    if (!m_isBundleUnderConstruction || m_bundledISBNs.empty() || ui->bundleNameEdit->text().isNull()) {
        // can't do shit
        return;
    }

    DBOpener db(this);

#define _USE_PSQL
#ifdef _USE_PSQL
    QSqlQuery getBundleIdQuery;
    getBundleIdQuery.setForwardOnly( true );
    qDebug() << "Prepare: " <<
                getBundleIdQuery.prepare( "SELECT 1 + COUNT(*) FROM bundle" );
    qDebug() << "Exec: " << getBundleIdQuery.exec();
    qDebug() << "First: "<< getBundleIdQuery.first();

    const uint bundleID = getBundleIdQuery.value( 0 ).toUInt();
    qDebug() << "BundleID: " << bundleID;
#endif

    QSqlQuery addBundleQuery;
    qDebug() << "Prepare: " <<
                addBundleQuery.prepare( "INSERT INTO bundle (bundle_id, name, deleted, commnt) VALUES ("
                                    #ifdef _USE_PSQL
                                        ":bundleID"
                                    #else
                                        "bundle_sequence.NEXTVAL"
                                    #endif
                                        ",:name, 0, :commnt)");
#ifdef _USE_PSQL
    addBundleQuery.bindValue( ":bundleID", bundleID);
#endif
    addBundleQuery.bindValue( ":name", ui->bundleNameEdit->text());
    addBundleQuery.bindValue( ":commnt", ui->bundleCommentEdit->toPlainText());

    qDebug() << "Transaction: " <<
                QSqlDatabase::database().transaction();
    const bool addBundleResult = addBundleQuery.exec();
    qDebug() << "Exec: " << addBundleResult;
    if (!addBundleResult) {
        qDebug() << "Rollback" <<
                  QSqlDatabase::database().rollback();
        return;
    }

#ifndef _USE_PSQL
    QSqlQuery getBundleIdQuery;
    getBundleIdQuery.setForwardOnly( true );
    qDebug() << "Prepare: " <<
                getBundleIdQuery.prepare( "SELECT bundle_sequence.CURRVAL FROM dual" );
    qDebug() << "Exec: " << getBundleIdQuery.exec() << getBundleIdQuery.first();

    const uint bundleID = getBundleIdQuery.value( 0 ).toUInt();
#endif

    QSqlQuery addBookToBundleQuery;
    qDebug() << "Prepare: " <<
                addBookToBundleQuery.prepare( "INSERT INTO bundledbook (isbn, bundle_id, discount, deleted) VALUES "
                                              "( :isbn, :bundle_id, :discount, 0 )");
    addBookToBundleQuery.bindValue( ":bundle_id", bundleID );

    for (int i( 0 ); m_bundledISBNs.size() != i; ++i) {
        addBookToBundleQuery.bindValue( ":isbn", m_bundledISBNs.at( i ));
        addBookToBundleQuery.bindValue( ":discount", m_bundledDiscounts.at( i ));

        const bool addBookResult = addBookToBundleQuery.exec();
        qDebug() << "Exec: " << addBookResult;
        if (!addBookResult) {
            qDebug() << "Rollback" <<
                      QSqlDatabase::database().rollback();
            return;
        }
    }


    const bool commit = QSqlDatabase::database().commit();
    qDebug() << "Commit: " << commit;
    if (!commit) {
        qDebug() << "Rollback" <<
                  QSqlDatabase::database().rollback();
        return;
    }

    m_bundledISBNs.clear();
    m_bundledDiscounts.clear();
    m_bundledPrices.clear();
    m_bundleBookModel->setStringList( QStringList() );
    m_isBundleUnderConstruction = false;

}

void MainWindow::discountReset()
{
    DebugHelper debugHelper( Q_FUNC_INFO );

    const int row = m_bundleBookSelectionModel->currentIndex().row();

    const qreal newValue = (1.0 - m_bundledDiscounts.at( row )) * m_bundledPrices.at( row );

    ui->discountSpin->setValue( static_cast< uint >(100 * m_bundledDiscounts.at( row )));
    ui->discountedPriceLabel->setText( QString::number( newValue, 'f', 2));

    ui->saveDiscountButton->setEnabled( false );

}

void MainWindow::discountSave()
{
    DebugHelper debugHelper( Q_FUNC_INFO );

    const int row = m_bundleBookSelectionModel->currentIndex().row();

    const qreal oldValue = (1.0 - m_bundledDiscounts.at( row )) * m_bundledPrices.at( row );
    const qreal newValue = 0.01 * static_cast< qreal >(100 - ui->discountSpin->value()) * m_bundledPrices.at( row );
    const qreal delta = newValue - oldValue;

    ui->totalLabel->setText( QString::number(
                                 ui->totalLabel->text().toDouble() + delta
                                 , 'f', 2
                                 ));

    ui->savingsLabel->setText( QString::number(
                                   ui->savingsLabel->text().toDouble() - delta
                                   , 'f', 2
                                   ));

    m_bundledDiscounts[ row ] = 0.01 * static_cast< qreal>( ui->discountSpin->value() );

    ui->saveDiscountButton->setEnabled( false );
}

void MainWindow::discountChanged(const int value)
{
    DebugHelper debugHelper( Q_FUNC_INFO );
    const int row = m_bundleBookSelectionModel->currentIndex().row();

    const qreal discount = 0.01 * static_cast< qreal >( value );

    const qreal price = ( 1.0 - discount ) * m_bundledPrices.at( row );

    ui->discountedPriceLabel->setText( QString::number( price, 'f', 2));

    ui->saveDiscountButton->setEnabled( true );
}

void MainWindow::removeFromBundle()
{
    DebugHelper debugHelper( Q_FUNC_INFO );

    discountReset();

    const int row = m_bundleBookSelectionModel->currentIndex().row();

    const qreal savings = m_bundledPrices.at( row ) * m_bundledDiscounts.at( row );

    ui->savingsLabel->setText( QString::number(
                                   ui->savingsLabel->text().toDouble() - savings
                                   , 'f', 2));

    ui->totalLabel->setText( QString::number(
                                 ui->totalLabel->text().toDouble() - m_bundledPrices.at( row ) + savings
                                 , 'f', 2
                                 ));

    m_bundledDiscounts.removeAt( row );
    m_bundledPrices.removeAt( row );
    m_bundledISBNs.removeAt( row );
    m_bundleBookModel->removeRow( row );

    if (m_bundledISBNs.empty())
    {
        m_removeBookFromBundle->setVisible( false );
        ui->currentBookBox->hide();
    }
}

void MainWindow::addToBundle()
{
    DebugHelper debugHelper( Q_FUNC_INFO );

    if (!m_isBundleUnderConstruction)
    {
        if (QMessageBox::Yes == QMessageBox::warning( this, tr("No bundle under construction")
                              , tr("There is no bundle under construction. Want to create new?")
                              , QMessageBox::Yes, QMessageBox::Cancel) )
        {
            m_bundledDiscounts.clear();
            m_bundledISBNs.clear();
            m_bundleBookModel->setStringList( QStringList() );
            ui->bundleCommentEdit->clear();
            ui->bundleNameEdit->setText( "Some Bundle Name");
            ui->totalLabel->setText( QString::number(0.0, 'f', 2) );
            ui->savingsLabel->setText( QString::number( 0.0, 'f', 2));

            ui->tabBundleMod->setEnabled(true);

            m_isBundleUnderConstruction = true;
            m_saveBundleAction->setVisible( true );
        }
    }

    const int row = m_inputSelectionModel->currentIndex().row();
    if (-1 == row)
    {
        qDebug() << "No row is selected";
        return;
    }

    const QString isbn = ui->isbnLabel->text();
    qDebug() << "ISBN: " << isbn;

    if (m_bundledISBNs.contains( isbn ))
    {
        qDebug() << "Already in Bundle";
        QMessageBox::information( this, tr("Cannot add book to Bundle")
                                  , tr("That book is already in bundle under construction."));
        return;
    }

    const QString title = ui->titleLabel->text();
    const qreal price   = ui->priceLabel->text().toDouble();
    const QString year     = ui->yearLabel->text();
    const QString publisherName( ui->publisherLabel->text() );
    const QString authors = ui->authorsLabel->text();

    const QString bundleItem( tr("%0 by %1; %2 (%3)")
                              .arg( title )
                              .arg( authors )
                              .arg( publisherName )
                              .arg( year )
                            );

    m_bundledISBNs << isbn;
    m_bundledDiscounts << 0.0;
    m_bundledPrices << price;

    m_bundleBookModel->insertRow( m_bundleBookModel->rowCount() );
    QModelIndex index = m_bundleBookModel->index( m_bundleBookModel->rowCount() - 1);
    m_bundleBookModel->setData( index, bundleItem );

    ui->totalLabel->setText( QString::number(ui->totalLabel->text().toDouble() + price, 'f', 2));

    m_addToBundleAction->setVisible( false );
}

void MainWindow::inputViewSelectionChanged(const QModelIndex &current, const QModelIndex &previous)
{
    DebugHelper debugHelper( Q_FUNC_INFO );

    if (current.row() == previous.row())
    {
        qDebug() << "Row has not changed";
        return;
    }

    if (-1 == current.row())
    {
        ui->currentBookBox->hide();
        return;
    }

    if (-1 == previous.row())
    {
        ui->currentBookBox->show();
    }

    const QString isbn = m_inputModel->record( current.row() ).value( "isbn" ).toString();
    qDebug() << "Selected ISBN: " << isbn;

    DBOpener dBOpener( this );

    QString title;
    uint quantity;
    qreal price;
    uint year;
    QString publisherName;
    findBookInfo( isbn, title, quantity, price, year, publisherName);

    const uint sold = m_inputModel->record( current.row() ).value( "sold" ).toUInt();
    const QStringList authors = findAuthorsForBook( isbn );

    ui->isbnLabel->setText( isbn );
    ui->titleLabel->setText( title );
    ui->quantityLabel->setText( QString::number( quantity ));
    ui->priceLabel->setText( QString::number( price, 'f', 2));
    ui->yearLabel->setText( QString::number( year ));
    ui->publisherLabel->setText( publisherName );
    ui->soldLabel->setText( QString::number( sold ) );
    ui->authorsLabel->setText( authors.join( ", " ) );

    uint clerkID;
    const uint requestedAmmount = findRequestedAmmount( isbn, clerkID);
    if (0 == requestedAmmount) // No request found
    {
        ui->requestedLabel->setText( tr("None"));
        m_fillRequestAction->setVisible( true );
        m_modifyRequestAction->setVisible( false );
        m_removeRequestAction->setVisible( false );

    }
    else
    {
        ui->requestedLabel->setText( QString::number( requestedAmmount ));
        m_fillRequestAction->setVisible( false );
        // Enable modifying of request if this clerk has filled it previously
        m_modifyRequestAction->setVisible( clerkID == m_clerkID );
        m_removeRequestAction->setVisible( clerkID == m_clerkID );
    }

//    if (!m_isBundleUnderConstruction)
//        return;

    if (m_bundledISBNs.contains(isbn))
    {
        m_addToBundleAction->setVisible( false );
        // TODO: add delete book from bundle???
        return;
    }

    m_addToBundleAction->setVisible( true );
}

void MainWindow::bundledBookViewSelectionChanged(const QModelIndex &current, const QModelIndex &previous)
{
    DebugHelper debugHelper( Q_FUNC_INFO );

    if (current.row() == previous.row())
    {
        qDebug() << "Row has not changed";
        return;
    }

    if (-1 == current.row())
    {
        ui->currentBookBox->hide();
        return;
    }

    if (-1 == previous.row())
    {
        ui->currentBookBox->show();
    }

    const QString& isbn = m_bundledISBNs.at( current.row() );
    qDebug() << "Selected ISBN: " << isbn;

    DBOpener dBOpener( this );

    QString title;
    uint quantity;
    qreal price;
    uint year;
    QString publisherName;
    findBookInfo( isbn, title, quantity, price, year, publisherName);

    const uint sold = m_inputModel->record( current.row() ).value( "sold" ).toUInt();
    const QStringList authors = findAuthorsForBook( isbn );

    ui->isbnLabel->setText( isbn );
    ui->titleLabel->setText( title );
    ui->quantityLabel->setText( QString::number( quantity ));
    ui->priceLabel->setText( QString::number( price, 'f', 2));
    ui->yearLabel->setText( QString::number( year ));
    ui->publisherLabel->setText( publisherName );
    ui->soldLabel->setText( QString::number( sold ) );
    ui->authorsLabel->setText( authors.join( ", " ) );

    ui->discountSpin->setValue( m_bundledDiscounts.at( current.row() ));

    ui->discountedPriceLabel->setText( QString::number(
                                           (1.0 - m_bundledDiscounts.at( current.row() ) ) * m_bundledPrices.at( current.row() )
                                           , 'f', 2 ) );

    m_removeBookFromBundle->setVisible( true );
}

void MainWindow::processLogin()
{
    DebugHelper debugHelper( Q_FUNC_INFO);

    disconnectClerk();

    m_login->clear();
    if (QDialog::Accepted == m_login->exec())
    {
        DBOpener dbopener( this );

        qDebug() << "Trying to login with ID: " << m_login->userName() << "; and passwordHash = " <<
                    m_login->passwordHash();

        QSqlQuery searchPasswordHash;
        searchPasswordHash.setForwardOnly( true );
        qDebug() << "Prepare: " <<
                    searchPasswordHash.prepare( "SELECT COUNT(*) "
                                                "FROM clerk "
                                                "WHERE clerk_id = :clerkID "
                                                "AND password_hash = :passwordHash ");
        searchPasswordHash.bindValue( ":clerkID", m_login->userName() );
        searchPasswordHash.bindValue( ":passwordHash", m_login->passwordHash() );

        qDebug() << "Exec: " << searchPasswordHash.exec();
        qDebug() << "First: "<< searchPasswordHash.first();

        if (1 == searchPasswordHash.value( 0 ).toUInt())
        {
            m_clerkID = m_login->userName().toUInt();
            emit connected();
        }
        else
        {
            m_clerkID = 0;
            if (QMessageBox::Retry ==
                    QMessageBox::critical( this
                                           , tr("Login error")
                                           , tr("User with provided credentials does not exist! Retry?")
                                           , QMessageBox::Retry | QMessageBox::Cancel)
                    )
                QTimer::singleShot(10, this, SLOT(processLogin()));
        }
    }
}

void MainWindow::redrawView()
{
    DebugHelper debugHelper( Q_FUNC_INFO);
    DBOpener dbopener( this );

    QSqlQuery simpleSearch;
    qDebug() << "Prepare: " <<
                simpleSearch.prepare( "SELECT b.isbn AS isbn, COUNT(purchasing_date) AS sold, quantity "
                                      "FROM book b LEFT JOIN history_of_purchasing h ON h.isbn = b.isbn "
                                      "WHERE (quantity BETWEEN :fromStock AND :toStock) "
                                      // TODO: uncomment on ORCL
                                      //" AND (purchasing_date ISNULL OR purchasing_date >= trunc(sysdate - 7)) "
                                      "GROUP BY b.isbn "
                                      "HAVING count(*) BETWEEN :fromBought AND :toBought");
    simpleSearch.bindValue(":fromBought",
                           ui->boughtMoreThanBox->isChecked() ?
                               ui->boughtMoreThenSpin->value()
                             : -1
                               );
    simpleSearch.bindValue(":toBought",
                           ui->boughtLessThanBox->isChecked() ?
                               ui->boughtLessThenSpin->value()
                             : 9000
                               );
    simpleSearch.bindValue(":fromStock",
                           ui->instockMoreThanBox->isChecked() ?
                               ui->instockMoreThenSpin->value()
                             : 0
                               );
    simpleSearch.bindValue(":toStock",
                           ui->instockLessThanBox->isChecked() ?
                               ui->instockLessThenSpin->value()
                             : 9000
                               );

    qDebug() << "Exec: " << simpleSearch.exec();


    m_inputModel->setQuery(simpleSearch);
    statusBar()->showMessage(tr("%1 row(s) were found.").arg(m_inputModel->rowCount()));
    ui->tableView->resizeColumnsToContents();
}

void MainWindow::connectFilters() const
{
    connect(ui->boughtLessThenSpin, SIGNAL(valueChanged(int)), this, SLOT(boughtLessTrigger(int)));
    connect(ui->boughtMoreThenSpin, SIGNAL(valueChanged(int)), this, SLOT(boughtMoreTrigger(int)));
    connect(ui->instockLessThenSpin, SIGNAL(valueChanged(int)), this, SLOT(instockLessTrigger(int)));
    connect(ui->instockMoreThenSpin, SIGNAL(valueChanged(int)), this, SLOT(instockMoreTrigger(int)));

    connect( ui->boughtLessThanBox, SIGNAL(clicked(bool)), this, SLOT(boughtLessBoxTrigger(bool)));
    connect( ui->boughtMoreThanBox, SIGNAL(clicked(bool)), this, SLOT(boughtMoreBoxTrigger(bool)));
    connect( ui->instockLessThanBox, SIGNAL(clicked(bool)), this, SLOT(instockLessBoxTrigger(bool)));
    connect( ui->instockMoreThanBox, SIGNAL(clicked(bool)), this, SLOT(instockMoreBoxTrigger(bool)));
}

void MainWindow::disconnectFilters() const
{
    disconnect( ui->boughtLessThenSpin, SIGNAL(valueChanged(int)),this, SLOT(boughtLessTrigger(int)) );
    disconnect( ui->boughtMoreThenSpin, SIGNAL(valueChanged(int)),this, SLOT(boughtMoreTrigger(int)) );
    disconnect( ui->instockLessThenSpin, SIGNAL(valueChanged(int)),this, SLOT(instockLessTrigger(int)) );
    disconnect( ui->instockMoreThenSpin, SIGNAL(valueChanged(int)), this, SLOT(instockMoreTrigger(int)) );

    disconnect( ui->boughtLessThanBox, SIGNAL(clicked(bool)), this, SLOT(boughtLessBoxTrigger(bool)));
    disconnect( ui->boughtMoreThanBox, SIGNAL(clicked(bool)), this, SLOT(boughtMoreBoxTrigger(bool)));
    disconnect( ui->instockLessThanBox, SIGNAL(clicked(bool)), this, SLOT(instockLessBoxTrigger(bool)));
    disconnect( ui->instockMoreThanBox, SIGNAL(clicked(bool)), this, SLOT(instockMoreBoxTrigger(bool)));
}

void MainWindow::boughtLessBoxTrigger(const bool isOn)
{
    if (isOn)
        ui->boughtLessThenSpin->setValue( qMax( ui->boughtLessThenSpin->value()
                                              , ui->boughtMoreThenSpin->value() ));

    ui->customRadioButton->setChecked( true );
}

void MainWindow::boughtMoreBoxTrigger(const bool isOn)
{
    if (isOn)
        ui->boughtMoreThenSpin->setValue( qMin( ui->boughtLessThenSpin->value()
                                              , ui->boughtMoreThenSpin->value() ));
    ui->customRadioButton->setChecked( true );
}

void MainWindow::instockLessBoxTrigger(const bool isOn)
{
    if (isOn)
        ui->instockLessThenSpin->setValue( qMax( ui->instockLessThenSpin->value()
                                               , ui->instockMoreThenSpin->value() ));
    ui->customRadioButton->setChecked( true );
}

void MainWindow::instockMoreBoxTrigger(const bool isOn)
{
    if (isOn)
        ui->instockMoreThenSpin->setValue( qMin( ui->instockLessThenSpin->value()
                                               , ui->instockMoreThenSpin->value() ));
    ui->customRadioButton->setChecked( true );
}

void MainWindow::boughtMoreTrigger(const int val)
{
    ui->boughtLessThenSpin->setValue( qMax( val, ui->boughtLessThenSpin->value() ));

    ui->customRadioButton->setChecked( true );
}

void MainWindow::boughtLessTrigger(const int val)
{
    ui->boughtMoreThenSpin->setValue( qMin( val, ui->boughtMoreThenSpin->value() ));

    ui->customRadioButton->setChecked( true );
}

void MainWindow::instockMoreTrigger(const int val)
{
    ui->instockLessThenSpin->setValue( qMax( val, ui->instockLessThenSpin->value() ));

    ui->customRadioButton->setChecked( true );
}

void MainWindow::filterChanged(const int buttonId)
{
    if (2 != buttonId)
        disconnectFilters();

    switch (buttonId)
    {
    case 0: /* trending */
        ui->instockLessThanBox->setChecked( true );
        ui->instockLessThenSpin->setValue( 10 );
        ui->instockMoreThanBox->setChecked( false );
        ui->boughtLessThanBox->setChecked( false );
        ui->boughtMoreThanBox->setChecked( true );
        ui->boughtMoreThenSpin->setValue( 15 );
        break;
    case 1: /* overstocked */
        ui->instockLessThanBox->setChecked( false );
        ui->instockMoreThanBox->setChecked( true );
        ui->instockMoreThenSpin->setValue( 10 );
        ui->boughtMoreThanBox->setChecked( false );
        ui->boughtLessThanBox->setChecked( true );
        ui->boughtLessThenSpin->setValue( 5 );
        break;
    case 2:
    default:
        ; /* do nothing */
    }

    if (2 != buttonId)
        connectFilters();
}

void MainWindow::instockLessTrigger(const int val)
{
    ui->instockMoreThenSpin->setValue( qMin( val, ui->instockMoreThenSpin->value() ));

    ui->customRadioButton->setChecked( true );
}
