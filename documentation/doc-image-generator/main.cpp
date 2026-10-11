#include <QApplication>
#include "mainwindow.h"

// Usage: doc-image-generator [output-directory]   (default: ./images)
int main(int argc, char *argv[])
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication a(argc, argv);
  const QStringList args = a.arguments();
  MainWindow w(args.size() > 1 ? args.at(1) : QStringLiteral("images"));
  if (!w.ready())
    return 1;
  return a.exec();
}
