#include <QtTest/QtTest>
#include "qcustomplot.h"

class TestDatacontainer : public QObject
{
  Q_OBJECT
private slots:
  void init();
  void cleanup();
  
  void sizeAndClear();
  void setSorted();
  void setUnsorted();
  void addSorted();
  void addUnsorted();
  void remove();
  void removeBefore();
  void removeAfter();
  
private:
  bool isSorted();
  bool isSameData(QVector<QCPGraphData> data, const QCPGraphDataContainer *container);
  void dumpContainer(const QCPGraphDataContainer *container, const QString &csvFilename);
  
  QCPDataContainer<QCPGraphData> *mData;
};

class BadRandom
{
public:
  BadRandom(quint32 seed, double min, double max);
  double get();
private:
  quint32 mState;
  double mMin, mMax;
};





