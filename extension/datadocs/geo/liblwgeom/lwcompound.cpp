#include "liblwgeom/liblwgeom_internal.hpp"
#include "liblwgeom/lwinline.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace duckdb {

int lwcompound_is_closed(const LWCOMPOUND *compound) {
	size_t size;
	int npoints = 0;

	if (lwgeom_has_z((LWGEOM *)compound)) {
		size = sizeof(POINT3D);
	} else {
		size = sizeof(POINT2D);
	}

	if (compound->geoms[compound->ngeoms - 1]->type == CIRCSTRINGTYPE) {
		npoints = ((LWCIRCSTRING *)compound->geoms[compound->ngeoms - 1])->points->npoints;
	} else if (compound->geoms[compound->ngeoms - 1]->type == LINETYPE) {
		npoints = ((LWLINE *)compound->geoms[compound->ngeoms - 1])->points->npoints;
	}

	if (memcmp(getPoint_internal((POINTARRAY *)compound->geoms[0]->data, 0),
	           getPoint_internal((POINTARRAY *)compound->geoms[compound->ngeoms - 1]->data, npoints - 1), size)) {
		return LW_FALSE;
	}

	return LW_TRUE;
}

int lwcompound_add_lwgeom(LWCOMPOUND *comp, LWGEOM *geom) {
	LWCOLLECTION *col = (LWCOLLECTION *)comp;

	/* Empty things can't continuously join up with other things */
	if (lwgeom_is_empty(geom)) {
		return LW_FAILURE;
	}

	if (col->ngeoms > 0) {
		POINT4D last, first;
		/* First point of the component we are adding */
		LWLINE *newline = (LWLINE *)geom;
		/* Last point of the previous component */
		LWLINE *prevline = (LWLINE *)(col->geoms[col->ngeoms - 1]);

		getPoint4d_p(newline->points, 0, &first);
		getPoint4d_p(prevline->points, prevline->points->npoints - 1, &last);

		if (!(FP_EQUALS(first.x, last.x) && FP_EQUALS(first.y, last.y))) {
			return LW_FAILURE;
		}
	}

	col = lwcollection_add_lwgeom(col, geom);
	return LW_SUCCESS;
}

LWPOINT *lwcompound_get_endpoint(const LWCOMPOUND *lwcmp) {
	LWLINE *lwline;
	if (lwcmp->ngeoms < 1) {
		return NULL;
	}

	lwline = (LWLINE *)(lwcmp->geoms[lwcmp->ngeoms - 1]);

	if ((!lwline) || (!lwline->points) || (lwline->points->npoints < 1)) {
		return NULL;
	}

	return lwline_get_lwpoint(lwline, lwline->points->npoints - 1);
}

LWPOINT *lwcompound_get_lwpoint(const LWCOMPOUND *lwcmp, uint32_t where) {
	uint32_t i;
	uint32_t count = 0;
	uint32_t npoints = 0;
	if (lwgeom_is_empty((LWGEOM *)lwcmp))
		return nullptr;

	npoints = lwgeom_count_vertices((LWGEOM *)lwcmp);
	if (where >= npoints) {
		lwerror("%s: index %d is not in range of number of vertices (%d) in input", __func__, where, npoints);
		return nullptr;
	}

	for (i = 0; i < lwcmp->ngeoms; i++) {
		LWGEOM *part = lwcmp->geoms[i];
		uint32_t npoints_part = lwgeom_count_vertices(part);
		if (where >= count && where < count + npoints_part) {
			return lwline_get_lwpoint((LWLINE *)part, where - count);
		} else {
			count += npoints_part;
		}
	}

	return nullptr;
}

LWPOINT *lwcompound_get_startpoint(const LWCOMPOUND *lwcmp) {
	return lwcompound_get_lwpoint(lwcmp, 0);
}

int lwgeom_contains_point(const LWGEOM *geom, const POINT2D *pt) {
	switch (geom->type) {
	case LINETYPE:
		return ptarray_contains_point(((LWLINE *)geom)->points, pt);
	case CIRCSTRINGTYPE:
		return ptarrayarc_contains_point(((LWCIRCSTRING *)geom)->points, pt);
	case COMPOUNDTYPE:
		return lwcompound_contains_point((LWCOMPOUND *)geom, pt);
	}
	lwerror("lwgeom_contains_point failed");
	return LW_FAILURE;
}

int lwcompound_contains_point(const LWCOMPOUND *comp, const POINT2D *pt) {
	uint32_t i;
	LWLINE *lwline;
	LWCIRCSTRING *lwcirc;
	int wn = 0;
	int winding_number = 0;
	int result;

	for (i = 0; i < comp->ngeoms; i++) {
		LWGEOM *lwgeom = comp->geoms[i];
		if (lwgeom->type == LINETYPE) {
			lwline = lwgeom_as_lwline(lwgeom);
			if (comp->ngeoms == 1) {
				return ptarray_contains_point(lwline->points, pt);
			} else {
				/* Don't check closure while doing p-i-p test */
				result = ptarray_contains_point_partial(lwline->points, pt, LW_FALSE, &winding_number);
			}
		} else {
			lwcirc = lwgeom_as_lwcircstring(lwgeom);
			if (!lwcirc) {
				lwerror("Unexpected component of type %s in compound curve", lwtype_name(lwgeom->type));
				return 0;
			}
			if (comp->ngeoms == 1) {
				return ptarrayarc_contains_point(lwcirc->points, pt);
			} else {
				/* Don't check closure while doing p-i-p test */
				result = ptarrayarc_contains_point_partial(lwcirc->points, pt, LW_FALSE, &winding_number);
			}
		}

		/* Propogate boundary condition */
		if (result == LW_BOUNDARY)
			return LW_BOUNDARY;

		wn += winding_number;
	}

	/* Outside */
	if (wn == 0)
		return LW_OUTSIDE;

	/* Inside */
	return LW_INSIDE;
}

double lwcompound_length_2d(const LWCOMPOUND *comp) {
	uint32_t i;
	double length = 0.0;
	if (lwgeom_is_empty((LWGEOM *)comp))
		return 0.0;

	for (i = 0; i < comp->ngeoms; i++) {
		length += lwgeom_length_2d(comp->geoms[i]);
	}
	return length;
}

} // namespace duckdb
