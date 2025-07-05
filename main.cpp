#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);  // Initializes the Qt application
    MainWindow w;                  // Creates the main window
    w.show();                      // Shows the main window
    return app.exec();             // Enters the event loop
}
