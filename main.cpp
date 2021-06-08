#include "InteractiveDigitalMontage.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    InteractiveDigitalMontage w;
    w.show();
    return a.exec();
}
