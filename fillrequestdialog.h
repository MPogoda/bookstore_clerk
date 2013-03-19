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

    /**
     * @brief prepareForm prepares form with various info about book
     * @param isbn  book's ISBN
     * @param title book's title
     * @param authors book's authors
     * @param quantity book's quantity in stock
     * @param sold how many books with that ISBN have been sold
     */
    void prepareForm( const QString& isbn
                    , const QString& title
                    , const QString& authors
                    , const uint quantity
                    , const uint sold );

    uint quantity() const;
private:
    Ui::FillRequestDialog *ui;
};
