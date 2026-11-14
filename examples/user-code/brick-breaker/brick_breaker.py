# Target sketch (milestone 6): brick-breaker/main.cpp as PySide code would read. Informative only.
import math
import sys

from PySide6.QtCore import QPointF, QRectF, QSizeF, Qt, QTimer
from PySide6.QtGui import QColor, QPen
from PySide6.QtWidgets import QApplication
from NeoQCP import QCP, QCPItem, QCustomPlot



# The rules: plain data and arithmetic, no NeoQCP type. The world is 100 x 60 data units.
class Breakout:
    def __init__(self):
        self.ball, self.velocity, self.radius = QPointF(50, 6), QPointF(), 1.0
        self.lives, self.score, self.waiting = 3, 0, True

    def ballRect(self):
        r = self.radius
        return QRectF(self.ball - QPointF(r, r), QSizeF(2 * r, 2 * r))

    def launch(self):
        if self.waiting and self.lives > 0:
            self.velocity, self.waiting = QPointF(18, 40), False

    def step(self, dt, paddle_centre):
        if self.waiting:
            self.ball = QPointF(paddle_centre, 6)
            return
        self.ball += dt * self.velocity
        if not self.radius <= self.ball.x() <= 100 - self.radius:
            self.velocity.setX(-self.velocity.x())
        if self.ball.y() > 60 - self.radius:
            self.velocity.setY(-abs(self.velocity.y()))
        if self.ball.y() < 0:
            self.lives -= 1
            self.waiting = True

    def bounceOff(self, box):
        overlap = self.ballRect().intersected(box)
        away = self.ball - box.center()
        if overlap.width() < overlap.height():
            self.velocity.setX(math.copysign(self.velocity.x(), away.x()))
        else:
            self.velocity.setY(math.copysign(self.velocity.y(), away.y()))

    def bounceOffPaddle(self, paddle):
        self.velocity = QPointF(4 * (self.ball.x() - paddle.center().x()), abs(self.velocity.y()))


class Brick(QCPItem):
    def __init__(self, box, color):
        super().__init__()
        self._box, self._color = box, color

    def draw(self, context):
        context.rect(self._box, QPen(Qt.white), self._color)


class Ball(QCPItem):
    def __init__(self, radius):
        super().__init__()
        self._radius = radius
        self._centre = None

    def setCentre(self, centre):
        self._centre = centre
        self.update()

    def draw(self, context):
        if self._centre is not None:
            context.circle(QCP.Data(self._centre.x(), self._centre.y()), self._radius, Qt.NoPen, Qt.white)


class Paddle(QCPItem):
    """The user drags it along x; the game reads where it is from its recorded geometry."""

    def __init__(self):
        super().__init__()
        self._centre = 50.0

    def centre(self):
        return self._centre

    def draw(self, context):
        context.rect(QRectF(self._centre - 7, 2, 14, 1.5), Qt.NoPen, QColor(90, 200, 255))

    def mousePressEvent(self, event):
        event.accept()

    def mouseMoveEvent(self, event):
        self._centre = min(max(event.data().x, 7.0), 93.0)
        self.update()


app = QApplication(sys.argv)
plot = QCustomPlot()
plot.resize(800, 520)
plot.setBackground(QColor(20, 20, 30))

game = Breakout()
ax = plot.axisRect()
ax.xAxis().setRange(0, 100)
ax.yAxis().setRange(0, 60)
ax.xAxis().setVisible(False)
ax.yAxis().setVisible(False)
bricks = [ax.add(Brick, QRectF(column * 100 / 12, 36 + row * 3, 100 / 12, 3),
                 QColor.fromHsv(row * 50, 160, 230))
          for row in range(6) for column in range(12)]
paddle = ax.add(Paddle)
ball = ax.add(Ball, game.radius)

plot.keyPress.connect(lambda e: game.launch() if e.key() == Qt.Key_Space else None)


def frame():
    global bricks
    game.step(1 / 60, paddle.centre())
    if game.ballRect().intersects(paddle.dataRect()):
        game.bounceOffPaddle(paddle.dataRect())
    kept = []
    for brick in bricks:
        if brick.dataRect().intersects(game.ballRect()):
            game.bounceOff(brick.dataRect())
            game.score += 10
            brick.remove()  # the other bricks keep their recordings
        else:
            kept.append(brick)
    bricks = kept
    ball.setCentre(game.ball)
    plot.setWindowTitle(f"score {game.score}   lives {game.lives}")


tick = QTimer()
tick.timeout.connect(frame)
tick.start(16)

plot.show()
app.exec()
