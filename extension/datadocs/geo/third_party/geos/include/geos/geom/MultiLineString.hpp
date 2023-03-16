/**********************************************************************
 *
 * GEOS - Geometry Engine Open Source
 * http://geos.osgeo.org
 *
 * Copyright (C) 2011 Sandro Santilli <strk@kbt.io>
 * Copyright (C) 2001-2002 Vivid Solutions Inc.
 * Copyright (C) 2005 2006 Refractions Research Inc.
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU Lesser General Public Licence as published
 * by the Free Software Foundation.
 * See the COPYING file for more information.
 *
 **********************************************************************
 *
 * Last port: geom/MultiLineString.java r320 (JTS-1.12)
 *
 **********************************************************************/

#pragma once

#include "geos/export.hpp"
#include "geos/geom/GeometryCollection.hpp" // for inheritance
#include "geos/geom/LineString.hpp"

#include <string>
#include <vector>

namespace geos {
namespace geom { // geos::geom

/// Models a collection of [LineStrings](@ref geom::LineString).
class GEOS_DLL MultiLineString : public GeometryCollection {
public:
	friend class GeometryFactory;

	~MultiLineString() override = default;

	const LineString *getGeometryN(std::size_t n) const override;

	std::string getGeometryType() const override;

	GeometryTypeId getGeometryTypeId() const override;

	bool isClosed() const;

	/// Returns line dimension (1)
	Dimension::DimensionType getDimension() const override;

	bool isDimensionStrict(Dimension::DimensionType d) const override {
		return d == Dimension::L;
	}

	/// Returns a (possibly empty) [MultiPoint](@ref geom::MultiPoint)
	std::unique_ptr<Geometry> getBoundary() const override;

	/**
	 * \brief
	 * Returns Dimension::False if all [LineStrings](@ref geom::LineString) in the collection
	 * are closed, 0 otherwise.
	 */
	int getBoundaryDimension() const override;

protected:
	/**
	 * \brief Constructs a MultiLineString.
	 *
	 * @param  newLines The [LineStrings](@ref geom::LineString) for this
	 *                  MultiLineString, or `null`
	 *                  or an empty array to create the empty geometry.
	 *                  Elements may be empty LineString,
	 *                  but not `null`s.
	 *
	 * @param newFactory The GeometryFactory used to create this geometry.
	 *                   Caller must keep the factory alive for the life-time
	 *                   of the constructed MultiLineString.
	 *
	 * @note Constructed object will take ownership of
	 *       the vector and its elements.
	 *
	 */
	MultiLineString(std::vector<Geometry *> *newLines, const GeometryFactory *newFactory);

	MultiLineString(std::vector<std::unique_ptr<LineString>> &&newLines, const GeometryFactory &newFactory);

	MultiLineString(std::vector<std::unique_ptr<Geometry>> &&newLines, const GeometryFactory &newFactory);

	MultiLineString(const MultiLineString &mp) : GeometryCollection(mp) {};

	MultiLineString *cloneImpl() const override {
		return new MultiLineString(*this);
	}

	int getSortIndex() const override {
		return SORTINDEX_MULTILINESTRING;
	};
};

} // namespace geom
} // namespace geos
