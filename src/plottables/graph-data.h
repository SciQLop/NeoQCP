/***************************************************************************
**                                                                        **
**  NeoQCP                                                                **
**                                                                        **
**  This program is free software: you can redistribute it and/or modify  **
**  it under the terms of the GNU General Public License as published by  **
**  the Free Software Foundation, either version 3 of the License, or     **
**  (at your option) any later version.                                   **
**                                                                        **
****************************************************************************/

#pragma once

#include "../axis/range.h"
#include "../datacontainer.h"
#include "../global.h"

/*! \class QCPGraphData
  \brief Holds the data of one single data point for QCPGraph.

  The stored data is:
  \li \a key: coordinate on the key axis of this data point (this is the \a mainKey and the \a
  sortKey)
  \li \a value: coordinate on the value axis of this data point (this is the \a mainValue)

  The container for storing multiple data points is \ref QCPGraphDataContainer. It is a typedef for
  \ref QCPDataContainer with \ref QCPGraphData as the DataType template parameter. See the
  documentation there for an explanation regarding the data type's generic methods.

  \see QCPGraphDataContainer
*/
class QCP_LIB_DECL QCPGraphData
{
public:
    QCPGraphData();
    QCPGraphData(double key, double value);

    inline double sortKey() const { return key; }

    inline static QCPGraphData fromSortKey(double sortKey) { return QCPGraphData(sortKey, 0); }

    inline static bool sortKeyIsMainKey() { return true; }

    inline double mainKey() const { return key; }

    inline double mainValue() const { return value; }

    inline QCPRange valueRange() const { return QCPRange(value, value); }

    double key, value;
};

Q_DECLARE_TYPEINFO(QCPGraphData, Q_PRIMITIVE_TYPE);

/* start documentation of inline functions */

/*! \fn double QCPGraphData::sortKey() const

  Returns the \a key member of this data point.

  For a general explanation of what this method is good for in the context of the data container,
  see the documentation of \ref QCPDataContainer.
*/

/*! \fn static QCPGraphData QCPGraphData::fromSortKey(double sortKey)

  Returns a data point with the specified \a sortKey. All other members are set to zero.

  For a general explanation of what this method is good for in the context of the data container,
  see the documentation of \ref QCPDataContainer.
*/

/*! \fn static static bool QCPGraphData::sortKeyIsMainKey()

  Since the member \a key is both the data point key coordinate and the data ordering parameter,
  this method returns true.

  For a general explanation of what this method is good for in the context of the data container,
  see the documentation of \ref QCPDataContainer.
*/

/*! \fn double QCPGraphData::mainKey() const

  Returns the \a key member of this data point.

  For a general explanation of what this method is good for in the context of the data container,
  see the documentation of \ref QCPDataContainer.
*/

/*! \fn double QCPGraphData::mainValue() const

  Returns the \a value member of this data point.

  For a general explanation of what this method is good for in the context of the data container,
  see the documentation of \ref QCPDataContainer.
*/

/*! \fn QCPRange QCPGraphData::valueRange() const

  Returns a QCPRange with both lower and upper boundary set to \a value of this data point.

  For a general explanation of what this method is good for in the context of the data container,
  see the documentation of \ref QCPDataContainer.
*/

/* end documentation of inline functions */

/*! \typedef QCPGraphDataContainer

  Container for storing \ref QCPGraphData points. The data is stored sorted by \a key.

  This template instantiation is the container in which QCPGraph holds its data. For details about
  the generic container, see the documentation of the class template \ref QCPDataContainer.

  \see QCPGraphData, QCPGraph::setData
*/
using QCPGraphDataContainer = QCPDataContainer<QCPGraphData>;
