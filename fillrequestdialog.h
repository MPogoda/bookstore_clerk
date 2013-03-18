#pragma once

#include <QDialog>

namespace Ui {
class FillRequestDialog;
}

class FillRequestDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit FillRequestDialog(QWidget *parent = 0);
    ~FillRequestDialog();

    void prepareForm( const QString& isbn
                    , const QString& title
                    , const QString& authors
                    , const uint quantity
                    , const uint sold );

    const QString comment() const;
    uint quantity() const;
    
private:
    Ui::FillRequestDialog *ui;
};
