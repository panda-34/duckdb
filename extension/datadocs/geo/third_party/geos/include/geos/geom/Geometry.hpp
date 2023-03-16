/**********************************************************************
 *
 * GEOS - Geometry Engine Open Source
 * http://geos.osgeo.org
 *
 * Copyright (C) 2009 2011 Sandro Santilli <strk@kbt.io>
 * Copyright (C) 2005 2006 Refractions Research Inc.
 * Copyright (C) 2001-2002 Vivid Solutions Inc.
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU Lesser General Public Licence as published
 * by the Free Software Foundation.
 * See the COPYING file for more information.
 *
 **********************************************************************
 *
 * Last port: geom/Geometry.java rev. 1.112
 *
 **********************************************************************/

#pragma once

#include <algorithm>
#include <geos/export.hpp>
#include <geos/geom/Dimension.hpp> // for Dimension::DimensionType
#include <geos/geom/Envelope.hpp>
#include <geos/geom/GeometryComponentFilter.hpp> // for inheritance
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// Forward declarations
namespace geos {
namespace geom {
class Coordinate;
class CoordinateFilter;
class CoordinateSequence;
class CoordinateSequenceFilter;
class GeometryComponentFilter;
class GeometryFactory;
class GeometryFilter;
class PrecisionModel;
class Point;
class IntersectionMatrix;
} // namespace geom
namespace io { // geos.io
class Unload;
} // namespace io
} // namespace geos

namespace geos { // geos
namespace geom { // geos::geom

/// Geometry types
enum GeometryTypeId {
	/// a point
	GEOS_POINT,
	/// a linestring
	GEOS_LINESTRING,
	/// a linear ring (linestring with 1st point == last point)
	GEOS_LINEARRING,
	/// a polygon
	GEOS_POLYGON,
	/// a collection of points
	GEOS_MULTIPOINT,
	/// a collection of linestrings
	GEOS_MULTILINESTRING,
	/// a collection of polygons
	GEOS_MULTIPOLYGON,
	/// a collection of heterogeneus geometries
	GEOS_GEOMETRYCOLLECTION
};

enum GeometrySortIndex {
	SORTINDEX_POINT = 0,
	SORTINDEX_MULTIPOINT = 1,
	SORTINDEX_LINESTRING = 2,
	SORTINDEX_LINEARRING = 3,
	SORTINDEX_MULTILINESTRING = 4,
	SORTINDEX_POLYGON = 5,
	SORTINDEX_MULTIPOLYGON = 6,
	SORTINDEX_GEOMETRYCOLLECTION = 7
};

/**
 * \class Geometry geom.h geos.h
 *
 * \brief Basic implementation of Geometry, constructed and
 * destructed by GeometryFactory.
 *
 *  <code>clone</code> returns a deep copy of the object.
 *  Use GeometryFactory to construct.
 *
 *  <H3>Binary Predicates</H3>
 * Because it is not clear at this time
 * what semantics for spatial
 *  analysis methods involving <code>GeometryCollection</code>s would be useful,
 *  <code>GeometryCollection</code>s are not supported as arguments to binary
 *  predicates (other than <code>convexHull</code>) or the <code>relate</code>
 *  method.
 *
 *  <H3>Set-Theoretic Methods</H3>
 *
 *  The spatial analysis methods will
 *  return the most specific class possible to represent the result. If the
 *  result is homogeneous, a <code>Point</code>, <code>LineString</code>, or
 *  <code>Polygon</code> will be returned if the result contains a single
 *  element; otherwise, a <code>MultiPoint</code>, <code>MultiLineString</code>,
 *  or <code>MultiPolygon</code> will be returned. If the result is
 *  heterogeneous a <code>GeometryCollection</code> will be returned. <P>
 *
 *  Because it is not clear at this time what semantics for set-theoretic
 *  methods involving <code>GeometryCollection</code>s would be useful,
 * <code>GeometryCollections</code>
 *  are not supported as arguments to the set-theoretic methods.
 *
 *  <H4>Representation of Computed Geometries </H4>
 *
 *  The SFS states that the result
 *  of a set-theoretic method is the "point-set" result of the usual
 *  set-theoretic definition of the operation (SFS 3.2.21.1). However, there are
 *  sometimes many ways of representing a point set as a <code>Geometry</code>.
 *  <P>
 *
 *  The SFS does not specify an unambiguous representation of a given point set
 *  returned from a spatial analysis method. One goal of JTS is to make this
 *  specification precise and unambiguous. JTS will use a canonical form for
 *  <code>Geometry</code>s returned from spatial analysis methods. The canonical
 *  form is a <code>Geometry</code> which is simple and noded:
 *  <UL>
 *    <LI> Simple means that the Geometry returned will be simple according to
 *    the JTS definition of <code>isSimple</code>.
 *    <LI> Noded applies only to overlays involving <code>LineString</code>s. It
 *    means that all intersection points on <code>LineString</code>s will be
 *    present as endpoints of <code>LineString</code>s in the result.
 *  </UL>
 *  This definition implies that non-simple geometries which are arguments to
 *  spatial analysis methods must be subjected to a line-dissolve process to
 *  ensure that the results are simple.
 *
 *  <H4> Constructed Points And The Precision Model </H4>
 *
 *  The results computed by the set-theoretic methods may
 *  contain constructed points which are not present in the input Geometry.
 *  These new points arise from intersections between line segments in the
 *  edges of the input Geometry. In the general case it is not
 *  possible to represent constructed points exactly. This is due to the fact
 *  that the coordinates of an intersection point may contain twice as many bits
 *  of precision as the coordinates of the input line segments. In order to
 *  represent these constructed points explicitly, JTS must truncate them to fit
 *  the PrecisionModel.
 *
 *  Unfortunately, truncating coordinates moves them slightly. Line segments
 *  which would not be coincident in the exact result may become coincident in
 *  the truncated representation. This in turn leads to "topology collapses" --
 *  situations where a computed element has a lower dimension than it would in
 *  the exact result.
 *
 *  When JTS detects topology collapses during the computation of spatial
 *  analysis methods, it will throw an exception. If possible the exception will
 *  report the location of the collapse.
 *
 *  equals(Object) and hashCode are not overridden, so that when two
 *  topologically equal Geometries are added to HashMaps and HashSets, they
 *  remain distinct. This behaviour is desired in many cases.
 *
 */
class GEOS_DLL Geometry {

public:
	friend class GeometryFactory;

	/// A vector of const Geometry pointers
	using ConstVect = std::vector<const Geometry *>;

	/// A vector of non-const Geometry pointers
	using NonConstVect = std::vector<Geometry *>;

	/// An unique_ptr of Geometry
	using Ptr = std::unique_ptr<Geometry>;

	/// Make a deep-copy of this Geometry
	std::unique_ptr<Geometry> clone() const {
		return std::unique_ptr<Geometry>(cloneImpl());
	}

	/// Destroy Geometry and all components
	virtual ~Geometry();

	/**
	 * \brief
	 * Gets the factory which contains the context in which this
	 * geometry was created.
	 *
	 * @return the factory for this geometry
	 */
	const GeometryFactory *getFactory() const {
		return _factory;
	}

	/**
	 * \brief
	 * A simple scheme for applications to add their own custom data to
	 * a Geometry.
	 * An example use might be to add an object representing a
	 * Coordinate Reference System.
	 *
	 * Note that user data objects are not present in geometries created
	 * by construction methods.
	 *
	 * @param newUserData an object, the semantics for which are
	 * defined by the application using this Geometry
	 */
	void setUserData(void *newUserData) {
		_userData = newUserData;
	}

	/**
	 * \brief
	 * Gets the user data object for this geometry, if any.
	 *
	 * @return the user data object, or <code>null</code> if none set
	 */
	void *getUserData() const {
		return _userData;
	}

	/** \brief
	 * Returns the ID of the Spatial Reference System used by the Geometry.
	 *
	 * GEOS supports Spatial Reference System information in the simple way
	 * defined in the SFS. A Spatial Reference System ID (SRID) is present
	 * in each Geometry object. Geometry provides basic accessor operations
	 * for this field, but no others. The SRID is represented as an integer.
	 *
	 * @return the ID of the coordinate space in which the Geometry is defined.
	 */
	virtual int getSRID() const {
		return SRID;
	}

	/** \brief
	 * Sets the ID of the Spatial Reference System used by the Geometry.
	 */
	virtual void setSRID(int newSRID) {
		SRID = newSRID;
	}

	/**
	 * \brief
	 * Get the PrecisionModel used to create this Geometry.
	 */
	const PrecisionModel *getPrecisionModel() const;

	/// Returns a vertex of this Geometry, or NULL if this is the empty geometry.
	virtual const CoordinateXY *getCoordinate() const = 0; // Abstract

	/// Returns the count of this Geometrys vertices.
	virtual std::size_t getNumPoints() const = 0; // Abstract

	/// Return a string representation of this Geometry type
	virtual std::string getGeometryType() const = 0; // Abstract

	/// Return an integer representation of this Geometry type
	virtual GeometryTypeId getGeometryTypeId() const = 0; // Abstract

	/// Returns the number of geometries in this collection
	/// (or 1 if this is not a collection)
	virtual std::size_t getNumGeometries() const {
		return 1;
	}

	/// \brief Returns a pointer to the nth Geometry in this collection
	/// (or self if this is not a collection)
	virtual const Geometry *getGeometryN(std::size_t /*n*/) const {
		return this;
	}

	/**
	 * \brief Tests the validity of this <code>Geometry</code>.
	 *
	 * Subclasses provide their own definition of "valid".
	 *
	 * @return <code>true</code> if this <code>Geometry</code> is valid
	 *
	 * @see IsValidOp
	 */
	virtual bool isValid() const;

	/// Returns whether or not the set of points in this Geometry is empty.
	virtual bool isEmpty() const = 0; // Abstract

	/// Returns the dimension of this Geometry (0=point, 1=line, 2=surface)
	virtual Dimension::DimensionType getDimension() const = 0; // Abstract

	/// Checks whether this Geometry consists only of components having dimension d.
	virtual bool isDimensionStrict(Dimension::DimensionType d) const {
		return d == getDimension();
	}

	bool isPuntal() const {
		return isDimensionStrict(Dimension::P);
	}

	bool isLineal() const {
		return isDimensionStrict(Dimension::L);
	}

	bool isPolygonal() const {
		return isDimensionStrict(Dimension::A);
	}

	bool isCollection() const {
		int t = getGeometryTypeId();
		return t == GEOS_GEOMETRYCOLLECTION || t == GEOS_MULTIPOINT || t == GEOS_MULTILINESTRING ||
		       t == GEOS_MULTIPOLYGON;
	}

	/// Returns the coordinate dimension of this Geometry (2=XY, 3=XYZ, 4=XYZM in future).
	virtual uint8_t getCoordinateDimension() const = 0; // Abstract

	/** \brief
	 * Returns the minimum and maximum x and y values in this Geometry,
	 * or a null Envelope if this Geometry is empty.
	 */
	virtual const Envelope *getEnvelopeInternal() const;

	/// Returns the area of this Geometry.
	virtual double getArea() const;

	/** \brief
	 * Returns a Geometry representing all the points in this Geometry
	 * and other.
	 *
	 * @throws util::TopologyException if a robustness error occurs
	 * @throws util::IllegalArgumentException if either input is a
	 *         non-empty GeometryCollection
	 *
	 */
	std::unique_ptr<Geometry> Union(const Geometry *other) const;
	// throw(IllegalArgumentException *, TopologyException *);

	/** \brief
	 * Computes the union of all the elements of this geometry. Heterogeneous
	 * [GeometryCollections](@ref GeometryCollection) are fully supported.
	 *
	 * The result obeys the following contract:
	 *
	 * - Unioning a set of [LineStrings](@ref LineString) has the effect of fully noding
	 *   and dissolving the linework.
	 * - Unioning a set of [Polygons](@ref Polygon) will always
	 *   return a polygonal geometry (unlike Geometry::Union(const Geometry* other) const),
	 *   which may return geometrys of lower dimension if a topology collapse
	 *   occurred.
	 *
	 * @return the union geometry
	 *
	 * @see UnaryUnionOp
	 */
	Ptr Union() const;
	// throw(IllegalArgumentException *, TopologyException *);

	virtual void apply_rw(const CoordinateFilter *filter) = 0; // Abstract
	virtual void apply_ro(CoordinateFilter *filter) const = 0; // Abstract
	virtual void apply_rw(GeometryFilter *filter);
	virtual void apply_ro(GeometryFilter *filter) const;
	virtual void apply_rw(GeometryComponentFilter *filter);
	virtual void apply_ro(GeometryComponentFilter *filter) const;

	/**
	 *  Performs an operation on the coordinates in this Geometry's
	 *  CoordinateSequences.
	 *  If the filter reports that a coordinate value has been changed,
	 *  {@link #geometryChanged} will be called automatically.
	 *
	 * @param  filter  the filter to apply
	 */
	virtual void apply_rw(CoordinateSequenceFilter &filter) = 0;

	/**
	 *  Performs a read-only operation on the coordinates in this
	 *  Geometry's CoordinateSequences.
	 *
	 * @param  filter  the filter to apply
	 */
	virtual void apply_ro(CoordinateSequenceFilter &filter) const = 0;

	/** \brief
	 * Apply a filter to each component of this geometry.
	 * The filter is expected to provide a .filter(const Geometry*)
	 * method.
	 *
	 * I intend similar templated methods to replace
	 * all the virtual apply_rw and apply_ro functions...
	 *                --strk(2005-02-06);
	 */
	template <class T>
	void applyComponentFilter(T &f) const {
		for (std::size_t i = 0, n = getNumGeometries(); i < n; ++i) {
			f.filter(getGeometryN(i));
		}
	}

	/**
	 * \brief
	 * Notifies this Geometry that its Coordinates have been changed
	 * by an external party (using a CoordinateFilter, for example).
	 */
	virtual void geometryChanged();

	/**
	 * \brief
	 * Notifies this Geometry that its Coordinates have been changed
	 * by an external party.
	 */
	void geometryChangedAction() {
		envelope.reset();
	}

	/// Returns false if the Geometry not simple.
	virtual bool isSimple() const;

	/// Returns a buffer region around this Geometry having the given width.
	///
	/// @throws util::TopologyException if a robustness error occurs
	///
	std::unique_ptr<Geometry> buffer(double distance) const;

	/**
	 * \brief
	 * Returns a Geometry representing the points making up this
	 * Geometry that do not make up other.
	 *
	 * @throws util::TopologyException if a robustness error occurs
	 * @throws util::IllegalArgumentException if either input is a
	 *         non-empty GeometryCollection
	 *
	 */
	std::unique_ptr<Geometry> difference(const Geometry *other) const;

	/**
	 * \brief
	 * Returns the boundary, or an empty geometry of appropriate
	 * dimension if this <code>Geometry</code>  is empty.
	 *
	 * (In the case of zero-dimensional geometries,
	 * an empty GeometryCollection is returned.)
	 * For a discussion of this function, see the OpenGIS Simple
	 * Features Specification. As stated in SFS Section 2.1.13.1,
	 * "the boundary of a Geometry is a set of Geometries of the
	 * next lower dimension."
	 *
	 * @return  the closure of the combinatorial boundary
	 *          of this <code>Geometry</code>.
	 *          Ownershipof the returned object transferred to caller.
	 */
	virtual std::unique_ptr<Geometry> getBoundary() const = 0; // Abstract

	/// Returns the dimension of this Geometrys inherent boundary.
	virtual int getBoundaryDimension() const = 0; // Abstract

	/** \brief
	 * Returns a Geometry representing the points shared by
	 * this Geometry and other.
	 *
	 * @throws util::TopologyException if a robustness error occurs
	 * @throws util::IllegalArgumentException if either input is a
	 *         non-empty GeometryCollection
	 *
	 */
	std::unique_ptr<Geometry> intersection(const Geometry *other) const;

	/** \brief
	 * Computes the centroid of this <code>Geometry</code>.
	 *
	 * The centroid is equal to the centroid of the set of component
	 * Geometries of highest dimension (since the lower-dimension geometries
	 * contribute zero "weight" to the centroid)
	 *
	 * @return a {@link Point} which is the centroid of this Geometry
	 */
	virtual std::unique_ptr<Point> getCentroid() const;

	/// Computes the centroid of this Geometry as a Coordinate
	//
	/// Returns false if centroid cannot be computed (EMPTY geometry)
	///
	virtual bool getCentroid(Coordinate &ret) const;

	/// \brief
	/// Returns the smallest convex Polygon that contains
	/// all the points in the Geometry.
	virtual std::unique_ptr<Geometry> convexHull() const;

	/**
	 * \brief
	 * Returns true if the elements in the DE-9IM intersection matrix
	 * for the two Geometrys match the elements in intersectionPattern.
	 *
	 * IntersectionPattern elements may be: 0 1 2 T ( = 0, 1 or 2)
	 * F ( = -1) * ( = -1, 0, 1 or 2).
	 *
	 * For more information on the DE-9IM, see the OpenGIS Simple
	 * Features Specification.
	 *
	 * @throws util::IllegalArgumentException if either arg is a collection
	 *
	 */
	bool relate(const Geometry *g, const std::string &intersectionPattern) const;

	bool relate(const Geometry &g, const std::string &intersectionPattern) const {
		return relate(&g, intersectionPattern);
	}

	/// Returns the DE-9IM intersection matrix for the two Geometrys.
	std::unique_ptr<IntersectionMatrix> relate(const Geometry *g) const;

	std::unique_ptr<IntersectionMatrix> relate(const Geometry &g) const;

	/**
	 * \brief
	 * Returns true if the DE-9IM intersection matrix for the two
	 * Geometrys is T*F**FFF*.
	 */
	virtual bool equals(const Geometry *g) const;

	/// Returns true if other.within(this) returns true.
	virtual bool contains(const Geometry *g) const;

	/**
	 * Tests whether this geometry is disjoint from the specified geometry.
	 *
	 * The <code>disjoint</code> predicate has the following equivalent
	 * definitions:
	 *  - The two geometries have no point in common
	 *  - The DE-9IM Intersection Matrix for the two geometries matches
	 *    <code>[FF*FF****]</code>
	 *  - <code>! g.intersects(this)</code>
	 *    (<code>disjoint</code> is the inverse of <code>intersects</code>)
	 *
	 * @param  other  the Geometry with which to compare this Geometry
	 * @return true if the two <code>Geometry</code>s are disjoint
	 *
	 * @see Geometry::intersects
	 */
	virtual bool disjoint(const Geometry *other) const;

	/** \brief
	 * Returns true if the DE-9IM intersection matrix for the two
	 * Geometrys is FT*******, F**T***** or F***T****.
	 */
	virtual bool touches(const Geometry *other) const;

	/// Returns true if disjoint returns false.
	virtual bool intersects(const Geometry *g) const;

	/// Returns the length of this Geometry.
	virtual double getLength() const;

	/// Polygon overrides to check for actual rectangle
	virtual bool isRectangle() const {
		return false;
	}

protected:
	/// The bounding box of this Geometry
	mutable std::unique_ptr<Envelope> envelope;

	/// Make a deep-copy of this Geometry
	virtual Geometry *cloneImpl() const = 0;

	/// Returns true if the array contains any non-empty Geometrys.
	template <typename T>
	static bool hasNonEmptyElements(const std::vector<T> *geometries) {
		return std::any_of(geometries->begin(), geometries->end(), [](const T &g) { return !g->isEmpty(); });
	}

	/// Returns true if the vector contains any null elements.
	template <typename T>
	static bool hasNullElements(const std::vector<T> *geometries) {
		return std::any_of(geometries->begin(), geometries->end(), [](const T &g) { return g == nullptr; });
	}

	virtual Envelope::Ptr computeEnvelopeInternal() const = 0; // Abstract

	int SRID;

	Geometry(const Geometry &geom);

	/** \brief
	 * Construct a geometry with the given GeometryFactory.
	 *
	 * Will keep a reference to the factory, so don't
	 * delete it until al Geometry objects referring to
	 * it are deleted.
	 *
	 * @param factory
	 */
	Geometry(const GeometryFactory *factory);

	template <typename T>
	static std::vector<std::unique_ptr<Geometry>> toGeometryArray(std::vector<std::unique_ptr<T>> &&v) {
		static_assert(std::is_base_of<Geometry, T>::value, "");
		std::vector<std::unique_ptr<Geometry>> gv(v.size());
		for (std::size_t i = 0; i < v.size(); i++) {
			gv[i] = std::move(v[i]);
		}
		return gv;
	}

protected:
	virtual int getSortIndex() const = 0;

private:
	class GEOS_DLL GeometryChangedFilter : public GeometryComponentFilter {
	public:
		void filter_rw(Geometry *geom) override;
	};

	static GeometryChangedFilter geometryChangedFilter;

	/// The GeometryFactory used to create this Geometry
	///
	/// Externally owned
	///
	const GeometryFactory *_factory;

	void *_userData;
};

struct GEOS_DLL GeometryGreaterThen {
	bool operator()(const Geometry *first, const Geometry *second);
};

// We use this instead of std::pair<unique_ptr<Geometry>> because C++11
// forbids that construct:
// http://lwg.github.com/issues/lwg-closed.html#2068
struct GeomPtrPair {
	typedef std::unique_ptr<Geometry> GeomPtr;
	GeomPtr first;
	GeomPtr second;
};

} // namespace geom
} // namespace geos
