#include "fillrequestdialog.h"
#include "ui_fillrequestdialog.h"

FillRequestDialog::FillRequestDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FillRequestDialog)
{
    ui->setupUi(this);
}

FillRequestDialog::~FillRequestDialog()
{
    delete ui;
}

void FillRequestDialog::prepareForm( const QString &isbn
                                   , const QString &title
                                   , const QString &authors
                                   , const uint quantity
                                   , const uint sold)
{
    ui->isbnLabel->setText( isbn );
    ui->titleLabel->setText( title );
    ui->authorsLabel->setText( authors );
    ui->quantityLabel->setText( QString::number( quantity ));
    ui->soldLabel->setText( QString::number( sold ));

    ui->commentEdit->clear();
    ui->quantityBox->setValue( 1 );
}

const QString FillRequestDialog::comment() const
{
    return ui->commentEdit->text();
}


uint FillRequestDialog::quantity() const
{
    return ui->quantityBox->value();
}
