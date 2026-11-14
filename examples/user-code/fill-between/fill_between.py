# Target sketch (milestone 6): fill-between/main.cpp as PySide code would read. Informative only.
import sys

import numpy as np
from PySide6.QtCore import Qt
from PySide6.QtGui import QColor, QPen
from PySide6.QtWidgets import QApplication
from NeoQCP import QCPScatterStyle, QCustomPlot

app = QApplication(sys.argv)
plot = QCustomPlot()
plot.resize(640, 400)

x = np.array([0, 1, 2, 3])
y1 = np.array([0.8, 0.8, 0.2, 0.2])
y2 = np.array([0, 0, 1, 1], dtype=float)

c0, c1 = QColor(31, 119, 180), QColor(255, 127, 14)
ax = plot.axisRect()
first = ax.line(x, y1, pen=QPen(c0, 1.5, Qt.DashLine),
                scatterStyle=QCPScatterStyle(QCPScatterStyle.ssDisc, c0, 7))
second = ax.line(x, y2, pen=QPen(c1, 1.5, Qt.DashLine),
                 scatterStyle=QCPScatterStyle(QCPScatterStyle.ssDisc, c1, 7))
ax.fillBetween(first, second, brush=QColor(44, 160, 44, 76), belowBrush=QColor(214, 39, 40, 76))
ax.setTitle("interpolation=True")


def move_second(event):
    global y2
    step = {Qt.Key_Up: 0.1, Qt.Key_Down: -0.1}.get(event.key(), 0.0)
    y2 = y2 + step
    second.setData(x, y2)  # the fill follows


plot.keyPress.connect(move_second)

plot.rescaleAxes()
plot.show()
app.exec()
