// Target program (milestone 6): Asteroids, annotations the user derives. Does not compile yet.

#include <qcustomplot.h>

#include <QApplication>
#include <QKeyEvent>
#include <QTimer>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <set>
#include <vector>

// The rules: plain data and arithmetic, no NeoQCP type.
struct Body
{
    QPointF pos;
    QPointF vel;
    double angle = 0; // degrees, counter-clockwise from +x
    double size = 1;
    double life = 0; // seconds left, bullets only
};

struct Game
{
    static constexpr double width = 100, height = 75;
    Body ship;
    std::vector<Body> rocks, bullets;
    std::set<int> held; // Qt::Key values
    int score = 0;
    bool over = false;

    Game() { restart(); }

    void restart()
    {
        ship = Body { { width / 2, height / 2 }, { }, 90, 2 };
        rocks.clear();
        bullets.clear();
        score = 0;
        over = false;
        std::mt19937 rng(3);
        std::uniform_real_distribution<double> unit(0, 1);
        for (int i = 0; i < 5; ++i)
            rocks.push_back({ { unit(rng) * width, 0 }, { 12 * unit(rng) - 6, 12 * unit(rng) - 6 },
                              360 * unit(rng), 6 });
    }

    void press(int key)
    {
        held.insert(key);
        if (key == Qt::Key_Space && !over)
            fire();
        if (key == Qt::Key_R && over)
            restart();
    }

    void release(int key) { held.erase(key); }

    void step(double dt)
    {
        if (over)
            return;
        steer(dt);
        move(ship, dt);
        for (Body& rock : rocks)
            move(rock, dt);
        for (Body& bullet : bullets)
            move(bullet, dt);
        std::erase_if(bullets, [](const Body& b) { return b.life <= 0; });
        collide();
    }

private:
    static QPointF heading(double degrees)
    {
        const double rad = degrees * std::numbers::pi / 180;
        return { std::cos(rad), std::sin(rad) };
    }

    static QPointF wrap(QPointF p)
    {
        return { std::fmod(p.x() + width, width), std::fmod(p.y() + height, height) };
    }

    static double distance(QPointF a, QPointF b) { return std::hypot(a.x() - b.x(), a.y() - b.y()); }

    void steer(double dt)
    {
        if (held.contains(Qt::Key_Left))
            ship.angle += 200 * dt;
        if (held.contains(Qt::Key_Right))
            ship.angle -= 200 * dt;
        if (held.contains(Qt::Key_Up))
            ship.vel += 30 * dt * heading(ship.angle);
    }

    static void move(Body& body, double dt)
    {
        body.pos = wrap(body.pos + dt * body.vel);
        body.life -= dt;
    }

    void fire()
    {
        bullets.push_back({ ship.pos, ship.vel + 60 * heading(ship.angle), ship.angle, 0.4, 1.2 });
    }

    void collide()
    {
        std::vector<Body> fragments;
        std::erase_if(rocks, [&](const Body& rock) {
            const auto hit = std::ranges::find_if(bullets, [&](const Body& b) {
                return b.life > 0 && distance(b.pos, rock.pos) < rock.size;
            });
            if (hit == bullets.end())
                return false;
            hit->life = 0;
            score += 10;
            if (rock.size > 2)
                for (double turn : { 90.0, -90.0 })
                    fragments.push_back({ rock.pos, QTransform().rotate(turn).map(rock.vel) * 1.5,
                                          rock.angle + turn, rock.size / 2 });
            return true;
        });
        rocks.insert(rocks.end(), fragments.begin(), fragments.end());
        over = std::ranges::any_of(rocks, [&](const Body& r) {
            return distance(r.pos, ship.pos) < r.size + ship.size * 0.6;
        });
    }
};

// One annotation draws every body of one kind: its outline, rotated and moved, in data units.
class Sprites : public QCPItem
{
public:
    Sprites(QPolygonF outline, QColor color) : mOutline(std::move(outline)), mPen(color, 1.5) { }

    void setBodies(std::vector<Body> bodies)
    {
        mBodies = std::move(bodies);
        update();
    }

protected:
    void draw(QCPDrawContext& context) const override
    {
        for (const Body& b : mBodies)
        {
            const QTransform pose = QTransform()
                                        .translate(b.pos.x(), b.pos.y())
                                        .rotate(b.angle)
                                        .scale(b.size, b.size);
            context.polygon(pose.map(mOutline), mPen);
        }
    }

private:
    QPolygonF mOutline;
    QPen mPen;
    std::vector<Body> mBodies;
};

// The score, and the game-over banner: the text is recorded after the box, so it is drawn over it.
class Hud : public QCPItem
{
public:
    void setScore(int score)
    {
        mScore = score;
        update();
    }

    void setGameOver(bool over)
    {
        mOver = over;
        update();
    }

protected:
    void draw(QCPDrawContext& context) const override
    {
        const QFont font("monospace", 16, QFont::Bold);
        context.text(QCP::AxisRatio { 0.02, 0.02 }, QString("SCORE %1").arg(mScore),
                     { .font = font, .color = Qt::white });
        if (!mOver)
            return;
        context.rect(QCP::AxisRatio { 0.2, 0.4 }, QCP::AxisRatio { 0.8, 0.6 }, QPen(Qt::white),
                     QColor(40, 0, 0, 220));
        context.text(QCP::AxisRatio { 0.5, 0.5 }, "GAME OVER - press R",
                     { .font = font, .color = Qt::white, .alignment = Qt::AlignCenter });
    }

private:
    int mScore = 0;
    bool mOver = false;
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCustomPlot plot;
    plot.resize(800, 600);
    plot.setBackground(Qt::black);

    const QPolygonF shipOutline { { 1, 0 }, { -0.7, 0.6 }, { -0.4, 0 }, { -0.7, -0.6 } };
    const QPolygonF rockOutline { { 1, 0 },   { 0.6, 0.7 },  { 0, 0.9 },   { -0.7, 0.8 },
                                  { -1, 0.1 }, { -0.6, -0.8 }, { 0.1, -1 }, { 0.8, -0.6 } };
    const QPolygonF bulletOutline { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } };

    auto ax = plot.axisRect();
    ax->xAxis()->setRange(0, Game::width);
    ax->yAxis()->setRange(0, Game::height);
    ax->xAxis()->setVisible(false);
    ax->yAxis()->setVisible(false);
    auto rocks = ax->add<Sprites>(rockOutline, QColor(200, 200, 200));
    auto bullets = ax->add<Sprites>(bulletOutline, QColor(255, 220, 90));
    auto ship = ax->add<Sprites>(shipOutline, QColor(120, 220, 255));
    auto hud = ax->add<Hud>(); // added last: drawn over every sprite, text included

    Game game;
    QObject::connect(&plot, &QCustomPlot::keyPress, [&](QKeyEvent* event) {
        if (!event->isAutoRepeat())
            game.press(event->key());
    });
    QObject::connect(&plot, &QCustomPlot::keyRelease, [&](QKeyEvent* event) {
        if (!event->isAutoRepeat())
            game.release(event->key());
    });

    // A plain timer is the frame clock: the rules step at a fixed dt, and the setters below
    // coalesce into one repaint per event-loop turn, so a vsync hook would add nothing here.
    QTimer tick;
    QObject::connect(&tick, &QTimer::timeout, [&] {
        game.step(1.0 / 60);
        rocks->setBodies(game.rocks);
        bullets->setBodies(game.bullets);
        ship->setBodies({ game.ship });
        hud->setScore(game.score);
        hud->setGameOver(game.over);
    });
    tick.start(16);

    plot.show();
    return app.exec();
}
