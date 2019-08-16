/*copyright 2019 mateczek
Ten plik jest częścią bramkaModbus.

    bramkaModbus jest wolnym oprogramowaniem: możesz go rozprowadzać dalej
    i/lub modyfikować na warunkach Powszechnej Licencji Publicznej GNU,
    wydanej przez Fundację Wolnego Oprogramowania - według wersji 3 tej
    Licencji lub (według twojego wyboru) którejś z późniejszych wersji.

    bramkaModbus rozpowszechniany jest z nadzieją, iż będzie on
    użyteczny - jednak BEZ JAKIEJKOLWIEK GWARANCJI, nawet domyślnej
    gwarancji PRZYDATNOŚCI HANDLOWEJ albo PRZYDATNOŚCI DO OKREŚLONYCH
    ZASTOSOWAŃ. W celu uzyskania bliższych informacji sięgnij do     Powszechnej Licencji Publicznej GNU.
    <http://www.gnu.org/licenses/>.
*/
#include <QCoreApplication>
#include<bramkamodbus.h>
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    bramkaModbus bm;
    QObject::connect(&bm,SIGNAL(quit()),&a,SLOT(quit()));
    return a.exec();
}
