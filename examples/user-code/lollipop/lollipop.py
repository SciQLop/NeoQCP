# Target sketch (milestone 6): lollipop.cpp as PySide code would read. Informative only.
import sys

from PySide6.QtGui import QColor, QPen
from PySide6.QtWidgets import QApplication
from NeoQCP import QCP, QCPGraphPlottable1D, QCustomPlot

px = QCP.PixelLength  # the C++ 5_px literal


# The binding exposes QCPPlottable1D<QCPGraphData> under one plain name.
class QCPLollipop(QCPGraphPlottable1D):
    def draw(self, context):
        for i in range(self.dataCount()):
            point = self.at(i)
            tip = QCP.Data(point.key, point.value)
            picked = self.selection().contains(i)
            stem = QPen(self.pen().color(), 3.5 if picked else 1.5)
            context.setDataIndex(i)
            context.line(QCP.Data(point.key, 0), tip, stem)
            context.circle(tip, px(8 if picked else 5), stem, self.brush())

    def drawLegendIcon(self, icon):
        icon.line(QCP.AxisRatio(0.5, 1.0), QCP.AxisRatio(0.5, 0.4), self.pen())
        icon.circle(QCP.AxisRatio(0.5, 0.35), px(3), self.pen(), self.brush())


app = QApplication(sys.argv)
plot = QCustomPlot()
plot.resize(640, 400)

month = range(1, 13)
rainfall = [78, 61, 55, 52, 63, 49, 62, 53, 47, 64, 79, 76]

blue = QColor(31, 119, 180)
ax = plot.axisRect()
lollipops = (ax.add(QCPLollipop, month, rainfall)
             .setName("rainfall (mm)").setPen(QPen(blue)).setBrush(blue))
lollipops.setSelectable(QCP.stSingleData)

plot.setInteractions(QCP.iSelectPlottables)
plot.legend.setVisible(True)
ax.xAxis().setLabel("month")
plot.rescaleAxes()
plot.show()
app.exec()
