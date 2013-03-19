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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_clerkID( 0 )
    , m_login(new LoginDialog(this))
    , m_fillRequest( new FillRequestDialog( this ))
    , m_inputModel( new QSqlQueryModel( this ) )
    , m_inputSelectionModel( new QItemSelectionModel( m_inputModel, this ) )
    , m_filterButtons( new QButtonGroup( this ) )
{
    DebugHelper debugHelper( Q_FUNC_INFO);

    ui->setupUi(this);
    ui->filterGroupBox->hide();

    QAction *fillRequestAction = new QAction( tr("Request"), this);
    fillRequestAction->setShortcut( QKeySequence::MoveToStartOfLine);
    fillRequestAction->setToolTip( tr("Fill request for selected book") );
    ui->mainToolBar->addAction( fillRequestAction);

    m_filterButtons->addButton( ui->trendingRadioButton, 0);
    m_filterButtons->addButton( ui->overstockedRadioButton, 1 );
    m_filterButtons->addButton( ui->customRadioButton, 2 );

    /* setup connection to database */
    setup_connection();

    connect(this, SIGNAL(connected()), this, SLOT(redrawView()));
    QTimer::singleShot(10, this, SLOT(processLogin()));

    ui->tableView->setModel(m_inputModel);
    ui->tableView->setSelectionModel( m_inputSelectionModel );

    connect( m_filterButtons, SIGNAL(buttonClicked(int)), this, SLOT(filterChanged(int)));
    connect_filters();

    connect( ui->filterButton, SIGNAL(clicked()), this, SLOT(redrawView()));
    connect( fillRequestAction, SIGNAL(triggered()), this, SLOT(fillRequest()));

    connect( ui->actionDisconnect, SIGNAL(triggered()), this, SLOT(disconnect_clerk()) );
    connect( ui->actionReconnect, SIGNAL(triggered()), this, SLOT(processLogin()) );
    connect( this, SIGNAL(connected()), this, SLOT(connect_clerk()) );
}

MainWindow::~MainWindow()
{
    DebugHelper debugHelper( Q_FUNC_INFO);
    delete ui;
}

void MainWindow::setup_connection() const
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
 * @brief find_request_for_book checks whether there is request for that book already
 * @param isbn ISBN number of book
 * @return true if exists
 */
bool find_request_for_book(const QString& isbn )
{
    DebugHelper debugHelper( Q_FUNC_INFO );
    // assume db is opened
    QSqlQuery searchRequest;
    searchRequest.setForwardOnly( true );
    qDebug() << "Prepare: " <<
                searchRequest.prepare( "select count(*) from request where isbn = :isbn" );
    searchRequest.bindValue( ":isbn", isbn );
    qDebug() << "Exec: " << searchRequest.exec() << searchRequest.first();

    return (0 != searchRequest.value( 0 ).toUInt());
}

/**
 * @brief find_books_title_and_quantity finds book's title and available quantity
 * @param isbn  ISBN number of book
 * @param title found book's title
 * @param quantity found book's quantity
 */
void find_books_title_and_quantity( const QString& isbn, QString& title, uint& quantity)
{
    DebugHelper debugHelper( Q_FUNC_INFO );
    QSqlQuery searchBook;
    searchBook.setForwardOnly( true );
    qDebug() << "Prepare: " <<
                searchBook.prepare( "select title, quantity from book where isbn = :isbn" );

    searchBook.bindValue( ":isbn", isbn );

    qDebug() << "Exec: " << searchBook.exec() << searchBook.first();

    title = searchBook.value( 0 ).toString();
    quantity = searchBook.value( 1 ).toUInt();
    qDebug() << "Title: " << title << "; Quantity: " << quantity;
}

/**
 * @brief find_authors_for_book get all authors that have written book
 * @param isbn ISBN number of book
 * @return StringList of authors
 */
const QStringList find_authors_for_book( const QString& isbn )
{
    DebugHelper debugHelper( Q_FUNC_INFO );
    QSqlQuery searchAuthors;
    searchAuthors.setForwardOnly( true );
    qDebug() << "Prepare: " <<
        searchAuthors.prepare( "select name "
                               "from book join book_s_author "
                                         "on book_s_author.isbn = book.isbn "
                                         "join author "
                                         "on author.author_id = book_s_author.author_id "
                               "where book_s_author.isbn = :isbn ");

    searchAuthors.bindValue( ":isbn", isbn );
    qDebug() << "Exec: " << searchAuthors.exec();

    QStringList authors;
    while (searchAuthors.next())
        authors << searchAuthors.value( 0 ).toString();

    qDebug() << "Authors: " << authors;

    return authors;
}

/**
 * @brief find_sold_number finds how many books with that ISBN have been sold past week (TODO)
 * @param isbn ISBN number of book
 * @return number of succesful purchases for past week
 */
uint find_sold_number( const QString& isbn)
{
    DebugHelper debugHelper( Q_FUNC_INFO );
    QSqlQuery searchSold;
    searchSold.setForwardOnly( true );
    qDebug() << "Prepare: " <<
                searchSold.prepare( "select count(*) as sold "
                                    "from history_of_purchasing join book "
                                    "on book.isbn = history_of_purchasing.isbn "
                                    "where purchasing_date > :fromDate "
                                    "and book.isbn = :isbn "
                                    "group by book.isbn " );

    // TODO: bind week before current
    searchSold.bindValue( ":fromDate", "0001-01-01" );
    searchSold.bindValue( ":isbn", isbn );

    qDebug() << "Exec: " << searchSold.exec() << searchSold.first();

    const uint sold = searchSold.value( 0 ).toUInt();
    qDebug() << "Sold: " << sold;

    return sold;
}
}

void MainWindow::fillRequest()
{
    DebugHelper debugHelper( Q_FUNC_INFO);
    DBOpener    dbopener( this );

    const int row = m_inputSelectionModel->currentIndex().row();
    if (-1 == row)
    {
        qDebug() << "No row is selected";
        return;
    }

    const QString isbn = m_inputModel->record( row ).value( "isbn" ).toString();
    qDebug() << "ISBN: " << isbn;

    //check if there already request for that ISBN
    if ( find_request_for_book( isbn ) )
    {
        qDebug() << "Found request :<";
        QMessageBox::information( this, tr("Cannot add request"), tr("Request already exists! Contact administrator"));
        return;
    }

    QString title;
    uint quantity;
    find_books_title_and_quantity( isbn, title, quantity);

    const QStringList authors = find_authors_for_book( isbn );

    const uint sold = find_sold_number( isbn );

    m_fillRequest->prepareForm( isbn, title, authors.join(", "), quantity, sold);
    if (QDialog::Accepted != m_fillRequest->exec())
    {
        qDebug() << "Request has been cancelled!";
        return;
    }

    const uint request = m_fillRequest->quantity();

    QSqlQuery insertRequest;
    qDebug() << "Prepare: " <<
                insertRequest.prepare( "insert into request( isbn, quantity, clerk_id ) values "
                                       "( :isbn, :quantity, :clerkID )");

    insertRequest.bindValue( ":isbn", isbn );
    insertRequest.bindValue( ":quantity", request );
    insertRequest.bindValue( ":clerkID", m_clerkID );

    qDebug() << "Transaction: " <<
                QSqlDatabase::database().transaction();
    qDebug() << "Exec: " <<
                insertRequest.exec();
    qDebug() << "Commit: " <<
                QSqlDatabase::database().commit();
}

void MainWindow::disconnect_clerk()
{
    DebugHelper debugHelper( Q_FUNC_INFO );

    m_inputModel->clear();
    ui->tabWidget->setEnabled( false );
    ui->mainToolBar->setEnabled( false );
    ui->filterGroupBox->hide();
    ui->filterToggleButton->setEnabled( false );
    ui->boughtLessThanBox->setChecked( false );
    ui->boughtMoreThanBox->setChecked( false );
    ui->boughtLessThanBox->setChecked( false );
    ui->boughtMoreThanBox->setChecked( false );

    ui->actionDisconnect->setEnabled( false );

    m_clerkID = 0;
}

void MainWindow::connect_clerk()
{
    DebugHelper debugHelper( Q_FUNC_INFO );

    if (0 == m_clerkID)
    {
        qDebug() << "Not connected.";
        return;
    }

    ui->tabWidget->setEnabled( true );
    ui->mainToolBar->setEnabled( true );
    ui->filterToggleButton->setEnabled( true );

    ui->actionDisconnect->setEnabled( true );
}

void MainWindow::processLogin()
{
    DebugHelper debugHelper( Q_FUNC_INFO);

    disconnect_clerk();

    m_login->clear();
    if (QDialog::Accepted == m_login->exec())
    {
        DBOpener dbopener( this );

        qDebug() << "Trying to login with ID: " << m_login->userName() << "; and passwordHash = " <<
                    m_login->passwordHash();

        QSqlQuery searchPasswordHash;
        searchPasswordHash.setForwardOnly( true );
        qDebug() << "Prepare: " <<
                    searchPasswordHash.prepare( "select count(*) "
                                                "from clerk "
                                                "where clerk_id = :clerkID "
                                                "and password_hash = :passwordHash ");
        searchPasswordHash.bindValue( ":clerkID", m_login->userName() );
        searchPasswordHash.bindValue( ":passwordHash", m_login->passwordHash() );

        qDebug() << "Exec: " << searchPasswordHash.exec()
                 << searchPasswordHash.first();

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
                simpleSearch.prepare("select   book.isbn as ISBN, count(*) as Sold, max(quantity) as Quantity "
                                     "from     history_of_purchasing join book on book.isbn = history_of_purchasing.isbn "
                                     "where    (purchasing_date >= :fromDate and purchasing_date <= :toDate) "
                                     "     and (quantity >= :fromStock and quantity <= :toStock) "
                                     "group by book.isbn "
                                     "having   count(*) >= :fromBought and count(*) <= :toBought"
                                     );
    // TODO: bind past week date
    simpleSearch.bindValue(":fromDate", "0001-01-01");
    simpleSearch.bindValue(":toDate", "3000-12-12");

    simpleSearch.bindValue(":fromBought",
                           ui->boughtMoreThanBox->isChecked() ?
                               ui->boughtMoreThenSpin->value()
                             : 0
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

    statusBar()->showMessage(tr("%1 row(s) were found.").arg(simpleSearch.size()));

    m_inputModel->setQuery(simpleSearch);
}

void MainWindow::connect_filters() const
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

void MainWindow::disconnect_filters() const
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
        disconnect_filters();

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
        connect_filters();
}


void MainWindow::instockLessTrigger(const int val)
{
    ui->instockMoreThenSpin->setValue( qMin( val, ui->instockMoreThenSpin->value() ));

    ui->customRadioButton->setChecked( true );
}
