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
 * Last port: geom/Point.java r320 (JTS-1.12)
 *
 **********************************************************************/

#pragma once

#include <geos/export.hpp>
#include <geos/geom/CoordinateSequence.hpp> // for proper use of unique_ptr<>
#include <geos/geom/Dimension.hpp>          // for Dimension::DimensionType
#include <geos/geom/Envelope.hpp>           // for proper use of unique_ptr<>
#include <geos/geom/FixedSizeCoordinateSequence.hpp>
#include <geos/geom/Geometry.hpp> // for inheritance
#include <memory>                 // for unique_ptr
#include <string>
#include <vector>

// Forward declarations
namespace geos {
namespace geom { // geos::geom
class Coordinate;
class CoordinateArraySequence;
class CoordinateFilter;
class CoordinateSequenceFilter;
class GeometryComponentFilter;
class GeometryFilter;
} // namespace geom
} // namespace geos

namespace geos {
namespace geom { // geos::geom

/**
 * Implementation of Point.
 *
 * A Point is valid iff:
 *
 * - the coordinate which defines it is a valid coordinate
 *   (i.e does not have an NaN X or Y ordinate)
 *
 */
class GEOS_DLL Point : public Geometry {

public:
	friend class GeometryFactory;

	/// A vector of const Point pointers
	typedef std::vector<const Point *> ConstVect;

	~Point() override = default;

	/**
	 * Creates and returns a full copy of this {@link Point} object.
	 * (including all coordinates contained by it).
	 *
	 * @return a clone of this instance
	 */
	std::unique_ptr<Point> clone() const {
		return std::unique_ptr<Point>(cloneImpl());
	}

	const CoordinateSequence *getCoordinatesRO() const;

	std::size_t getNumPoints() const override;
	bool isEmpty() const override;
	bool isSimple() const override;

	/// Returns point dimension (0)
	Dimension::DimensionType getDimension() const override;

	/// Returns coordinate dimension.
	uint8_t getCoordinateDimension() const override;

	void setXY(double x, double y) {
		empty3d = empty2d = false;
		coordinates.setAt({x, y}, 0);
		geometryChangedAction();
	}

	const Coordinate *getCoordinate() const override {
		return isEmpty() ? nullptr : &coordinates[0];
	}

	double getX() const;
	double getY() const;
	double getZ() const;

	std::string getGeometryType() const override;
	GeometryTypeId getGeometryTypeId() const override;
	void apply_ro(CoordinateFilter *filter) const override;
	void apply_rw(const CoordinateFilter *filter) override;
	void apply_ro(GeometryFilter *filter) const override;
	void apply_rw(GeometryFilter *filter) override;
	void apply_rw(GeometryComponentFilter *filter) override;
	void apply_ro(GeometryComponentFilter *filter) const override;
	void apply_rw(CoordinateSequenceFilter &filter) override;
	void apply_ro(CoordinateSequenceFilter &filter) const override;

	/**
	 * Gets the boundary of this geometry.
	 * Zero-dimensional geometries have no boundary by definition,
	 * so an empty GeometryCollection is returned.
	 *
	 * @return an empty GeometryCollection
	 * @see Geometry::getBoundary
	 */
	std::unique_ptr<Geometry> getBoundary() const override;

	/// Returns Dimension::False (Point has no boundary)
	int getBoundaryDimension() const override;

protected:
	/**
	 * \brief
	 * Creates a Point taking ownership of the given CoordinateSequence
	 * (must have 1 element)
	 *
	 * @param  newCoords
	 *	contains the single coordinate on which to base this
	 *	<code>Point</code> or <code>null</code> to create
	 *	the empty geometry.
	 *
	 * @param newFactory the GeometryFactory used to create this geometry
	 */
	Point(CoordinateSequence *newCoords, const GeometryFactory *newFactory);

	Point(const Coordinate &c, const GeometryFactory *newFactory);

	Point(const CoordinateXY &c, const GeometryFactory *newFactory);

	Point(const Point &p);

	Point *cloneImpl() const override {
		return new Point(*this);
	}

	Envelope::Ptr computeEnvelopeInternal() const override;

	int getSortIndex() const override {
		return SORTINDEX_POINT;
	};

private:
	/**
	 *  The <code>Coordinate</code> wrapped by this <code>Point</code>.
	 */
	FixedSizeCoordinateSequence<1> coordinates;

	bool empty2d;
	bool empty3d;
};

} // namespace geom
} // namespace geos
