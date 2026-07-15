#include "MainWindow.h"

#include <QApplication>
#include <QMessageBox>

#include <exception>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("MyBarrier");
    QApplication::setOrganizationName("mybarrier");
    QApplication::setApplicationVersion("0.2.0");

    try {
        mybarrier::gui::MainWindow window;
        window.resize(1060, 760);
        window.show();
        return app.exec();
    } catch (const std::exception& ex) {
        QMessageBox::critical(nullptr, "MyBarrier", QString::fromLocal8Bit(ex.what()));
        return 1;
    }
}
