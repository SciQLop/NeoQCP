# Target sketch (milestone 6): callout.cpp as PySide code would read. Informative only.
import math
import sys

from PySide6.QtCore import QMarginsF, QPointF, QRectF, QSizeF, Qt
from PySide6.QtGui import QColor, QPen
from PySide6.QtWidgets import QApplication
from NeoQCP import QCP, QCPItem, QCPItemEllipse, QCPItemText, QCPTextStyle, QCustomPlot

POINTER, BOX, CORNER = range(3)


def offset(p):
    return QCP.PixelOffset(p.x(), p.y())


class Callout(QCPItem):
    def __init__(self, tip, text):
        super().__init__()
        self._tip = tip
        self._text = text
        self._box = QRectF(30, -110, 170, 60)  # pixels from the tip; y grows downwards
        self.tip = self.createAnchor("tip", lambda: self._tip)
        self.box = self.createAnchor("box", lambda: self._tip + offset(self._box.center()))

    def draw(self, context):
        top_left = self._tip + offset(self._box.topLeft())
        bottom_right = self._tip + offset(self._box.bottomRight())
        outline = QPen(QColor(60, 60, 60), 1.2)
        paper = QColor(255, 250, 225)
        centre = self._box.center()

        context.setPart(POINTER)
        context.polygon([self._tip, self._tip + offset(centre + QPointF(-12, 0)),
                         self._tip + offset(centre + QPointF(12, 0))], outline, paper)
        context.setPart(BOX)
        context.roundedRect(top_left, bottom_right, QCP.PixelLength(6), outline, paper)
        context.text(top_left, bottom_right, self._text, QCPTextStyle(padding=QMarginsF(8, 6, 8, 6)))
        context.setPart(CORNER)
        context.rect(bottom_right + QCP.PixelOffset(-8, -8), bottom_right, Qt.NoPen, QColor(60, 60, 60))

    def mousePressEvent(self, event):
        if event.part() == POINTER:
            event.ignore()  # a press on the pointer pans the plot

    def mouseMoveEvent(self, event):
        moved = event.pixels() - event.lastPixels()
        delta = QPointF(moved.dx, moved.dy)
        if event.part() == BOX:
            self._box.translate(delta)
        else:
            self._box.setBottomRight(self._box.bottomRight() + delta)
        self._box.setSize(self._box.size().expandedTo(QSizeF(80, 36)))
        self.update()


app = QApplication(sys.argv)
plot = QCustomPlot()
plot.resize(720, 460)

t = [i / 40 for i in range(401)]
signal = [math.sin(v) * math.exp(-v / 8) for v in t]
ax = plot.axisRect()
ax.line(t, signal, pen=QPen(QColor(31, 119, 180), 2), name="damped sine")

first_max = math.pi / 2
peak = ax.add(Callout, QCP.Data(first_max, math.exp(-first_max / 8)),
              "First maximum.\nDrag the box; resize it from its corner.")

ring = ax.add(QCPItemEllipse)
ring.topLeft.setParentAnchor(peak.tip)
ring.topLeft.setCoords(-7, -7)
ring.bottomRight.setParentAnchor(peak.tip)
ring.bottomRight.setCoords(7, 7)

note = ax.add(QCPItemText)
note.position.setParentAnchor(peak.box)
note.position.setCoords(0, 50)
note.setText("hangs below the box")

plot.setInteractions(QCP.iRangeDrag | QCP.iRangeZoom)
plot.rescaleAxes()
plot.show()
app.exec()
