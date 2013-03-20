#pragma once

#include <QDialog>

namespace Ui {
class FillRequestDialog;
}

class FillRequestDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit FillRequestDialog(QWidget * const parent = NULL);
    ~FillRequestDialog();

    /**
     * @brief prepareForm prepares form with initial quantity
     */
    void prepareForm( const uint quantity );

    uint quantity() const;
private:
    Ui::FillRequestDialog * const ui;
};
