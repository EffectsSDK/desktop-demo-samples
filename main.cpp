#include "sample.h"

#include <QtWidgets>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName(TSVB_COMPANY);
    app.setApplicationName(TSVB_APP_BIN_NAME);
    app.setApplicationDisplayName(TSVB_APP_NAME);
    app.setApplicationVersion(TSVB_VERSION_STRING);

    Sample sample;
    sample.show();

    return app.exec();
};
