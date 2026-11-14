#ifndef QCP_THEME_H
#define QCP_THEME_H

#include "global.h"
#include <QColor>
#include <QObject>

class QCP_LIB_DECL QCPTheme : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(QCPTheme)
    Q_PROPERTY(QColor background READ background WRITE setBackground NOTIFY changed)
    Q_PROPERTY(QColor foreground READ foreground WRITE setForeground NOTIFY changed)
    Q_PROPERTY(QColor grid READ grid WRITE setGrid NOTIFY changed)
    Q_PROPERTY(QColor subGrid READ subGrid WRITE setSubGrid NOTIFY changed)
    Q_PROPERTY(QColor selection READ selection WRITE setSelection NOTIFY changed)
    Q_PROPERTY(QColor legendBackground READ legendBackground WRITE setLegendBackground NOTIFY changed)
    Q_PROPERTY(QColor legendBorder READ legendBorder WRITE setLegendBorder NOTIFY changed)
    Q_PROPERTY(QColor tooltipBackground READ tooltipBackground WRITE setTooltipBackground NOTIFY changed)
    Q_PROPERTY(QColor tooltipBorder READ tooltipBorder WRITE setTooltipBorder NOTIFY changed)
    Q_PROPERTY(QColor tooltipText READ tooltipText WRITE setTooltipText NOTIFY changed)
    Q_PROPERTY(qreal tooltipCornerRadius READ tooltipCornerRadius WRITE setTooltipCornerRadius NOTIFY changed)
    Q_PROPERTY(QColor crosshairColor READ crosshairColor WRITE setCrosshairColor NOTIFY changed)
    Q_PROPERTY(QString busyIndicatorSymbol READ busyIndicatorSymbol WRITE setBusyIndicatorSymbol NOTIFY changed)
    Q_PROPERTY(qreal busyFadeAlpha READ busyFadeAlpha WRITE setBusyFadeAlpha NOTIFY changed)
    Q_PROPERTY(int busyShowDelayMs READ busyShowDelayMs WRITE setBusyShowDelayMs NOTIFY changed)
    Q_PROPERTY(int busyHideDelayMs READ busyHideDelayMs WRITE setBusyHideDelayMs NOTIFY changed)

public:
    explicit QCPTheme(QObject* parent = nullptr);

    [[nodiscard]] QColor background() const { return mBackground; }
    [[nodiscard]] QColor foreground() const { return mForeground; }
    [[nodiscard]] QColor grid() const { return mGrid; }
    [[nodiscard]] QColor subGrid() const { return mSubGrid; }
    [[nodiscard]] QColor selection() const { return mSelection; }
    [[nodiscard]] QColor legendBackground() const { return mLegendBackground; }
    [[nodiscard]] QColor legendBorder() const { return mLegendBorder; }
    [[nodiscard]] QColor tooltipBackground() const { return mTooltipBackground; }
    [[nodiscard]] QColor tooltipBorder() const { return mTooltipBorder; }
    [[nodiscard]] QColor tooltipText() const { return mTooltipText; }
    [[nodiscard]] qreal tooltipCornerRadius() const { return mTooltipCornerRadius; }
    [[nodiscard]] QColor crosshairColor() const { return mCrosshairColor; }
    [[nodiscard]] QString busyIndicatorSymbol() const { return mBusyIndicatorSymbol; }
    [[nodiscard]] qreal busyFadeAlpha() const { return mBusyFadeAlpha; }
    [[nodiscard]] int busyShowDelayMs() const { return mBusyShowDelayMs; }
    [[nodiscard]] int busyHideDelayMs() const { return mBusyHideDelayMs; }

    void setBackground(const QColor& color);
    void setForeground(const QColor& color);
    void setGrid(const QColor& color);
    void setSubGrid(const QColor& color);
    void setSelection(const QColor& color);
    void setLegendBackground(const QColor& color);
    void setLegendBorder(const QColor& color);
    void setTooltipBackground(const QColor& color);
    void setTooltipBorder(const QColor& color);
    void setTooltipText(const QColor& color);
    void setTooltipCornerRadius(qreal radius);
    void setCrosshairColor(const QColor& color);
    void setBusyIndicatorSymbol(const QString& symbol);
    void setBusyFadeAlpha(qreal alpha);
    void setBusyShowDelayMs(int ms);
    void setBusyHideDelayMs(int ms);

    static QCPTheme* light(QObject* parent = nullptr);
    static QCPTheme* dark(QObject* parent = nullptr);

signals:
    void changed();

private:
    QColor mBackground;
    QColor mForeground;
    QColor mGrid;
    QColor mSubGrid;
    QColor mSelection;
    QColor mLegendBackground;
    QColor mLegendBorder;
    QColor mTooltipBackground { 0xee, 0xf2, 0xf7, 220 };
    QColor mTooltipBorder { 0, 0, 0, 150 };
    QColor mTooltipText { Qt::black };
    qreal mTooltipCornerRadius = 4;
    QColor mCrosshairColor { 128, 128, 128, 180 };
    QString mBusyIndicatorSymbol = QStringLiteral("⟳");
    qreal mBusyFadeAlpha = 0.3;
    int mBusyShowDelayMs = 500;
    int mBusyHideDelayMs = 500;
};

#endif // QCP_THEME_H
