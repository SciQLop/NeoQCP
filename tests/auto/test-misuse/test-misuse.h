#include <QtTest/QtTest>
#include "qcustomplot.h"

class TestMisuse : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void checkPassesSilently();
    void checkReportsWhereAndWhat();
    void checkOrReturnRefuses();
    void setMisuseHandlerReturnsPrevious();
};
