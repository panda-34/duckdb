/**********************************************************************
 *
 * GEOS - Geometry Engine Open Source
 * http://geos.osgeo.org
 *
 * Copyright (C) 2005-2006 Refractions Research Inc.
 * Copyright (C) 2001-2002 Vivid Solutions Inc.
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU Lesser General Public Licence as published
 * by the Free Software Foundation.
 * See the COPYING file for more information.
 *
 ***********************************************************************
 *
 * Last port: operation/overlay/OverlayOp.java r567 (JTS-1.12+)
 *
 **********************************************************************/

#include <cassert>
#include <cmath>
#include <geos/constants.hpp>
#include <geos/geom/Coordinate.hpp>
#include <geos/geom/Geometry.hpp>
#include <geos/geom/GeometryFactory.hpp>
#include <geos/geom/LineString.hpp>
#include <geos/geom/Point.hpp>
#include <geos/geom/Polygon.hpp>
#include <geos/geom/PrecisionModel.hpp>
#include <geos/geomgraph/DirectedEdge.hpp>
#include <geos/geomgraph/DirectedEdgeStar.hpp>
#include <geos/geomgraph/Edge.hpp>
#include <geos/geomgraph/EdgeEndStar.hpp>
#include <geos/geomgraph/EdgeNodingValidator.hpp>
#include <geos/geomgraph/GeometryGraph.hpp>
#include <geos/geomgraph/Node.hpp>
#include <geos/operation/overlay/ElevationMatrix.hpp>
#include <geos/operation/overlay/LineBuilder.hpp>
#include <geos/operation/overlay/OverlayNodeFactory.hpp>
#include <geos/operation/overlay/OverlayOp.hpp>
#include <geos/operation/overlay/PointBuilder.hpp>
#include <geos/operation/overlay/PolygonBuilder.hpp>
#include <geos/util.hpp>
#include <geos/util/Interrupt.hpp>
#include <geos/util/TopologyException.hpp>
#include <memory> // for unique_ptr
#include <sstream>
#include <vector>

#ifndef GEOS_DEBUG
#define GEOS_DEBUG 0
#endif

#define COMPUTE_Z            1
#define USE_ELEVATION_MATRIX 1
#define USE_INPUT_AVGZ       0

// A result validator using FuzzyPointLocator to
// check validity of result. Pretty expensive...
// #define ENABLE_OVERLAY_RESULT_VALIDATOR 1

// Edge noding validator, more lightweighted and
// catches robustness errors earlier
#define ENABLE_EDGE_NODING_VALIDATOR 1

// Define this to get some reports about validations
// #define GEOS_DEBUG_VALIDATION 1

// Other validators, not found in JTS
// #define ENABLE_OTHER_OVERLAY_RESULT_VALIDATORS 1

using namespace geos::geom;
using namespace geos::geomgraph;
using namespace geos::algorithm;

namespace geos {
namespace operation { // geos.operation
namespace overlay {   // geos.operation.overlay

/* static public */
Geometry *OverlayOp::overlayOp(const Geometry *geom0, const Geometry *geom1, OverlayOp::OpCode opCode)
// throw(TopologyException *)
{
	OverlayOp gov(geom0, geom1);
	return gov.getResultGeometry(opCode);
}

Dimension::DimensionType OverlayOp::resultDimension(OverlayOp::OpCode overlayOpCode, const Geometry *g0,
                                                    const Geometry *g1) {
	Dimension::DimensionType dim0 = g0->getDimension();
	Dimension::DimensionType dim1 = g1->getDimension();

	Dimension::DimensionType resultDimension = Dimension::False;
	switch (overlayOpCode) {
	case OverlayOp::opINTERSECTION:
		resultDimension = std::min(dim0, dim1);
		break;
	case OverlayOp::opUNION:
		resultDimension = std::max(dim0, dim1);
		break;
	case OverlayOp::opDIFFERENCE:
		resultDimension = dim0;
		break;
	case OverlayOp::opSYMDIFFERENCE:
		resultDimension = std::max(dim0, dim1);
		break;
	}
	return resultDimension;
}

std::unique_ptr<geom::Geometry> OverlayOp::createEmptyResult(OverlayOp::OpCode overlayOpCode, const geom::Geometry *a,
                                                             const geom::Geometry *b, const GeometryFactory *geomFact) {
	std::unique_ptr<geom::Geometry> result = nullptr;
	switch (resultDimension(overlayOpCode, a, b)) {
	case Dimension::P:
		result = geomFact->createPoint();
		break;
	case Dimension::L:
		result = geomFact->createLineString();
		break;
	case Dimension::A:
		result = geomFact->createPolygon();
		break;
	default:
		result = geomFact->createGeometryCollection();
		break;
	}
	return result;
}

OverlayOp::OverlayOp(const Geometry *g0, const Geometry *g1)
    : // this builds graphs in arg[0] and arg[1]
      GeometryGraphOperation(g0, g1),
      /*
       * Use factory of primary geometry.
       * Note that this does NOT handle mixed-precision arguments
       * where the second arg has greater precision than the first.
       */
      geomFact(g0->getFactory()), resultGeom(nullptr), graph(OverlayNodeFactory::instance()), resultPolyList(nullptr),
      resultLineList(nullptr), resultPointList(nullptr) {

#if COMPUTE_Z
#if USE_INPUT_AVGZ
	avgz[0] = DoubleNotANumber;
	avgz[1] = DoubleNotANumber;
	avgzcomputed[0] = false;
	avgzcomputed[1] = false;
#endif // USE_INPUT_AVGZ

	Envelope env(*(g0->getEnvelopeInternal()));
	env.expandToInclude(g1->getEnvelopeInternal());
#if USE_ELEVATION_MATRIX
	elevationMatrix = new ElevationMatrix(env, 3, 3);
	elevationMatrix->add(g0);
	elevationMatrix->add(g1);
#endif // USE_ELEVATION_MATRIX
#endif // COMPUTE_Z
}

OverlayOp::~OverlayOp() {
	delete resultPolyList;
	delete resultLineList;
	delete resultPointList;
	for (std::size_t i = 0; i < dupEdges.size(); i++) {
		delete dupEdges[i];
	}
#if USE_ELEVATION_MATRIX
	delete elevationMatrix;
#endif
}

/*public*/
Geometry *OverlayOp::getResultGeometry(OverlayOp::OpCode funcCode)
// throw(TopologyException *)
{
	computeOverlay(funcCode);
	return resultGeom;
}

/*private*/
void OverlayOp::computeOverlay(OverlayOp::OpCode opCode)
// throw(TopologyException *)
{

	// Compute the target envelope
	const Envelope *env = nullptr;
	const Envelope *env0 = getArgGeometry(0)->getEnvelopeInternal();
	const Envelope *env1 = getArgGeometry(1)->getEnvelopeInternal();
	Envelope opEnv;
	if (resultPrecisionModel->isFloating()) {
		// Envelope-based optimization only works in floating precision
		switch (opCode) {
		case opINTERSECTION:
			env0->intersection(*env1, opEnv);
			env = &opEnv;
			break;
		case opDIFFERENCE:
			opEnv = *env0;
			env = &opEnv;
			break;
		default:
			break;
		}
	}
	// env = 0; // WARNING: uncomment to disable env-optimization

	// copy points from input Geometries.
	// This ensures that any Point geometries
	// in the input are considered for inclusion in the result set
	copyPoints(0, env);
	copyPoints(1, env);

	GEOS_CHECK_FOR_INTERRUPTS();

	// node the input Geometries
	arg[0]->computeSelfNodes(li, false, env);
	GEOS_CHECK_FOR_INTERRUPTS();
	arg[1]->computeSelfNodes(li, false, env);

	GEOS_CHECK_FOR_INTERRUPTS();

	// compute intersections between edges of the two input geometries
	arg[0]->computeEdgeIntersections(arg[1], &li, true, env);

	GEOS_CHECK_FOR_INTERRUPTS();

	std::vector<Edge *> baseSplitEdges;
	arg[0]->computeSplitEdges(&baseSplitEdges);
	GEOS_CHECK_FOR_INTERRUPTS();
	arg[1]->computeSplitEdges(&baseSplitEdges);

	GEOS_CHECK_FOR_INTERRUPTS();

	// add the noded edges to this result graph
	insertUniqueEdges(&baseSplitEdges, env);
	computeLabelsFromDepths();
	replaceCollapsedEdges();
	// Debug.println(edgeList);

	GEOS_CHECK_FOR_INTERRUPTS();

#ifdef ENABLE_EDGE_NODING_VALIDATOR // {
	/*
	 * Check that the noding completed correctly.
	 *
	 * This test is slow, but necessary in order to catch
	 * robustness failure situations.
	 * If an exception is thrown because of a noding failure,
	 * then snapping will be performed, which will hopefully avoid
	 * the problem.
	 * In the future hopefully a faster check can be developed.
	 *
	 */
	try {
		// Will throw TopologyException if noding is
		// found to be invalid
		EdgeNodingValidator::checkValid(edgeList.getEdges());
	} catch (const util::TopologyException &ex) {
		::geos::ignore_unused_variable_warning(ex);
		// In the error scenario, the edgeList is not properly
		// deleted. Cannot add to the destructor of EdgeList
		// (as it should) because
		// "graph.addEdges(edgeList.getEdges());" below
		// takes over edgeList ownership in the success case.
		edgeList.clearList();

		throw;
	}

#endif // ENABLE_EDGE_NODING_VALIDATOR }

	GEOS_CHECK_FOR_INTERRUPTS();

	graph.addEdges(edgeList.getEdges());

	GEOS_CHECK_FOR_INTERRUPTS();

	// this can throw TopologyException *
	computeLabelling();

	// Debug.printWatch();
	labelIncompleteNodes();
	// Debug.printWatch();
	// nodeMap.print(System.out);

	GEOS_CHECK_FOR_INTERRUPTS();

	/*
	 * The ordering of building the result Geometries is important.
	 * Areas must be built before lines, which must be built
	 * before points.
	 * This is so that lines which are covered by areas are not
	 * included explicitly, and similarly for points.
	 */
	findResultAreaEdges(opCode);
	cancelDuplicateResultEdges();

	GEOS_CHECK_FOR_INTERRUPTS();

	PolygonBuilder polyBuilder(geomFact);

	// might throw a TopologyException *
	polyBuilder.add(&graph);

	std::vector<Geometry *> *gv = polyBuilder.getPolygons();
	std::size_t gvsize = gv->size();
	resultPolyList = new std::vector<Polygon *>(gvsize);
	for (std::size_t i = 0; i < gvsize; ++i) {
		Polygon *p = dynamic_cast<Polygon *>((*gv)[i]);
		(*resultPolyList)[i] = p;
	}
	delete gv;

	LineBuilder lineBuilder(this, geomFact, &ptLocator);
	resultLineList = lineBuilder.build(opCode);

	PointBuilder pointBuilder(this, geomFact, &ptLocator);
	resultPointList = pointBuilder.build(opCode);

	// gather the results from all calculations into a single
	// Geometry for the result set
	resultGeom = computeGeometry(resultPointList, resultLineList, resultPolyList, opCode);

	checkObviouslyWrongResult(opCode);

#if USE_ELEVATION_MATRIX
	elevationMatrix->elevate(resultGeom);
#endif // USE_ELEVATION_MATRIX
}

/*private*/
void OverlayOp::computeLabelling()
// throw(TopologyException *) // and what else ?
{
	const auto &nodeMap = graph.getNodeMap()->nodeMap;

	for (const auto &nodeIt : nodeMap) {
		Node *node = nodeIt.second.get();
		node->getEdges()->computeLabelling(&arg);
	}
	mergeSymLabels();
	updateNodeLabelling();
}

/*private*/
void OverlayOp::updateNodeLabelling() {
	// update the labels for nodes
	// The label for a node is updated from the edges incident on it
	// (Note that a node may have already been labelled
	// because it is a point in one of the input geometries)

	const auto &nodeMap = graph.getNodeMap()->nodeMap;

	for (const auto &nodeIt : nodeMap) {
		Node *node = nodeIt.second.get();
		EdgeEndStar *ees = node->getEdges();
		DirectedEdgeStar *des = detail::down_cast<DirectedEdgeStar *>(ees);
		Label &lbl = des->getLabel();
		node->getLabel().merge(lbl);
	}
}

/*private*/
void OverlayOp::labelIncompleteNodes() {
	const auto &nodeMap = graph.getNodeMap()->nodeMap;

	for (const auto &nodeIt : nodeMap) {
		Node *n = nodeIt.second.get();
		const Label &label = n->getLabel();
		if (n->isIsolated()) {
			if (label.isNull(0)) {
				labelIncompleteNode(n, 0);
			} else {
				labelIncompleteNode(n, 1);
			}
		}
		// now update the labelling for the DirectedEdges incident on this node
		EdgeEndStar *ees = n->getEdges();
		DirectedEdgeStar *des = detail::down_cast<DirectedEdgeStar *>(ees);

		des->updateLabelling(label);
		//((DirectedEdgeStar*)n->getEdges())->updateLabelling(label);
		// n.print(System.out);
	}
}

/*public*/
bool OverlayOp::isCoveredByLA(const Coordinate &coord) {
	if (isCovered(coord, resultLineList)) {
		return true;
	}
	if (isCovered(coord, resultPolyList)) {
		return true;
	}
	return false;
}

/*private*/
bool OverlayOp::isCovered(const Coordinate &coord, std::vector<Geometry *> *geomList) {
	for (std::size_t i = 0, n = geomList->size(); i < n; ++i) {
		Geometry *geom = (*geomList)[i];
		Location loc = ptLocator.locate(coord, geom);
		if (loc != Location::EXTERIOR) {
			return true;
		}
	}
	return false;
}

/*private*/
bool OverlayOp::isCovered(const Coordinate &coord, std::vector<LineString *> *geomList) {
	for (std::size_t i = 0, n = geomList->size(); i < n; ++i) {
		Geometry *geom = (Geometry *)(*geomList)[i];
		Location loc = ptLocator.locate(coord, geom);
		if (loc != Location::EXTERIOR) {
			return true;
		}
	}
	return false;
}

/*private*/
bool OverlayOp::isCovered(const Coordinate &coord, std::vector<Polygon *> *geomList) {
	for (std::size_t i = 0, n = geomList->size(); i < n; ++i) {
		Geometry *geom = (Geometry *)(*geomList)[i];
		Location loc = ptLocator.locate(coord, geom);
		if (loc != Location::EXTERIOR) {
			return true;
		}
	}
	return false;
}

/*private*/
void OverlayOp::labelIncompleteNode(Node *n, uint8_t targetIndex) {
	const Geometry *targetGeom = arg[targetIndex]->getGeometry();
	Location loc = ptLocator.locate(n->getCoordinate(), targetGeom);
	n->getLabel().setLocation(targetIndex, loc);

#if COMPUTE_Z
	/*
	 * If this node has been labeled INTERIOR of a line
	 * or BOUNDARY of a polygon we must merge
	 * Z values of the intersected segment.
	 * The intersection point has been already computed
	 * by LineIntersector invoked by PointLocator.
	 */

	// Only do this if input does have Z
	// See https://trac.osgeo.org/geos/ticket/811
	if (targetGeom->getCoordinateDimension() < 3u) {
		return;
	}

	const LineString *line = dynamic_cast<const LineString *>(targetGeom);
	if (loc == Location::INTERIOR && line) {
		mergeZ(n, line);
	}
	const Polygon *poly = dynamic_cast<const Polygon *>(targetGeom);
	if (loc == Location::BOUNDARY && poly) {
		mergeZ(n, poly);
	}
#if USE_INPUT_AVGZ
	if (loc == Location::INTERIOR && poly) {
		n->addZ(getAverageZ(targetIndex));
	}
#endif // USE_INPUT_AVGZ
#endif // COMPUTE_Z
}

/*private*/
void OverlayOp::findResultAreaEdges(OverlayOp::OpCode opCode) {
	std::vector<EdgeEnd *> *ee = graph.getEdgeEnds();
	for (std::size_t i = 0, e = ee->size(); i < e; ++i) {
		DirectedEdge *de = (DirectedEdge *)(*ee)[i];
		// mark all dirEdges with the appropriate label
		const Label &label = de->getLabel();
		if (label.isArea() && !de->isInteriorAreaEdge() &&
		    isResultOfOp(label.getLocation(0, Position::RIGHT), label.getLocation(1, Position::RIGHT), opCode)) {
			de->setInResult(true);
			// Debug.print("in result "); Debug.println(de);
		}
	}
}

/*private*/
void OverlayOp::cancelDuplicateResultEdges() {
	// remove any dirEdges whose sym is also included
	// (they "cancel each other out")
	std::vector<EdgeEnd *> *ee = graph.getEdgeEnds();
	for (std::size_t i = 0, eesize = ee->size(); i < eesize; ++i) {
		DirectedEdge *de = static_cast<DirectedEdge *>((*ee)[i]);
		DirectedEdge *sym = de->getSym();
		if (de->isInResult() && sym->isInResult()) {
			de->setInResult(false);
			sym->setInResult(false);
			// Debug.print("cancelled "); Debug.println(de); Debug.println(sym);
		}
	}
}

/*private*/
void OverlayOp::copyPoints(uint8_t argIndex, const Envelope *env) {

	// TODO: set env to null if it covers arg geometry envelope

	const auto &nodeMap = arg[argIndex]->getNodeMap()->nodeMap;
	for (const auto &nodeIt : nodeMap) {
		Node *graphNode = nodeIt.second.get();
		assert(graphNode);
		const Coordinate &coord = graphNode->getCoordinate();

		if (env && !env->covers(&coord)) {
			continue;
		}
		Node *newNode = graph.addNode(coord);
		assert(newNode);

		newNode->setLabel(argIndex, graphNode->getLabel().getLocation(argIndex));
	}
}

/*private*/
void OverlayOp::insertUniqueEdges(std::vector<Edge *> *edges, const Envelope *env) {
	for (std::size_t i = 0, n = edges->size(); i < n; ++i) {
		Edge *e = (*edges)[i];
		if (env && !env->intersects(e->getEnvelope())) {
			dupEdges.push_back(e); // or could it be deleted directly ?
			continue;
		}
		insertUniqueEdge(e);
	}
}

/*protected*/
void OverlayOp::insertUniqueEdge(Edge *e) {
	//<FIX> MD 8 Oct 03  speed up identical edge lookup
	// fast lookup
	Edge *existingEdge = edgeList.findEqualEdge(e);

	// If an identical edge already exists, simply update its label
	if (existingEdge) {

		Label &existingLabel = existingEdge->getLabel();

		Label labelToMerge = e->getLabel();

		// check if new edge is in reverse direction to existing edge
		// if so, must flip the label before merging it
		if (!existingEdge->isPointwiseEqual(e)) {
			labelToMerge.flip();
		}
		Depth &depth = existingEdge->getDepth();

		// if this is the first duplicate found for this edge,
		// initialize the depths
		if (depth.isNull()) {
			depth.add(existingLabel);
		}

		depth.add(labelToMerge);

		existingLabel.merge(labelToMerge);

		// Debug.print("inserted edge: "); Debug.println(e);
		// Debug.print("existing edge: "); Debug.println(existingEdge);
		dupEdges.push_back(e);
	} else {
		// no matching existing edge was found
		// add this new edge to the list of edges in this graph
		// e.setName(name+edges.size());
		// e.getDepth().add(e.getLabel());
		edgeList.add(e);
	}
}

/*private*/
void OverlayOp::computeLabelsFromDepths() {
	for (auto &e : edgeList.getEdges()) {
		Label &lbl = e->getLabel();
		Depth &depth = e->getDepth();

		/*
		 * Only check edges for which there were duplicates,
		 * since these are the only ones which might
		 * be the result of dimensional collapses.
		 */
		if (depth.isNull()) {
			continue;
		}

		depth.normalize();
		for (uint8_t i = 0; i < 2; i++) {
			if (!lbl.isNull(i) && lbl.isArea() && !depth.isNull(i)) {
				/*
				 * if the depths are equal, this edge is the result of
				 * the dimensional collapse of two or more edges.
				 * It has the same location on both sides of the edge,
				 * so it has collapsed to a line.
				 */
				if (depth.getDelta(i) == 0) {
					lbl.toLine(i);
				} else {
					/*
					 * This edge may be the result of a dimensional collapse,
					 * but it still has different locations on both sides.  The
					 * label of the edge must be updated to reflect the resultant
					 * side locations indicated by the depth values.
					 */
					assert(!depth.isNull(i, Position::LEFT)); // depth of LEFT side has not been initialized
					lbl.setLocation(i, Position::LEFT, depth.getLocation(i, Position::LEFT));
					assert(!depth.isNull(i, Position::RIGHT)); // depth of RIGHT side has not been initialized
					lbl.setLocation(i, Position::RIGHT, depth.getLocation(i, Position::RIGHT));
				}
			}
		}
	}
}

/*private*/
Geometry *OverlayOp::computeGeometry(std::vector<Point *> *nResultPointList, std::vector<LineString *> *nResultLineList,
                                     std::vector<Polygon *> *nResultPolyList, OverlayOp::OpCode opCode) {
	std::size_t nPoints = nResultPointList->size();
	std::size_t nLines = nResultLineList->size();
	std::size_t nPolys = nResultPolyList->size();

	std::unique_ptr<std::vector<Geometry *>> geomList {new std::vector<Geometry *>()};
	geomList->reserve(nPoints + nLines + nPolys);

	// element geometries of the result are always in the order P,L,A
	geomList->insert(geomList->end(), nResultPointList->begin(), nResultPointList->end());

	geomList->insert(geomList->end(), nResultLineList->begin(), nResultLineList->end());

	geomList->insert(geomList->end(), nResultPolyList->begin(), nResultPolyList->end());

	if (geomList->empty()) {
		return createEmptyResult(opCode, arg[0]->getGeometry(), arg[1]->getGeometry(), geomFact).release();
	}
	// build the most specific geometry possible
	Geometry *g = geomFact->buildGeometry(geomList.release());
	return g;
}

/*private*/
void OverlayOp::replaceCollapsedEdges() {
	std::vector<Edge *> &edges = edgeList.getEdges();

	for (std::size_t i = 0, nedges = edges.size(); i < nedges; ++i) {
		Edge *e = edges[i];
		assert(e);
		if (e->isCollapsed()) {
			edges[i] = e->getCollapsedEdge();

			// should we keep this alive some more ?
			delete e;
		}
	}
}

/*private*/
void OverlayOp::mergeSymLabels() {
	const auto &nodeMap = graph.getNodeMap()->nodeMap;

	for (const auto &nodeIt : nodeMap) {
		Node *node = nodeIt.second.get();
		EdgeEndStar *ees = node->getEdges();
		detail::down_cast<DirectedEdgeStar *>(ees)->mergeSymLabels();
		//((DirectedEdgeStar*)node->getEdges())->mergeSymLabels();
		// node.print(System.out);
	}
}

void OverlayOp::checkObviouslyWrongResult(OverlayOp::OpCode opCode) {
	using std::cerr;
	using std::endl;

	assert(resultGeom);

#ifdef ENABLE_OTHER_OVERLAY_RESULT_VALIDATORS

	if (opCode == opINTERSECTION && arg[0]->getGeometry()->getDimension() == Dimension::A &&
	    arg[1]->getGeometry()->getDimension() == Dimension::A) {
		// Area of result must be less or equal area of smaller geom
		double area0 = arg[0]->getGeometry()->getArea();
		double area1 = arg[1]->getGeometry()->getArea();
		double minarea = min(area0, area1);
		double resultArea = resultGeom->getArea();

		if (resultArea > minarea) {
			throw util::TopologyException("Obviously wrong result: "
			                              "area of intersection "
			                              "result is bigger then minimum area between "
			                              "input geometries");
		}
	}

	else if (opCode == opDIFFERENCE && arg[0]->getGeometry()->getDimension() == Dimension::A &&
	         arg[1]->getGeometry()->getDimension() == Dimension::A) {
		// Area of result must be less or equal area of first geom
		double area0 = arg[0]->getGeometry()->getArea();
		double resultArea = resultGeom->getArea();

		if (resultArea > area0) {
			throw util::TopologyException("Obviously wrong result: "
			                              "area of difference "
			                              "result is bigger then area of first "
			                              "input geometry");
		}

		// less obvious check:
		// each vertex in first geom must be either covered by
		// result or second geom
		std::vector<const Geometry *> testGeoms;
		testGeoms.reserve(2);
		testGeoms.push_back(resultGeom);
		testGeoms.push_back(arg[1]->getGeometry());
		PointCoveredByAny tester(testGeoms);
		arg[0]->getGeometry()->apply_ro(&tester);
	}

	// Add your tests here

#else
	::geos::ignore_unused_variable_warning(opCode);
#endif

#ifdef ENABLE_OVERLAY_RESULT_VALIDATOR
	// This only work for FLOATING precision
	if (resultPrecisionModel->isFloating()) {
		validate::OverlayResultValidator validator(*(arg[0]->getGeometry()), *(arg[1]->getGeometry()), *(resultGeom));
		bool isvalid = validator.isValid(opCode);
		if (!isvalid) {
			throw util::TopologyException("Obviously wrong result: "
			                              "OverlayResultValidator didn't like "
			                              "the result: \n"
			                              "Invalid point: " +
			                              validator.getInvalidLocation().toString() +
			                              std::string("\nInvalid result: ") + resultGeom->toString());
		}
	}
#endif
}

/*private*/
int OverlayOp::mergeZ(Node *n, const Polygon *poly) const {
	const LineString *ls;
	int found = 0;
	ls = poly->getExteriorRing();
	found = mergeZ(n, ls);
	if (found) {
		return 1;
	}
	for (std::size_t i = 0, nr = poly->getNumInteriorRing(); i < nr; ++i) {
		ls = poly->getInteriorRingN(i);
		found = mergeZ(n, ls);
		if (found) {
			return 1;
		}
	}
	return 0;
}

/*private*/
int OverlayOp::mergeZ(Node *n, const LineString *line) const {
	const CoordinateSequence *pts = line->getCoordinatesRO();
	const Coordinate &p = n->getCoordinate();
	LineIntersector p_li;
	for (std::size_t i = 1, size = pts->size(); i < size; ++i) {
		const Coordinate &p0 = pts->getAt(i - 1);
		const Coordinate &p1 = pts->getAt(i);
		p_li.computeIntersection(p, p0, p1);
		if (p_li.hasIntersection()) {
			if (p == p0) {
				n->addZ(p0.z);
			} else if (p == p1) {
				n->addZ(p1.z);
			} else {
				n->addZ(LineIntersector::interpolateZ(p, p0, p1));
			}
			return 1;
		}
	}
	return 0;
}

/*public*/
bool OverlayOp::isCoveredByA(const Coordinate &coord) {
	if (isCovered(coord, resultPolyList)) {
		return true;
	}
	return false;
}

/* static public */
bool OverlayOp::isResultOfOp(const Label &label, OverlayOp::OpCode opCode) {
	Location loc0 = label.getLocation(0);
	Location loc1 = label.getLocation(1);
	return isResultOfOp(loc0, loc1, opCode);
}

/* static public */
bool OverlayOp::isResultOfOp(Location loc0, Location loc1, OverlayOp::OpCode opCode) {
	if (loc0 == Location::BOUNDARY) {
		loc0 = Location::INTERIOR;
	}
	if (loc1 == Location::BOUNDARY) {
		loc1 = Location::INTERIOR;
	}
	switch (opCode) {
	case opINTERSECTION:
		return loc0 == Location::INTERIOR && loc1 == Location::INTERIOR;
	case opUNION:
		return loc0 == Location::INTERIOR || loc1 == Location::INTERIOR;
	case opDIFFERENCE:
		return loc0 == Location::INTERIOR && loc1 != Location::INTERIOR;
	case opSYMDIFFERENCE:
		return (loc0 == Location::INTERIOR && loc1 != Location::INTERIOR) ||
		       (loc0 != Location::INTERIOR && loc1 == Location::INTERIOR);
	}
	return false;
}

} // namespace overlay
} // namespace operation
} // namespace geos
