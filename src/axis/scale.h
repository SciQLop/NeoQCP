#ifndef QCP_SCALE_H
#define QCP_SCALE_H

#include "../global.h"
#include "range.h"

/*!
  The mapping an axis, a colour scale or a plottable's data applies: linear, log or symlog.

  A scale maps a data value into *scale space*, where the pixel mapping is linear. Log is
  log_base(|v|): the sign rule and the off-range rule depend on the axis range, so they stay in the
  axis and in QCPTransform::Snapshot. Symlog is matplotlib's SymmetricalLogTransform, with
  a' = linearScale / (1 - 1/base):

  - |v| <= threshold: v * a'
  - otherwise: sign(v) * threshold * (a' + log_base(|v| / threshold))

  A default-constructed scale is linear. A bad parameter is refused through the misuse tier and
  gives the factory's default: a threshold or a linear scale that is not finite and positive, a
  base that is not finite or not greater than 1.
*/
class QCP_LIB_DECL QCPScale
{
    Q_GADGET

public:
    //! The kind of mapping. \see linear, log, symlog
    enum class Kind : quint8
    {
        Linear, //!< the identity
        Log, //!< log_base(|v|)
        Symlog //!< matplotlib's SymmetricalLogTransform
    };
    Q_ENUM(Kind)

    QCPScale() = default;

    //! A scale that maps a value to itself.
    static QCPScale linear();
    //! log_base(|v|). A base not finite or not greater than 1 is refused.
    static QCPScale log(double base = kDefaultBase);
    /*!
      matplotlib's SymmetricalLogTransform. See the class documentation for the formula. Any
      parameter that is refused gives symlog() with all its defaults, not just the refused one.
    */
    static QCPScale symlog(double threshold = kDefaultThreshold, double base = kDefaultBase,
                           double linearScale = kDefaultLinearScale);

    Kind kind() const { return mKind; }

    bool isLinear() const { return mKind == Kind::Linear; }

    bool isLog() const { return mKind == Kind::Log; }

    bool isSymlog() const { return mKind == Kind::Symlog; }

    double base() const { return mBase; }

    double threshold() const { return mThreshold; }

    double linearScale() const { return mLinearScale; }

    //! The value in scale space. Log ignores the sign, as the axis owns the sign rule.
    double toScale(double value) const;
    //! The value a scale-space coordinate came from. Log returns the positive value.
    double fromScale(double s) const;
    //! The range as this scale can show it: the log rule on a log scale, else the linear rule.
    [[nodiscard]] QCPRange sanitized(const QCPRange& range) const;

    bool operator==(const QCPScale& other) const = default;

private:
    //! What log() and symlog() default to, and what a refused parameter gives back.
    static constexpr double kDefaultBase = 10.0;
    static constexpr double kDefaultThreshold = 2.0;
    static constexpr double kDefaultLinearScale = 1.0;

    QCPScale(Kind kind, double base, double threshold, double linearScale);

    Kind mKind = Kind::Linear;
    double mBase = kDefaultBase;
    double mThreshold = kDefaultThreshold;
    double mLinearScale = kDefaultLinearScale;
};

//! Hash key for QHash and dependency revisions.
QCP_LIB_DECL size_t qHash(const QCPScale& scale, size_t seed = 0);

#endif // QCP_SCALE_H
