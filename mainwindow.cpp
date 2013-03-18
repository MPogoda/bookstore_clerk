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

namespace {
struct DebugHelper {
    const QString m_funcInfo;
    DebugHelper( const QString funcInfo ) : m_funcInfo(funcInfo)
    { qDebug() << "====ENTERING " << funcInfo << "========="; }
    ~DebugHelper() { qDebug() << "=====LEAVING " << m_funcInfo << "===========";}
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
}



MainWindow::~MainWindow()
{
    DebugHelper debugHelper( Q_FUNC_INFO);
    delete ui;
    delete m_simpleSearch;
    delete m_searchBook;
    delete m_searchAuthors;
    delete m_searchSold;
    delete m_searchPasswordHash;
    delete m_insertRequest;
}

void MainWindow::setup_connection()
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

void MainWindow::prepare_queries()
{
    DebugHelper debugHelper( Q_FUNC_INFO);
    m_simpleSearch = new QSqlQuery;
  //  m_simpleSearch->setForwardOnly( true );
    qDebug() << "SimpleSearch" <<
    m_simpleSearch->prepare("select   book.isbn as ISBN, count(*) as Sold, max(quantity) as Quantity "
                            "from     history_of_purchasing join book on book.isbn = history_of_purchasing.isbn "
                            "where    (purchasing_date >= :fromDate and purchasing_date <= :toDate) "
                            "     and (quantity >= :fromStock and quantity <= :toStock) "
                            "group by book.isbn "
                            "having   count(*) >= :fromBought and count(*) <= :toBought"
                            );
    m_simpleSearch->bindValue(":fromDate", "0001-01-01");
    m_simpleSearch->bindValue(":toDate", "3000-12-12");
    m_simpleSearch->bindValue(":fromStock", 0);
    m_simpleSearch->bindValue(":toStock", 9000);
    m_simpleSearch->bindValue(":fromBought", 0);
    m_simpleSearch->bindValue(":toBought", 9010);

    m_searchBook = new QSqlQuery;
    m_searchBook->setForwardOnly( true );
    qDebug() << "Search Book" <<
    m_searchBook->prepare( "select title, quantity from book where isbn = :isbn" );

    m_searchAuthors = new QSqlQuery;
    m_searchAuthors->setForwardOnly( true );
    qDebug() << "Search Authors" <<
    m_searchAuthors->prepare( "select name "
                              "from book join book_s_author "
                                        "on book_s_author.isbn = book.isbn "
                                    "join author "
                                        "on author.author_id = book_s_author.author_id "
                              "where book_s_author.isbn = :isbn ");

    m_searchSold = new QSqlQuery;
    m_searchSold->setForwardOnly( true );
    qDebug() << "Search Sold" <<
    m_searchSold->prepare( "select count(*) as sold "
                           "from history_of_purchasing join book "
                                                      "on book.isbn = history_of_purchasing.isbn "
                           "where purchasing_date > :fromDate "
                                  "and book.isbn = :isbn "
                           "group by book.isbn " );
    m_searchSold->bindValue( ":fromDate", "0001-01-01" );

    m_searchPasswordHash = new QSqlQuery;
    m_searchPasswordHash->setForwardOnly( true );
    qDebug() << "Search Password Hash" <<
    m_searchPasswordHash->prepare( "select count(*) "
                                   "from clerk "
                                   "where clerk_id = :clerkID "
                                          "and password_hash = :passwordHash ");

    m_insertRequest = new QSqlQuery;
    qDebug() << "Insert Request: " <<
    m_insertRequest->prepare( "insert into request( isbn, quantity, clerk_id ) values "
                                    "( :isbn, :quantity, :clerkID )");
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

    const QString isbn = m_inputModel->record( row ).value( "isbn" ).toString();
    qDebug() << "ISBN: " << isbn;
    m_searchBook->bindValue( ":isbn", isbn );
    qDebug() << "1st query: " << m_searchBook->exec() << m_searchBook->first();

    const QString title = m_searchBook->value( 0 ).toString();
    const uint quantity = m_searchBook->value( 1 ).toUInt();
    qDebug() << title << quantity;


    m_searchAuthors->bindValue( ":isbn", isbn );
    qDebug() << "2nd query: " << m_searchAuthors->exec();
    QStringList authors;
    while (m_searchAuthors->next())
        authors << m_searchAuthors->value( 0 ).toString();

    qDebug() << "authors: " << authors;

    m_searchSold->bindValue( ":isbn", isbn );
    qDebug() << "3rd query: " << m_searchSold->exec() << m_searchSold->first();
    const uint sold = m_searchSold->value( 0 ).toUInt();

    qDebug() << "sold: " << sold;

    m_fillRequest->prepareForm( isbn, title, authors.join(", "), quantity, sold);
    if (QDialog::Accepted != m_fillRequest->exec())
    {
        qDebug() << "CANCELLED";
        return;
    }

    const uint request = m_fillRequest->quantity();

    m_insertRequest->bindValue( ":isbn", isbn );
    m_insertRequest->bindValue( ":quantity", request );
    m_insertRequest->bindValue( ":clerkID", m_clerkID );

    qDebug() << "TRANSACTION: " <<
    QSqlDatabase::database().transaction();
    qDebug() << "INSERT : " <<
                m_insertRequest->exec();
    qDebug() << "COMMIT: " <<
                QSqlDatabase::database().commit();
    // TODO: prepare insert query.
    // TODO: clerkID...
}


void MainWindow::processLogin()
{
    DebugHelper debugHelper( Q_FUNC_INFO);
    ui->tabWidget->setEnabled(false);
    m_login->clear();
    if (QDialog::Accepted == m_login->exec())
    {
        // TODO: clear all tables, make initial state
        m_inputModel->clear();

        qDebug() << "Trying to login with ID: " << m_login->userName() << "; and passwordHash = " <<
                    m_login->passwordHash();
        m_searchPasswordHash->bindValue( ":clerkID", m_login->userName() );
        m_searchPasswordHash->bindValue( ":passwordHash", m_login->passwordHash() );
        qDebug() << "Executing password hash query: " << m_searchPasswordHash->exec()
                                                      << m_searchPasswordHash->first();
        if (1 == m_searchPasswordHash->value( 0 ).toUInt())
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
    //    if (NULL == m_inputModel)
    //        m_inputModel = new QSqlQueryModel( this );
    //    if (NULL != m_inputModel)
    //    {
    //        ui->tableView->setModel(NULL);
    //        delete m_inputModel;
    //    }
    //    m_inputModel = new QSqlQueryModel(this);
    m_simpleSearch->bindValue(":fromBought",
                              ui->boughtMoreThanBox->isChecked() ?
                                  ui->boughtMoreThenSpin->value()
                                : 0
                                  );
    m_simpleSearch->bindValue(":toBought",
                              ui->boughtLessThanBox->isChecked() ?
                                  ui->boughtLessThenSpin->value()
                                : 9000
                                  );
    m_simpleSearch->bindValue(":fromStock",
                              ui->instockMoreThanBox->isChecked() ?
                                  ui->instockMoreThenSpin->value()
                                : 0
                                  );
    m_simpleSearch->bindValue(":toStock",
                              ui->instockLessThanBox->isChecked() ?
                                  ui->instockLessThenSpin->value()
                                : 9000
                                  );

    qDebug() << m_simpleSearch->exec();

    statusBar()->showMessage(tr("%1 row(s) were found.").arg(m_simpleSearch->size()));

    m_inputModel->setQuery(*m_simpleSearch);

    ui->tabWidget->setEnabled(true);
}

void MainWindow::connect_filters()
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

void MainWindow::disconnect_filters()
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
