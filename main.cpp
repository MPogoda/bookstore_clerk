#include "mainwindow.h"
#include <QApplication>
#include <stdexcept>
#include <QDebug>


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCoreApplication::setOrganizationName( "Bookstore" );
    QCoreApplication::setOrganizationDomain( "example.com" );
    QCoreApplication::setApplicationName( "bookstore_clerk" );

    MainWindow w;
    w.show();
    
    return a.exec();
}
