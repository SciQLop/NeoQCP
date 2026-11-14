# Target sketch (milestone 6): annotations/main.cpp as PySide code would read. Informative only.
import sys

from PySide6.QtCore import QMarginsF, Qt
from PySide6.QtGui import QColor, QPen
from PySide6.QtWidgets import QApplication
from NeoQCP import QCP, QCPConnection, QCPLineEnding, QCPScatterStyle, QCPTextStyle, QCustomPlot
from NeoQCP.QCP import px

point, label = QCP.Data(0.3, 0.2), QCP.Data(0.8, 0.6)
blue, grey = QColor(31, 119, 180), QColor(128, 128, 128)


def show_panel(ax):
    ax.line([point.x, label.x], [point.y, label.y],
            pen=Qt.NoPen, scatterStyle=QCPScatterStyle(QCPScatterStyle.ssDisc, blue, 5))
    ax.xAxis().setRange(0, 1)
    ax.yAxis().setRange(0, 1.25)
    for axis in ax.axes():
        axis.setVisible(True)
        axis.setTicks(False)
        axis.grid().setVisible(False)


def demo_connection(ax, connection, name):
    show_panel(ax)
    ax.annotation(point, label, "", connection=connection, pen=QPen(grey),
                  head=QCPLineEnding.esLineArrow, shrink=px(5))
    ax.text(QCP.AxisRatio(0.05, 0.05), name)


app = QApplication(sys.argv)
plot = QCustomPlot()
plot.resize(700, 630)
plot.setAxisRectGrid(3, 5)

demos = [
    (0, 0, QCPConnection.curve(90, 0), "curve(90, 0)"),
    (1, 0, QCPConnection.curve(0, 90), "curve(0, 90)"),
    (0, 1, QCPConnection.arc(0), "arc(0)"),
    (1, 1, QCPConnection.arc(0.3), "arc(0.3)"),
    (2, 1, QCPConnection.arc(-0.3), "arc(-0.3)"),
    (0, 2, QCPConnection.elbow(-90, 180), "elbow(-90, 180)"),
    (1, 2, QCPConnection.elbow(-90, 180, px(5)), "elbow(-90, 180,\n5 px)"),
    (2, 2, QCPConnection.elbow(-90, 10, px(5)), "elbow(-90, 10,\n5 px)"),
    (0, 3, QCPConnection.arms(-90, px(30), 0, px(30)), "arms(-90, 30 px,\n0, 30 px)"),
    (1, 3, QCPConnection.arms(-90, px(30), 0, px(30), px(5)), "arms(-90, 30 px,\n0, 30 px, 5 px)"),
    (2, 3, QCPConnection.arms(-90, px(0), 0, px(40)), "arms(-90, 0 px,\n0, 40 px)"),
    (0, 4, QCPConnection.bracket(0.3), "bracket(0.3)"),
    (1, 4, QCPConnection.bracket(-0.3), "bracket(-0.3)"),
    (2, 4, QCPConnection.bracket(-0.2, 180), "bracket(-0.2, 180)"),
]
for row, column, connection, name in demos:
    demo_connection(plot.axisRect(row, column), connection, name)

# matplotlib leaves this cell empty; it shows the text and its arrow as one annotation.
spare = plot.axisRect(2, 0)
show_panel(spare)
spare.annotation(point, label, "text and\narrow", connection=QCPConnection.elbow(-90, 180, px(5)),
                 pen=QPen(grey), head=QCPLineEnding.esLineArrow, shrink=px(5),
                 textStyle=QCPTextStyle(pen=QPen(grey), brush=Qt.white, padding=QMarginsF(4, 2, 4, 2)))

plot.show()
app.exec()
