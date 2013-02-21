#include "logindialog.h"
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_isOnline(false)
    , m_login(new LoginDialog(this))
    , m_model(NULL)
{
    ui->setupUi(this);
    ui->toolBox->hide();

    if (!connect_to_database())
    {
        QMessageBox::critical(this, "Database connection error", "Cannot establish connection to database");
        throw std::logic_error("Cannot establish database connection");
    }

    connect(this, SIGNAL(connected()), this, SLOT(redrawView()));
    QTimer::singleShot(10, this, SLOT(processLogin()));

    m_simpleSearch = new QSqlQuery();
    m_simpleSearch->prepare("select   book.isbn,count(*),max(quantity) "
                           "from     history_of_purchasing join book on book.isbn = history_of_purchasing.isbn "
                           "where    (date between :fromDate and :toDate) "
                           "     and (quantity between :fromStock and :toStock) "
                           "group by book.isbn "
                           "having   count(*) between :fromBought and :toBought"
                           );
    m_simpleSearch->bindValue(":fromDate", "0001-01-01");
    m_simpleSearch->bindValue(":toDate", "3000-12-12");
    m_simpleSearch->bindValue(":fromStock", 0);
    m_simpleSearch->bindValue(":toStock", 9000);
    m_simpleSearch->bindValue(":fromBought", 0);
    m_simpleSearch->bindValue(":toBought123", 9010);

    connect(ui->boughtLessThenSpin, SIGNAL(valueChanged(int)), this, SLOT(boughtLessTrigger(int)));
    connect(ui->boughtMoreThenSpin, SIGNAL(valueChanged(int)), this, SLOT(boughtMoreTrigger(int)));
    connect(ui->instockLessThenSpin, SIGNAL(valueChanged(int)), this, SLOT(instockLessTrigger(int)));
    connect(ui->instockMoreThenSpin, SIGNAL(valueChanged(int)), this, SLOT(instockMoreTrigger(int)));
    connect(ui->boughtLessThanBox, SIGNAL(toggled(bool)), this, SLOT(redrawView()));
    connect(ui->boughtMoreThanBox, SIGNAL(toggled(bool)), this, SLOT(redrawView()));
    connect(ui->instockMoreThanBox, SIGNAL(toggled(bool)), this, SLOT(redrawView()));
    connect(ui->instockLessThanBox, SIGNAL(toggled(bool)), this, SLOT(redrawView()));
}

void MainWindow::boughtMoreTrigger(int val)
{
    if (ui->boughtLessThenSpin->value() < val)
        ui->boughtLessThenSpin->setValue( val );

    redrawView();
}

void MainWindow::boughtLessTrigger(int val)
{
    if (ui->boughtMoreThenSpin->value() > val)
        ui->boughtMoreThenSpin->setValue(val);

    redrawView();
}

void MainWindow::instockMoreTrigger(int val)
{
    if (ui->instockLessThenSpin->value() < val)
        ui->instockLessThenSpin->setValue( val );

    redrawView();
}

void MainWindow::instockLessTrigger(int val)
{
    if (ui->instockMoreThenSpin->value() > val)
        ui->instockMoreThenSpin->setValue(val);

    redrawView();
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::connect_to_database()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL");
    db.setHostName("localhost");
    db.setDatabaseName("bookstore");
    db.setUserName("username");
    db.setPassword("password");
    return db.open();
}

void MainWindow::processLogin()
{
    ui->tabWidget->setEnabled(false);
    m_login->clear();
    if (QDialog::Accepted == m_login->exec())
    {
        // TODO: clear all tables, make initial state
        if (m_login->passwordHash() == "a3f05c8283e5350106829f855c93c07d")
        {
            m_isOnline = true;
            emit connected();
        }
        else
        {
            m_isOnline = false;
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
    if (NULL != m_model)
    {
        ui->tableView->setModel(NULL);
        delete m_model;
    }
    m_model = new QSqlQueryModel(this);
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
    qDebug() << m_simpleSearch->exec() << m_simpleSearch->size();
    statusBar()->showMessage(tr("%1 rows were found.").arg(m_simpleSearch->size()));
    m_model->setQuery(*m_simpleSearch);

    ui->tableView->setModel(m_model);
    ui->tabWidget->setEnabled(true);
}
