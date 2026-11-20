#ifndef QCP_TEXT_CACHE_H
#define QCP_TEXT_CACHE_H

#include "../global.h"

#include <QFont>
#include <QSize>
#include <QString>

struct QCPTextStyle;

namespace qcp
{
/*!
  The measured glyphs of one text, measured again only when something the measurement reads
  changes: the string, the font, mathText, or the text flags. A recording asks on every frame, and
  measuring is the expensive part.

  Keying on the flags too: they change the size (a tab expands, an alignment clips), so a cache
  without them hands back a stale box.
*/
class QCP_LIB_DECL TextGlyphCache
{
public:
    //! The size of \a text in \a style, measured once per distinct set of inputs.
    [[nodiscard]] QSize size(const QCPTextStyle& style, const QString& text) const;

private:
    mutable QString mText;
    mutable QFont mFont;
    mutable bool mMathText = false;
    mutable int mTextFlags = 0;
    mutable QSize mSize;
    mutable bool mValid = false;
};
} // namespace qcp

#endif // QCP_TEXT_CACHE_H
