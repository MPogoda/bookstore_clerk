#pragma once

#include <QDialog>

namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit LoginDialog(QWidget *parent = 0);
    ~LoginDialog();
    
    const QString& passwordHash() const { return m_passwordHash; }
    const QString& userName()     const { return m_userName;     }
    void clear();
private:
    Ui::LoginDialog *ui;
    QString m_passwordHash;
    QString m_userName;
private slots:
    void store_credentials();
};

