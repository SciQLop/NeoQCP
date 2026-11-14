# Target sketch (milestone 6): asteroids.cpp as PySide code would read. Informative only.
import math
import random
import sys
from dataclasses import dataclass, field

from PySide6.QtCore import QPointF, Qt, QTimer
from PySide6.QtGui import QColor, QFont, QPen, QPolygonF, QTransform
from PySide6.QtWidgets import QApplication
from NeoQCP import QCP, QCPItem, QCPTextStyle, QCustomPlot



# The rules: plain data and arithmetic, no NeoQCP type.
@dataclass
class Body:
    pos: QPointF
    vel: QPointF = field(default_factory=QPointF)
    angle: float = 0.0  # degrees, counter-clockwise from +x
    size: float = 1.0
    life: float = 0.0  # seconds left, bullets only


def heading(degrees):
    return QPointF(math.cos(math.radians(degrees)), math.sin(math.radians(degrees)))


def distance(a, b):
    return math.hypot(a.x() - b.x(), a.y() - b.y())


class Game:
    width, height = 100.0, 75.0

    def __init__(self):
        self.held = set()
        self.restart()

    def restart(self):
        rng = random.Random(3)
        self.ship = Body(QPointF(self.width / 2, self.height / 2), angle=90, size=2)
        self.rocks = [Body(QPointF(rng.random() * self.width, 0),
                           QPointF(12 * rng.random() - 6, 12 * rng.random() - 6), 360 * rng.random(), 6)
                      for _ in range(5)]
        self.bullets, self.score, self.over = [], 0, False

    def press(self, key):
        self.held.add(key)
        if key == Qt.Key_Space and not self.over:
            s = self.ship
            self.bullets.append(Body(s.pos, s.vel + 60 * heading(s.angle), s.angle, 0.4, 1.2))
        if key == Qt.Key_R and self.over:
            self.restart()

    def release(self, key):
        self.held.discard(key)

    def step(self, dt):
        if self.over:
            return
        s = self.ship
        s.angle += 200 * dt * ((Qt.Key_Left in self.held) - (Qt.Key_Right in self.held))
        if Qt.Key_Up in self.held:
            s.vel += 30 * dt * heading(s.angle)
        for b in [s, *self.rocks, *self.bullets]:
            b.pos = QPointF((b.pos.x() + dt * b.vel.x()) % self.width, (b.pos.y() + dt * b.vel.y()) % self.height)
            b.life -= dt
        self.bullets = [b for b in self.bullets if b.life > 0]
        self.collide()

    def collide(self):
        survivors = []
        for rock in self.rocks:
            hit = next((b for b in self.bullets if b.life > 0 and distance(b.pos, rock.pos) < rock.size), None)
            if hit is None:
                survivors.append(rock)
                continue
            hit.life = 0
            self.score += 10
            if rock.size > 2:
                survivors += [Body(rock.pos, QTransform().rotate(t).map(rock.vel) * 1.5, rock.angle + t, rock.size / 2)
                              for t in (90.0, -90.0)]
        self.rocks = survivors
        s = self.ship
        self.over = any(distance(r.pos, s.pos) < r.size + s.size * 0.6 for r in self.rocks)


class Sprites(QCPItem):
    """One annotation draws every body of one kind: its outline, rotated and moved, in data units."""

    def __init__(self, outline, color):
        super().__init__()
        self._outline = outline
        self._pen = QPen(color, 1.5)
        self._bodies = []

    def setBodies(self, bodies):
        self._bodies = list(bodies)
        self.update()

    def draw(self, context):
        for b in self._bodies:
            pose = QTransform().translate(b.pos.x(), b.pos.y()).rotate(b.angle).scale(b.size, b.size)
            context.polygon(pose.map(self._outline), self._pen)


class Hud(QCPItem):
    """The score, and the game-over banner: the text is recorded after the box, so it is drawn over it."""

    def __init__(self):
        super().__init__()
        self._score, self._over = 0, False

    def setScore(self, score):
        self._score = score
        self.update()

    def setGameOver(self, over):
        self._over = over
        self.update()

    def draw(self, context):
        font = QFont("monospace", 16, QFont.Bold)
        context.text(QCP.AxisRatio(0.02, 0.02), f"SCORE {self._score}",
                     QCPTextStyle(font=font, color=Qt.white))
        if not self._over:
            return
        context.rect(QCP.AxisRatio(0.2, 0.4), QCP.AxisRatio(0.8, 0.6), QPen(Qt.white),
                     QColor(40, 0, 0, 220))
        context.text(QCP.AxisRatio(0.5, 0.5), "GAME OVER - press R",
                     QCPTextStyle(font=font, color=Qt.white, alignment=Qt.AlignCenter))


app = QApplication(sys.argv)
plot = QCustomPlot()
plot.resize(800, 600)
plot.setBackground(Qt.black)

ship_outline = QPolygonF([QPointF(1, 0), QPointF(-0.7, 0.6), QPointF(-0.4, 0), QPointF(-0.7, -0.6)])
rock_outline = QPolygonF([QPointF(x, y) for x, y in
                          [(1, 0), (0.6, 0.7), (0, 0.9), (-0.7, 0.8),
                           (-1, 0.1), (-0.6, -0.8), (0.1, -1), (0.8, -0.6)]])
bullet_outline = QPolygonF([QPointF(1, 0), QPointF(0, 1), QPointF(-1, 0), QPointF(0, -1)])

ax = plot.axisRect()
ax.xAxis().setRange(0, Game.width)
ax.yAxis().setRange(0, Game.height)
ax.xAxis().setVisible(False)
ax.yAxis().setVisible(False)
rocks = ax.add(Sprites, rock_outline, QColor(200, 200, 200))
bullets = ax.add(Sprites, bullet_outline, QColor(255, 220, 90))
ship = ax.add(Sprites, ship_outline, QColor(120, 220, 255))
hud = ax.add(Hud)  # added last: drawn over every sprite, text included

game = Game()
plot.keyPress.connect(lambda e: None if e.isAutoRepeat() else game.press(e.key()))
plot.keyRelease.connect(lambda e: None if e.isAutoRepeat() else game.release(e.key()))


def frame():
    game.step(1 / 60)
    rocks.setBodies(game.rocks)
    bullets.setBodies(game.bullets)
    ship.setBodies([game.ship])
    hud.setScore(game.score)
    hud.setGameOver(game.over)


# A plain timer is the frame clock: the rules step at a fixed dt, and the setters coalesce into one
# repaint per event-loop turn, so a vsync hook would add nothing here.
tick = QTimer()
tick.timeout.connect(frame)
tick.start(16)

plot.show()
app.exec()
