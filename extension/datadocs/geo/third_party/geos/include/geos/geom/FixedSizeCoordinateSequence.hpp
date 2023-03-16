/**********************************************************************
 *
 * GEOS - Geometry Engine Open Source
 * http://geos.osgeo.org
 *
 * Copyright (C) 2019 Daniel Baston
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU Lesser General Public Licence as published
 * by the Free Software Foundation.
 * See the COPYING file for more information.
 *
 **********************************************************************/

#pragma once

#include <algorithm>
#include <array>
#include <geos/geom/Coordinate.hpp>
#include <geos/geom/CoordinateFilter.hpp>
#include <geos/geom/CoordinateSequence.hpp>
#include <geos/util.hpp>
#include <geos/util/IllegalArgumentException.hpp>
#include <memory>
#include <sstream>
#include <vector>

namespace geos {
namespace geom {

template <size_t N>
class FixedSizeCoordinateSequence : public CoordinateSequence {

public:
	explicit FixedSizeCoordinateSequence(std::size_t dimension_in = 0) : dimension(dimension_in) {
	}

	std::unique_ptr<CoordinateSequence> clone() const final override {
		auto seq = detail::make_unique<FixedSizeCoordinateSequence<N>>(dimension);
		seq->m_data = m_data;
		return seq;
	}

	const Coordinate &getAt(std::size_t i) const final override {
		return m_data[i];
	}

	Coordinate &getAt(std::size_t i) final override {
		return m_data[i];
	}

	void getAt(std::size_t i, Coordinate &c) const final override {
		c = m_data[i];
	}

	void setAt(const Coordinate &c, std::size_t pos) final override {
		m_data[pos] = c;
	}

	void setOrdinate(std::size_t index, std::size_t ordinateIndex, double value) final override {
		switch (ordinateIndex) {
		case CoordinateSequence::X:
			m_data[index].x = value;
			break;
		case CoordinateSequence::Y:
			m_data[index].y = value;
			break;
		case CoordinateSequence::Z:
			m_data[index].z = value;
			break;
		default: {
			std::stringstream ss;
			ss << "Unknown ordinate index " << ordinateIndex;
			throw geos::util::IllegalArgumentException(ss.str());
			break;
		}
		}
	}

	std::size_t getSize() const final override {
		return N;
	}

	bool isEmpty() const final override {
		return N == 0;
	}

	std::size_t getDimension() const final override {
		if (dimension != 0) {
			return dimension;
		}

		if (isEmpty()) {
			return 3;
		}

		if (std::isnan(m_data[0].z)) {
			dimension = 2;
		} else {
			dimension = 3;
		}

		return dimension;
	}

	void toVector(std::vector<Coordinate> &out) const final override {
		out.insert(out.end(), m_data.begin(), m_data.end());
	}

	void toVector(std::vector<CoordinateXY> &out) const final override {
		for (const auto &pt : m_data) {
			out.push_back(pt);
		}
	}

	void apply_ro(CoordinateFilter *filter) const final override {
		std::for_each(m_data.begin(), m_data.end(), [&filter](const Coordinate &c) { filter->filter_ro(&c); });
	}

	void apply_rw(const CoordinateFilter *filter) final override {
		std::for_each(m_data.begin(), m_data.end(), [&filter](Coordinate &c) { filter->filter_rw(&c); });
		dimension = 0; // re-check (see http://trac.osgeo.org/geos/ticket/435)
	}

private:
	std::array<Coordinate, N> m_data;
	mutable std::size_t dimension;
};

} // namespace geom
} // namespace geos
