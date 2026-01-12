#include <QApplication>
#include <QMessageBox>
#include "MainWindow.h"
#include "FfiWrapper.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("DA Table Viewer");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("DA Modding Tools");

    // Initialize FFI
    if (!FfiWrapper::instance().initialize()) {
        QMessageBox::critical(nullptr, "Error",
            "Failed to load da_ffi.dll.\n\n"
            "Make sure the DLL is in the same directory as the executable.");
        return 1;
    }

    MainWindow window;
    window.show();

    return app.exec();
}
