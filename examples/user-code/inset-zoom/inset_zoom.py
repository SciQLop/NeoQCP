# Target sketch (milestone 6): inset-zoom/main.cpp as PySide code would read. Informative only.
import sys

import numpy as np
from PySide6.QtCore import QRectF, Qt
from PySide6.QtGui import QPen
from PySide6.QtWidgets import QApplication
from NeoQCP import QCP, QCPColorGradient, QCustomPlot


def bivariate_normal(x, y, sx, sy, mx, my):
    u, v = (x - mx) / sx, (y - my) / sy
    return np.exp(-0.5 * (u * u + v * v)) / (2 * np.pi * sx * sy)


app = QApplication(sys.argv)
plot = QCustomPlot()
plot.resize(560, 540)

# matplotlib's sample axes_grid/bivariate_normal.npy, pasted into a 150 x 150 zero image.
X, Y = np.meshgrid(np.arange(-3.0, 4.001, 0.5), np.arange(-4.0, 3.001, 0.5))
Z2 = np.zeros((150, 150))
Z2[30:45, 30:45] = 10 * (bivariate_normal(X, Y, 1.0, 1.0, 0, 0) - bivariate_normal(X, Y, 1.5, 0.5, 1, 1))
x = -3 + (np.arange(150) + 0.5) * 7 / 150
y = -4 + (np.arange(150) + 0.5) * 7 / 150
z = Z2.T.ravel()  # column after column

look = dict(gradient=QCPColorGradient.gpViridis, colorBar=False)
ax = plot.axisRect()
ax.colorMap(x, y, z, **look)
ax.xAxis().setRange(-3, 4)
ax.yAxis().setRange(-4, 3)

inset = ax.addInset(QRectF(0.5, 0.03, 0.47, 0.47))
inset.colorMap(x, y, z, **look)
inset.xAxis().setRange(-1.5, -0.9)
inset.yAxis().setRange(-2.5, -1.9)
inset.xAxis().setTickLabels(False)
inset.yAxis().setTickLabels(False)

ax.zoomIndicator(inset, pen=QPen(Qt.black))

plot.setInteractions(QCP.iRangeDrag | QCP.iRangeZoom)
plot.show()
app.exec()
