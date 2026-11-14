// Target program (milestone 6): a brick breaker, collisions in data units. Does not compile yet.

#include <qcustomplot.h>

#include <QApplication>
#include <QKeyEvent>
#include <QTimer>

#include <algorithm>
#include <cmath>

// The rules: plain data and arithmetic, no NeoQCP type. The world is 100 x 60 data units.
struct Breakout
{
    QPointF ball { 50, 6 };
    QPointF velocity;
    double radius = 1.0;
    int lives = 3;
    int score = 0;
    bool waiting = true; // the ball rests on the paddle until space

    QRectF ballRect() const
    {
        return { ball - QPointF(radius, radius), QSizeF(2 * radius, 2 * radius) };
    }

    void launch()
    {
        if (!waiting || lives == 0)
            return;
        velocity = { 18, 40 };
        waiting = false;
    }

    void step(double dt, double paddleCentre)
    {
        if (waiting)
        {
            ball = { paddleCentre, 6 };
            return;
        }
        ball += dt * velocity;
        if (ball.x() < radius || ball.x() > 100 - radius)
            velocity.setX(-velocity.x());
        if (ball.y() > 60 - radius)
            velocity.setY(-std::abs(velocity.y()));
        if (ball.y() < 0)
        {
            --lives;
            waiting = true;
        }
    }

    // Reflects along the axis of the smaller overlap: the side of \a box the ball came through.
    void bounceOff(const QRectF& box)
    {
        const QRectF overlap = ballRect().intersected(box);
        const QPointF away = ball - box.center();
        if (overlap.width() < overlap.height())
            velocity.setX(std::copysign(velocity.x(), away.x()));
        else
            velocity.setY(std::copysign(velocity.y(), away.y()));
    }

    // The paddle sends the ball up, steeper near its middle.
    void bounceOffPaddle(const QRectF& paddle)
    {
        velocity = { 4 * (ball.x() - paddle.center().x()), std::abs(velocity.y()) };
    }
};

class Brick : public QCPItem
{
public:
    Brick(QRectF box, QColor color) : mBox(box), mColor(color) { }

protected:
    void draw(QCPDrawContext& context) const override
    {
        context.rect(mBox, QPen(Qt::white), mColor);
    }

private:
    QRectF mBox;
    QColor mColor;
};

class Ball : public QCPItem
{
public:
    explicit Ball(double radius) : mRadius(radius) { }

    void setCentre(QPointF centre)
    {
        mCentre = centre;
        update();
    }

protected:
    void draw(QCPDrawContext& context) const override
    {
        context.circle(QCP::Data { mCentre.x(), mCentre.y() }, mRadius, Qt::NoPen, Qt::white);
    }

private:
    double mRadius;
    QPointF mCentre;
};

// The user drags it along x; the game reads where it is from its recorded geometry.
class Paddle : public QCPItem
{
public:
    double centre() const { return mCentre; }

protected:
    void draw(QCPDrawContext& context) const override
    {
        context.rect(QRectF(mCentre - 7, 2, 14, 1.5), Qt::NoPen, QColor(90, 200, 255));
    }

    void mousePressEvent(QCPMouseEvent* event) override { event->accept(); }

    void mouseMoveEvent(QCPMouseEvent* event) override
    {
        mCentre = std::clamp(event->data().x, 7.0, 93.0);
        update();
    }

private:
    double mCentre = 50;
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCustomPlot plot;
    plot.resize(800, 520);
    plot.setBackground(QColor(20, 20, 30));
    plot.xAxis->setRange(0, 100);
    plot.yAxis->setRange(0, 60);
    plot.xAxis->setVisible(false);
    plot.yAxis->setVisible(false);

    Breakout game;
    auto ax = plot.axisRect();
    QList<QCPHandle<Brick>> bricks;
    for (int row = 0; row < 6; ++row)
        for (int column = 0; column < 12; ++column)
            bricks << ax->add<Brick>(QRectF(column * 100.0 / 12, 36 + row * 3, 100.0 / 12, 3),
                                     QColor::fromHsv(row * 50, 160, 230));
    auto paddle = ax->add<Paddle>();
    auto ball = ax->add<Ball>(game.radius);

    QObject::connect(&plot, &QCustomPlot::keyPress, [&](QKeyEvent* event) {
        if (event->key() == Qt::Key_Space)
            game.launch();
    });

    QTimer tick;
    QObject::connect(&tick, &QTimer::timeout, [&] {
        game.step(1.0 / 60, paddle->centre());
        if (game.ballRect().intersects(paddle->dataRect()))
            game.bounceOffPaddle(paddle->dataRect());
        bricks.removeIf([&](QCPHandle<Brick>& brick) {
            if (!brick->dataRect().intersects(game.ballRect()))
                return false;
            game.bounceOff(brick->dataRect());
            game.score += 10;
            brick.remove(); // the other bricks keep their recordings
            return true;
        });
        ball->setCentre(game.ball);
        plot.setWindowTitle(QString("score %1   lives %2").arg(game.score).arg(game.lives));
    });
    tick.start(16);

    plot.show();
    return app.exec();
}
