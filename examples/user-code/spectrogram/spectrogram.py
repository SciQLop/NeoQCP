# Target sketch (milestone 6): spectrogram/main.cpp as PySide code would read. Informative only.
# The spectrogram is matplotlib.mlab.specgram here; in C++ it is a plain function. Only the NeoQCP side matters.
import sys

import numpy as np
from matplotlib import mlab
from PySide6.QtGui import QColor, QPen
from PySide6.QtWidgets import QApplication
from NeoQCP import QCP, QCPColorGradient, QCPScale, QCustomPlot

app = QApplication(sys.argv)
plot = QCustomPlot()
plot.resize(720, 560)

np.random.seed(19680801)
dt = 0.0005
t = np.arange(0.0, 20.5, dt)
s1 = np.sin(2 * np.pi * 100 * t)
s2 = 2 * np.sin(2 * np.pi * 400 * t)
s2[t <= 10] = s2[12 <= t] = 0
x = s1 + s2 + 0.01 * np.random.random(size=len(t))
power, frequency, time = mlab.specgram(x, NFFT=1024, Fs=1 / dt, noverlap=128)

spectrum = plot.axisRect()
signal = plot.addAxisRect(shareX=spectrum)

spectrum.colorMap(time, frequency[1:], power[1:].T.ravel(),  # no 0 Hz row on a log axis
                  gradient=QCPColorGradient.gpViridis, dataScale=QCPScale.log())
spectrum.yAxis().setScale(QCPScale.log())
spectrum.yAxis().setLabel("Frequency (Hz)")
spectrum.colorScale().setLabel("power (1/Hz)")

signal.line(t, x, pen=QPen(QColor(31, 119, 180)))
signal.yAxis().setLabel("Signal")
signal.xAxis().setLabel("Time (s)")

plot.setInteractions(QCP.iRangeDrag | QCP.iRangeZoom)
plot.rescaleAxes()
signal.xAxis().setRange(0, 20)
plot.show()
app.exec()
