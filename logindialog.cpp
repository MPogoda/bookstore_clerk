#include "logindialog.h"
#include "ui_logindialog.h"
#include <QCryptographicHash>

LoginDialog::LoginDialog(QWidget *parent)
  : QDialog(parent)
  , ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(store_credentials()));
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

void LoginDialog::clear()
{
    ui->passwordEdit->clear();
    ui->userEdit->clear();
    ui->userEdit->setFocus();
    m_passwordHash.clear();
    m_userName.clear();
}

void LoginDialog::store_credentials()
{
    m_passwordHash = QCryptographicHash::hash((ui->passwordEdit->text().toStdString().c_str()), QCryptographicHash::Md5).toHex();
    m_userName = ui->userEdit->text();
}
