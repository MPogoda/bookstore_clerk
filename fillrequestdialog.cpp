#include "fillrequestdialog.h"
#include "ui_fillrequestdialog.h"

FillRequestDialog::FillRequestDialog(QWidget * const parent)
  : QDialog(parent)
  , ui(new Ui::FillRequestDialog)
{
    ui->setupUi(this);
}

FillRequestDialog::~FillRequestDialog()
{
    delete ui;
}

void FillRequestDialog::prepareForm( const uint quantity)
{
    ui->quantityBox->setValue( quantity );
}

uint FillRequestDialog::quantity() const
{
    return ui->quantityBox->value();
}
