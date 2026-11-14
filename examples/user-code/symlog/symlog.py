# Target sketch (milestone 6): symlog/main.cpp as PySide code would read. Informative only.
import sys

import numpy as np
from PySide6.QtGui import QColor, QPen
from PySide6.QtWidgets import QApplication
from NeoQCP import QCP, QCPScale, QCustomPlot

app = QApplication(sys.argv)
plot = QCustomPlot()
plot.resize(640, 720)

x = np.arange(-50.0, 50.0, 0.01)
y = np.arange(0, 100.0, 0.01)

ax0 = plot.axisRect()
ax1 = plot.addAxisRect()
ax2 = plot.addAxisRect()
blue = QPen(QColor(31, 119, 180), 1.5)

ax0.line(x, y, pen=blue)
ax0.xAxis().setScale(QCPScale.symlog())
ax0.xAxis().grid().setSubGridVisible(True)
ax0.yAxis().setLabel("symlogx")

ax1.line(y, x, pen=blue)
ax1.yAxis().setScale(QCPScale.symlog())
ax1.yAxis().setLabel("symlogy")
ax1.xAxis().grid().setVisible(False)
ax1.yAxis().grid().setVisible(False)

ax2.line(x, np.sin(x / 3.0), pen=blue)
ax2.xAxis().setScale(QCPScale.symlog())
ax2.yAxis().setScale(QCPScale.symlog(0.015))
ax2.yAxis().setLabel("symlog both")

plot.setInteractions(QCP.iRangeDrag | QCP.iRangeZoom)
plot.rescaleAxes()
plot.show()
app.exec()
