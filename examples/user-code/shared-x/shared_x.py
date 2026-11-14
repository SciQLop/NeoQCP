# Target sketch (milestone 6): shared-x/main.cpp as PySide code would read. Informative only.
import sys

import numpy as np
from PySide6.QtGui import QColor, QPen
from PySide6.QtWidgets import QApplication
from NeoQCP import QCP, QCustomPlot

app = QApplication(sys.argv)
plot = QCustomPlot()
plot.resize(640, 480)

t1 = np.linspace(0, 8, 201)
y1 = np.sin(2 * np.pi * t1)
t2 = np.linspace(2, 10, 201)
y2 = 20 * np.cos(2 * np.pi * t2) ** 2 * np.exp(-0.3 * t2)

plot.setAxisRectGrid(2, 1, shareX=True)
ax1 = plot.axisRect(0, 0)
ax2 = plot.axisRect(1, 0)
blue = QPen(QColor(31, 119, 180), 1.5)

ax1.line(t1, y1, pen=blue)
ax1.yAxis().setLabel("Signal 1")

ax2.line(t2, y2, pen=blue)
ax2.yAxis().setLabel("Signal 2")
ax2.xAxis().setLabel("Time (s)")

plot.setInteractions(QCP.iRangeDrag | QCP.iRangeZoom)
plot.rescaleAxes()
plot.show()
app.exec()
