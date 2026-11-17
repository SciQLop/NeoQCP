/***************************************************************************
**                                                                        **
**  QCustomPlot, an easy to use, modern plotting widget for Qt            **
**  Copyright (C) 2011-2022 Emanuel Eichhammer                            **
**                                                                        **
**  This program is free software: you can redistribute it and/or modify  **
**  it under the terms of the GNU General Public License as published by  **
**  the Free Software Foundation, either version 3 of the License, or     **
**  (at your option) any later version.                                   **
**                                                                        **
**  This program is distributed in the hope that it will be useful,       **
**  but WITHOUT ANY WARRANTY; without even the implied warranty of        **
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         **
**  GNU General Public License for more details.                          **
**                                                                        **
**  You should have received a copy of the GNU General Public License     **
**  along with this program.  If not, see http://www.gnu.org/licenses/.   **
**                                                                        **
****************************************************************************
**           Author: Emanuel Eichhammer                                   **
**  Website/Contact: https://www.qcustomplot.com/                         **
**             Date: 06.11.22                                             **
**          Version: 2.1.1                                                **
****************************************************************************/

#ifndef QCPVECTOR2D_H
#define QCPVECTOR2D_H

#include "global.h"
#include <algorithm>
#include <cmath>
#include <limits>

class QCP_LIB_DECL QCPVector2D
{
public:
    constexpr QCPVector2D() : mX(0), mY(0) {}
    constexpr QCPVector2D(double x, double y) : mX(x), mY(y) {}
    QCPVector2D(const QPoint& point);
    QCPVector2D(const QPointF& point);

    // getters:
    [[nodiscard]] constexpr double x() const { return mX; }

    [[nodiscard]] constexpr double y() const { return mY; }

    constexpr double& rx() { return mX; }

    constexpr double& ry() { return mY; }

    // setters:
    constexpr void setX(double x) { mX = x; }

    constexpr void setY(double y) { mY = y; }

    // non-virtual methods:
    [[nodiscard]] double length() const { return qSqrt(mX * mX + mY * mY); }

    [[nodiscard]] constexpr double lengthSquared() const { return mX * mX + mY * mY; }

    [[nodiscard]] double angle() const { return qAtan2(mY, mX); }

    //! This vector as a pixel. A position far off the screen is past any integer, so it is clamped
    //! into the range rather than cast into it, which is undefined.
    [[nodiscard]] QPoint toPoint() const
    {
        const auto pixel = [](double value)
        {
            if (std::isnan(value))
                return 0;
            return int(std::clamp(value, double(std::numeric_limits<int>::min()),
                                  double(std::numeric_limits<int>::max())));
        };
        return QPoint(pixel(mX), pixel(mY));
    }

    [[nodiscard]] QPointF toPointF() const { return QPointF(mX, mY); }

    [[nodiscard]] bool isNull() const { return qIsNull(mX) && qIsNull(mY); }

    void normalize();
    [[nodiscard]] QCPVector2D normalized() const;

    [[nodiscard]] constexpr QCPVector2D perpendicular() const { return QCPVector2D(-mY, mX); }

    [[nodiscard]] constexpr double dot(const QCPVector2D& vec) const { return mX * vec.mX + mY * vec.mY; }

    [[nodiscard]] double distanceSquaredToLine(const QCPVector2D& start, const QCPVector2D& end) const;
    [[nodiscard]] double distanceSquaredToLine(const QLineF& line) const;
    [[nodiscard]] double distanceToStraightLine(const QCPVector2D& base, const QCPVector2D& direction) const;

    QCPVector2D& operator*=(double factor);
    QCPVector2D& operator/=(double divisor);
    QCPVector2D& operator+=(const QCPVector2D& vector);
    QCPVector2D& operator-=(const QCPVector2D& vector);

private:
    // property members:
    double mX, mY;

    friend inline const QCPVector2D operator*(double factor, const QCPVector2D& vec);
    friend inline const QCPVector2D operator*(const QCPVector2D& vec, double factor);
    friend inline const QCPVector2D operator/(const QCPVector2D& vec, double divisor);
    friend inline const QCPVector2D operator+(const QCPVector2D& vec1, const QCPVector2D& vec2);
    friend inline const QCPVector2D operator-(const QCPVector2D& vec1, const QCPVector2D& vec2);
    friend inline const QCPVector2D operator-(const QCPVector2D& vec);
};

Q_DECLARE_TYPEINFO(QCPVector2D, Q_MOVABLE_TYPE);

inline const QCPVector2D operator*(double factor, const QCPVector2D& vec)
{
    return QCPVector2D(vec.mX * factor, vec.mY * factor);
}

inline const QCPVector2D operator*(const QCPVector2D& vec, double factor)
{
    return QCPVector2D(vec.mX * factor, vec.mY * factor);
}

inline const QCPVector2D operator/(const QCPVector2D& vec, double divisor)
{
    return QCPVector2D(vec.mX / divisor, vec.mY / divisor);
}

inline const QCPVector2D operator+(const QCPVector2D& vec1, const QCPVector2D& vec2)
{
    return QCPVector2D(vec1.mX + vec2.mX, vec1.mY + vec2.mY);
}

inline const QCPVector2D operator-(const QCPVector2D& vec1, const QCPVector2D& vec2)
{
    return QCPVector2D(vec1.mX - vec2.mX, vec1.mY - vec2.mY);
}

inline const QCPVector2D operator-(const QCPVector2D& vec)
{
    return QCPVector2D(-vec.mX, -vec.mY);
}

/*! \relates QCPVector2D

  Prints \a vec in a human readable format to the qDebug output.
*/
inline QDebug operator<<(QDebug d, const QCPVector2D& vec)
{
    d.nospace() << "QCPVector2D(" << vec.x() << ", " << vec.y() << ")";
    return d.space();
}

#endif // QCPVECTOR2D_H
