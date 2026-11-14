# Target sketch (milestone 6): peak-fit/main.cpp as PySide code would read. Informative only.
# The fit is numpy here; in C++ it is a plain function. Only the NeoQCP side matters.
import sys

import numpy as np
from PySide6.QtGui import QColor, QPen
from PySide6.QtCore import Qt
from PySide6.QtWidgets import QApplication
from NeoQCP import QCP, QCPItem, QCustomPlot


def basis(x, peaks):
    return np.column_stack([np.ones_like(x)] +
                           [np.exp(-0.5 * ((x - c) / w) ** 2) for c, w in peaks])


def fit_amplitudes(x, y, peaks):
    a = basis(x, peaks)
    model = a @ np.linalg.lstsq(a, y, rcond=None)[0]
    return model, y - model


BODY, LEFT_EDGE, RIGHT_EDGE = range(3)


class Peak(QCPItem):
    """One peak guess: a band across the panel, dragged by its body, resized from either edge."""

    def __init__(self, centre, width):
        super().__init__()
        self.centre, self.width = centre, width

    def draw(self, context):
        left, right = self.centre - self.width, self.centre + self.width
        red = QColor(214, 39, 40)
        edge = QPen(red, 2)
        context.setPart(BODY)
        context.rect(QCP.DataXRatioY(left, 0), QCP.DataXRatioY(right, 1), Qt.NoPen, QColor(214, 39, 40, 40))
        context.line(QCP.DataXRatioY(self.centre, 0), QCP.DataXRatioY(self.centre, 1),
                     QPen(red, 1, Qt.DashLine))
        context.setPart(LEFT_EDGE)
        context.line(QCP.DataXRatioY(left, 0), QCP.DataXRatioY(left, 1), edge)
        context.setPart(RIGHT_EDGE)
        context.line(QCP.DataXRatioY(right, 0), QCP.DataXRatioY(right, 1), edge)

    def mousePressEvent(self, event):
        event.accept()

    def mouseMoveEvent(self, event):
        dx = event.data().x - event.lastData().x
        if event.part() == BODY:
            self.centre += dx
        elif event.part() == LEFT_EDGE:
            self.width -= dx
        else:
            self.width += dx
        self.width = max(self.width, 0.3)
        self.update()


app = QApplication(sys.argv)
plot = QCustomPlot()
plot.resize(800, 560)

rng = np.random.default_rng(7)
energy = np.arange(401) * 0.25
counts = 12 + rng.normal(0, 6, energy.size) + basis(energy, [(25, 2.5), (48, 4), (70, 3)])[:, 1:] @ [120, 80, 150]

spectrum = plot.axisRect()
residuals = plot.addAxisRect(shareX=spectrum, stretch=0.35)
spectrum.yAxis().setLabel("counts")
residuals.yAxis().setLabel("residual")
residuals.xAxis().setLabel("energy (keV)")

peaks = [spectrum.add(Peak, centre, 4.0) for centre in (22.0, 52.0, 67.0)]


def guesses():
    return [(p.centre, p.width) for p in peaks]


model0, residual0 = fit_amplitudes(energy, counts, guesses())
spectrum.line(energy, counts, pen=QPen(Qt.gray), name="spectrum")
model = spectrum.line(energy, model0, pen=QPen(Qt.black, 2), name="fit")
misfit = residuals.line(energy, residual0, pen=QPen(Qt.gray), name="residual")


def refit():
    m, r = fit_amplitudes(energy, counts, guesses())
    model.setData(energy, m)
    misfit.setData(energy, r)


for peak in peaks:
    peak.changed.connect(refit)

plot.legend.setVisible(True)
plot.rescaleAxes()
plot.show()
app.exec()
