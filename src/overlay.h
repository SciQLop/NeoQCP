#ifndef QCP_OVERLAY_H
#define QCP_OVERLAY_H

#include "global.h"
#include "layer.h"

class QCPPainter;

class QCP_LIB_DECL QCPOverlay : public QCPLayerable
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(bool collapsible READ isCollapsible WRITE setCollapsible NOTIFY collapsibleChanged)
    Q_PROPERTY(bool collapsed READ isCollapsed WRITE setCollapsed NOTIFY collapsedChanged)
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity NOTIFY opacityChanged)
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
    /// \endcond

public:
    enum Level { Info, Warning, Error };
    Q_ENUM(Level)
    enum SizeMode { Compact, FitContent, FullWidget };
    Q_ENUM(SizeMode)
    enum Position { Top, Bottom, Left, Right };
    Q_ENUM(Position)

    explicit QCPOverlay(QCustomPlot* parentPlot);
    ~QCPOverlay() override = default;

    // getters:
    [[nodiscard]] QString text() const { return mText; }
    [[nodiscard]] Level level() const { return mLevel; }
    [[nodiscard]] SizeMode sizeMode() const { return mSizeMode; }
    [[nodiscard]] Position position() const { return mPosition; }
    [[nodiscard]] bool isCollapsible() const { return mCollapsible; }
    [[nodiscard]] bool isCollapsed() const { return mCollapsed; }
    [[nodiscard]] qreal opacity() const { return mOpacity; }
    [[nodiscard]] QFont font() const { return mFont; }
    [[nodiscard]] QRect overlayRect() const;

    // actions:
    void showMessage(const QString& text, Level level = Info,
                     SizeMode sizeMode = Compact, Position position = Top);
    void clearMessage();

    // setters:
    void setCollapsible(bool enabled);
    void setCollapsed(bool collapsed);
    void setOpacity(qreal opacity);
    void setFont(const QFont& font);

signals:
    void messageChanged(const QString& text, QCPOverlay::Level level);
    void collapsedChanged(bool collapsed);
    void collapsibleChanged(bool collapsible);
    void opacityChanged(qreal opacity);
    void fontChanged(const QFont& font);

protected:
    void applyDefaultAntialiasingHint(QCPPainter* painter) const override;
    void draw(QCPPainter* painter) override;
    double selectTest(const QPointF& pos, bool onlySelectable,
                      QVariant* details = nullptr) const override;
    void mousePressEvent(QMouseEvent* event, const QVariant& details) override;

private:
    QString mText;
    Level mLevel = Info;
    SizeMode mSizeMode = Compact;
    Position mPosition = Top;
    bool mCollapsible = false;
    bool mCollapsed = false;
    qreal mOpacity = 1.0;
    QFont mFont;

    QColor levelColor() const;
    QRect computeRect() const;
    void redraw();
    QRect collapseHandleRect() const;

    Q_DISABLE_COPY_MOVE(QCPOverlay)
};

#endif // QCP_OVERLAY_H
