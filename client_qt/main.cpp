
#include <QApplication>
#include "mywin.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MyWin win;

    win.show();

    app.exec();
}
