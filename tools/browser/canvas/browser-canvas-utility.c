/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "browser-canvas-utility.h"
#include <math.h>
#include <string.h>

static gboolean compute_intersect_rect_line (gdouble rectx1, gdouble recty1, gdouble rectx2, gdouble recty2,
					     gdouble P1x, gdouble P1y, gdouble P2x, gdouble P2y,
					     gdouble *R1x, gdouble *R1y, gdouble *R2x, gdouble *R2y);

static void     compute_text_marks_offsets (gdouble x1, gdouble y1, gdouble x2, gdouble y2,
					    gdouble *xoff, gdouble *yoff, GooCanvasAnchorType *anchor_type);

static GSList *browser_canvas_canvas_shape_add_to_list (GSList *list, gchar *swallow_id, GooCanvasItem *item);
static BrowserCanvasCanvasShape *browser_canvas_canvas_shape_find (GSList *list, const gchar *id);


/*
 * Computes the points' coordinates of the line going from
 * @ref_pk_ent to @fk_ent (which are themselves rectangles)
 *
 * if @shapes is not NULL, then the shapes in the list are reused, and the ones which don't need
 * to exist anymore are removed
 *
 * Returns a list of BrowserCanvasCanvasShapes structures
 */
GSList *
browser_canvas_util_compute_anchor_shapes (GooCanvasItem *parent, GSList *shapes,
					   BrowserCanvasTable *fk_ent, BrowserCanvasTable *ref_pk_ent, 
					   guint nb_anchors, guint ext, gboolean with_handle)
{
	GSList *retval = shapes;
	guint i;
	gdouble fx1, fy1, fx2, fy2; /* FK entity item (bounds) */
	gdouble rx1, ry1, rx2, ry2; /* REF PK entity item  (bounds) */

	BrowserCanvasCanvasShape *shape;
	gchar *id;

	gdouble rcx, rcy; /* center of ref_pk entity item */
	gdouble cx, cy;

	gdouble rux, ruy; /* current ref_pk point for the arrow line */
	gdouble dx, dy; /* increments to compute the new ref_pk point for the arrow line */
	GooCanvasBounds bounds;

	g_return_val_if_fail (nb_anchors > 0, NULL);

	browser_canvas_table_get_anchor_bounds (fk_ent, &bounds);
	fx1 = bounds.x1;
	fy1 = bounds.y1;
	fx2 = bounds.x2;
	fy2 = bounds.y2;
	browser_canvas_table_get_anchor_bounds (ref_pk_ent, &bounds);
	rx1 = bounds.x1;
	ry1 = bounds.y1;
	rx2 = bounds.x2;
	ry2 = bounds.y2;

	/* compute the cx, cy, dx and dy values */
	rcx = (rx1 + rx2) / 2.;
	rcy = (ry1 + ry2) / 2.;
	cx = (fx1 + fx2) / 2.;
	cy = (fy1 + fy2) / 2.;
	rux = rcx;
	ruy = rcy;
	dx = 0;
	dy = 0;

	for (i = 0; i < nb_anchors; i++) {
		/* TODO:
		   - detect tables overlapping
		*/		
		if ((rcx == cx) && (rcy == cy)) {
			/* tables have the same center (includes case when they are equal) */
			gdouble Dy, Dx;
			GooCanvasPoints *ap, *points;
			GooCanvasItem *item;
			
			points = goo_canvas_points_new (4);
			ap = goo_canvas_points_new (4);

			Dy = (ry2 - ry1) / 2. / (gdouble ) (nb_anchors + 1) * (gdouble) (i + 1);
			Dx = (rx2 - rx1) * (0.8 + 0.1 * i);
			if (! compute_intersect_rect_line (rx1, ry1, rx2, ry2,
							   cx, cy, cx + Dx, cy - Dy,
							   &(ap->coords[0]), &(ap->coords[1]),
							   &(ap->coords[2]), &(ap->coords[3])))
				return retval;
			
			if (ap->coords[0] > ap->coords[2]) {
				points->coords[0] = ap->coords[0];
				points->coords[1] = ap->coords[1];
			}
			else {
				points->coords[0] = ap->coords[2];
				points->coords[1] = ap->coords[3];
			}

			points->coords[2] = cx + Dx;
			points->coords[3] = cy - Dy;

			Dy = (fy2 - fy1) / 2. / (gdouble ) (nb_anchors + 1) * (gdouble) (i + 1);
			Dx = (fx2 - fx1) * (0.8 + 0.1 * i);
			points->coords[4] = cx + Dx;
			points->coords[5] = cy + Dy;

			if (! compute_intersect_rect_line (fx1, fy1, fx2, fy2,
							   cx, cy, cx + Dx, cy + Dy,
							   &(ap->coords[0]), &(ap->coords[1]),
							   &(ap->coords[2]), &(ap->coords[3])))
				return retval;
			
			if (ap->coords[0] > ap->coords[2]) {
				points->coords[6] = ap->coords[0];
				points->coords[7] = ap->coords[1];
			}
			else {
				points->coords[6] = ap->coords[2];
				points->coords[7] = ap->coords[3];
			}
			
			id = g_strdup_printf ("a%d", i);
			shape = browser_canvas_canvas_shape_find (retval, id);
			if (shape) {
				g_object_set (shape->item, "points", points, NULL);
				shape->_used = TRUE;
				g_free (id);
			}
			else {
				item = goo_canvas_polyline_new_line (parent, 
								     points->coords[0], points->coords [1],
								     points->coords[2], points->coords [3],
								     "close-path", FALSE,
								     "points", points, NULL);
				retval = browser_canvas_canvas_shape_add_to_list (retval, id, item);
			}
			goo_canvas_points_unref (ap);
			
			/* extension marks as text */
			if (ext & CANVAS_SHAPE_EXT_JOIN_OUTER_1) {
				id = g_strdup_printf ("a%de1", i);
				shape = browser_canvas_canvas_shape_find (retval, id);
				if (shape) {
					g_object_set (shape->item, 
						      "x", points->coords[2] + 5.,
						      "y", points->coords[3] - 5., NULL);
					shape->_used = TRUE;
					g_free (id);
				}
				else {
					item = goo_canvas_text_new (parent, "*", 
								    points->coords[2] + 5.,
								    points->coords[3] - 5., -1,
								    GOO_CANVAS_ANCHOR_SOUTH, NULL);
					retval = browser_canvas_canvas_shape_add_to_list (retval, id, item);
				}
			}

			if (ext & CANVAS_SHAPE_EXT_JOIN_OUTER_2) {
				id = g_strdup_printf ("a%de2", i);
				if (shape) {
					g_object_set (shape->item, 
						      "x", points->coords[4] + 5.,
						      "y", points->coords[5] + 5., NULL);
					shape->_used = TRUE;
					g_free (id);
				}
				else {
					item = goo_canvas_text_new (parent, "*", 
								    points->coords[4] + 5.,
								    points->coords[5] + 5., -1,
								    GOO_CANVAS_ANCHOR_NORTH, NULL);
					retval = browser_canvas_canvas_shape_add_to_list (retval, id, item);
				}
			}
			
			goo_canvas_points_unref (points);
		}
		else {
			GooCanvasPoints *ap, *points;
			GooCanvasItem *item;

			points = goo_canvas_points_new (2);
			ap = goo_canvas_points_new (4);

			if (nb_anchors > 1) {
				if ((dx == 0) && (dy == 0)) {
					/* compute perpendicular to D={(rcx, rcy), (cx, cy)} */
					gdouble vx = (rcx - cx), vy = (rcy - cy);
					gdouble tmp;
					
					tmp = vx;
					vx = vy;
					vy = - tmp;
					
					/* compute intersect of ref_pkey rectangle and D=[vx, vy] passing at (rcx, rcy) */
					if (! compute_intersect_rect_line (rx1, ry1, rx2, ry2,
									   rcx, rcy, rcx + vx, rcy + vy,
									   &(ap->coords[0]), &(ap->coords[1]),
									   &(ap->coords[2]), &(ap->coords[3])))
						return retval;
					dx = (ap->coords[2] - ap->coords[0]) / (gdouble) (nb_anchors  + 1);
					dy = (ap->coords[3] - ap->coords[1]) / (gdouble) (nb_anchors  + 1);
					rux = ap->coords[0];
					ruy = ap->coords[1];
				}

				rux += dx;
				ruy += dy;
			}
			
			/* compute the 4 intersection points */
			if (! compute_intersect_rect_line (rx1, ry1, rx2, ry2,
							   rux, ruy, cx, cy,
							   &(ap->coords[0]), &(ap->coords[1]),
							   &(ap->coords[2]), &(ap->coords[3])))
				return retval;
			if (! compute_intersect_rect_line (fx1, fy1, fx2, fy2,
							   rux, ruy, cx, cy,
							   &(ap->coords[4]), &(ap->coords[5]),
							   &(ap->coords[6]), &(ap->coords[7])))
				return retval;
			
			/* choosing between point coords(0,1) and coords(2,3) */
			if (((ap->coords[0] - ap->coords[4]) * (ap->coords[0] - ap->coords[4]) + 
			     (ap->coords[1] - ap->coords[5]) * (ap->coords[1] - ap->coords[5])) <
			    ((ap->coords[2] - ap->coords[4]) * (ap->coords[2] - ap->coords[4]) + 
			     (ap->coords[3] - ap->coords[5]) * (ap->coords[3] - ap->coords[5]))) {
				points->coords[0] = ap->coords[0];
				points->coords[1] = ap->coords[1];
			}
			else {
				points->coords[0] = ap->coords[2];
				points->coords[1] = ap->coords[3];
			}
			
			/* choosing between point coords(4,5) and coords(6,7) */
			if (((points->coords[0] - ap->coords[4]) * (points->coords[0] - ap->coords[4]) +
			     (points->coords[1] - ap->coords[5]) * (points->coords[1] - ap->coords[5])) <
			    ((points->coords[0] - ap->coords[6]) * (points->coords[0] - ap->coords[6]) +
			     (points->coords[1] - ap->coords[7]) * (points->coords[1] - ap->coords[7]))) {
				points->coords[2] = ap->coords[4];
				points->coords[3] = ap->coords[5];
			}
			else {
				points->coords[2] = ap->coords[6];
				points->coords[3] = ap->coords[7];
			}
			
			id = g_strdup_printf ("a%d", i);
			shape = browser_canvas_canvas_shape_find (retval, id);
			if (shape) {
				g_object_set (shape->item, "points", points, NULL);
				shape->_used = TRUE;
				g_free (id);
			}
			else {
				item = goo_canvas_polyline_new_line (parent, 
								     points->coords[0], points->coords [1],
								     points->coords[2], points->coords [3],
								     "close-path", FALSE,
								     "points", points, NULL);
				retval = browser_canvas_canvas_shape_add_to_list (retval, id, item);
			}
			goo_canvas_points_unref (ap);

			/* extension marks as text */
			if (ext & CANVAS_SHAPE_EXT_JOIN_OUTER_1) {
				gdouble mxoff = 0., myoff = 0.;
				GooCanvasAnchorType atype;

				compute_text_marks_offsets (points->coords[0], points->coords[1], 
							    points->coords[2], points->coords[3],
							    &mxoff, &myoff, &atype);
				id = g_strdup_printf ("a%de1", i);
				shape = browser_canvas_canvas_shape_find (retval, id);
				if (shape) {
					g_object_set (shape->item, 
						      "x", points->coords[2] + mxoff,
						      "y", points->coords[3] + myoff, 
						      "anchor", atype, NULL);
					shape->_used = TRUE;
					g_free (id);
				}
				else {
					item = goo_canvas_text_new (parent, "*", 
								    points->coords[2] + mxoff,
								    points->coords[3] + myoff, -1,
								    atype, NULL);
					retval = browser_canvas_canvas_shape_add_to_list (retval, id, item);
				}
			}

			if (ext & CANVAS_SHAPE_EXT_JOIN_OUTER_2) {
				gdouble mxoff, myoff;
				GooCanvasAnchorType atype;
				
				compute_text_marks_offsets (points->coords[2], points->coords[3], 
							    points->coords[0], points->coords[1],
							    &mxoff, &myoff, &atype);

				id = g_strdup_printf ("a%de2", i);
				if (shape) {
					g_object_set (shape->item, 
						      "x", points->coords[0] + mxoff,
						      "y", points->coords[1] + myoff, 
						      "anchor", atype, NULL);
					shape->_used = TRUE;
					g_free (id);
				}
				else {
					item = goo_canvas_text_new (parent, "*", 
								    points->coords[0] + mxoff,
								    points->coords[1] + myoff, -1,
								    atype, NULL);
					retval = browser_canvas_canvas_shape_add_to_list (retval, id, item);
				}
			}

			goo_canvas_points_unref (points);
		}
	}

	return retval;
}

/*
 * Computes the position offsets, relative to X2=(x2, y2) of a text to be written
 * close to the X2 point, also computes the anchor type
 */
static void
compute_text_marks_offsets (gdouble x1, gdouble y1, gdouble x2, gdouble y2,
			    gdouble *xoff, gdouble *yoff, GooCanvasAnchorType *anchor_type)
{
	gdouble mxoff, myoff;
	GooCanvasAnchorType atype = GOO_CANVAS_ANCHOR_CENTER; /* FIXME */
	gdouble sint, cost;
	gdouble sina = 0.5;
	gdouble cosa = 0.866025; /* sqrt(3)/2 */
	gdouble hyp;
	gdouble d = 15.;

	hyp = sqrt ((y2 - y1) * (y2 - y1) + (x2 - x1) * (x2 - x1));
	sint = - (y2 - y1) / hyp;
	cost = (x2 - x1) / hyp;
	
	mxoff = -d * (sina * sint + cosa * cost);
	myoff = -d * (sina * cost - cosa * sint);

	if (xoff)
		*xoff = mxoff;
	if (yoff)
		*yoff = myoff;
	if (anchor_type)
		*anchor_type = atype;
}

/*
 * Computes the points of intersection between a rectangle (defined by the first 2 points)
 * and a line (defined by the next 2 points).
 *
 * The result is returned in place of the line's point definition
 *
 *             --------- -----   D1
 *             |       |
 *             |       |
 *             |       |
 *             |       |
 *             |       |
 *             |       |
 *             |       |
 *             --------- ----   D2
 *             
 *             |       |
 *             |       |
 *             D3      D4
 *
 * Returns: TRUE if the line crosses the rectangle, and FALSE if it misses it.
 */
static gboolean
compute_intersect_rect_line (gdouble rectx1, gdouble recty1, gdouble rectx2, gdouble recty2,
			     gdouble P1x, gdouble P1y, gdouble P2x, gdouble P2y,
			     gdouble *R1x, gdouble *R1y, gdouble *R2x, gdouble *R2y)
{
	gboolean retval = FALSE;
	gboolean rotated = FALSE;
	gdouble a=0.; /* line slope   y = a x + b */
	gdouble b;    /* line offset  y = a x + b */
	gdouble offset = 2.;
		
	gdouble ptsx[4]; /* points' X coordinate: 0 for intersect with D1, 1 for D2,... */
	gdouble ptsy[4]; /* points' Y coordinate */

	if ((rectx1 == rectx2) && (recty1 == recty2))
		return FALSE;
	if ((rectx1 >= rectx2) || (recty1 >= recty2))
		return FALSE;
	if ((P1x == P2x) && (P1y == P2y))
		return FALSE;

	/* rotate the coordinates to invert X and Y to avoid rounding problems ? */
	if (P1x != P2x)
		a = (P1y - P2y) / (P1x - P2x);
	if ((P1x == P2x) || (fabs (a) > 1)) {
		gdouble tmp;
		rotated = TRUE;
		tmp = rectx1; rectx1 = recty1; recty1 = tmp;
		tmp = rectx2; rectx2 = recty2; recty2 = tmp;
		tmp = P1x; P1x = P1y; P1y = tmp;
		tmp = P2x; P2x = P2y; P2y = tmp;
		a = (P1y - P2y) / (P1x - P2x);
	}

	/* here we have (P1x != P2x), non vertical line */
	b = P1y - a * P1x;

	if (a == 0) {
		/* horizontal line */

		if ((b <= recty2) && (b >= recty1)) {
			retval = TRUE;
			*R1x = rectx1 - offset; *R1y = b;
			*R2x = rectx2 + offset; *R2y = b;
		}
	}
	else {
		gdouble retx[2] = {0., 0.};
		gdouble rety[2] = {0., 0.};
		gint i = 0;

		/* non horizontal and non vertical line */
		/* D1 */
		ptsy[0] = recty1 - offset;
		ptsx[0] = (recty1 - b) / a;

		/* D2 */
		ptsy[1] = recty2 + offset;
		ptsx[1] = (recty2 - b) / a;

		/* D3 */
		ptsx[2] = rectx1 - offset;
		ptsy[2] = a * rectx1 + b;

		/* D4 */
		ptsx[3] = rectx2 + offset;
		ptsy[3] = a * rectx2 + b;
		
		if ((ptsx[0] >= rectx1) && (ptsx[0] <= rectx2)) {
			retval = TRUE;
			retx[i] = ptsx[0]; rety[i] = ptsy[0];
			i ++;
		}
		if ((ptsx[1] >= rectx1) && (ptsx[1] <= rectx2)) {
			retval = TRUE;
			retx[i] = ptsx[1]; rety[i] = ptsy[1];
			i ++;
		}
		if ((i<2) && (ptsy[2] >= recty1) && (ptsy[2] <= recty2)) {
			retval = TRUE;
			retx[i] = ptsx[2]; rety[i] = ptsy[2];
			i ++;
		}
		if ((i<2) && (ptsy[3] >= recty1) && (ptsy[3] <= recty2)) {
			retval = TRUE;
			retx[i] = ptsx[3]; rety[i] = ptsy[3];
			i++;
		}

		if (retval) {
			g_assert (i == 2); /* wee need 2 points! */
			*R1x = retx[0]; *R1y = rety[0];
			*R2x = retx[1]; *R2y = rety[1];
		}
	}

	if (retval && rotated) {
		/* rotate it back */
		gdouble tmp;

		tmp = *R1x; *R1x = *R1y; *R1y = tmp;
		tmp = *R2x; *R2x = *R2y; *R2y = tmp;
	}

	return retval;
}

/*
 * Compute the anchor shapes to link field1 to field2 from ent1 to ent2
 *
 * if @shapes is not NULL, then the shapes in the list are reused, and the ones which don't need
 * to exist anymore are removed
 *
 * Returns a list of BrowserCanvasCanvasShapes structures
 */
GSList *
browser_canvas_util_compute_connect_shapes (GooCanvasItem *parent, GSList *shapes,
					  BrowserCanvasTable *ent1, GdaMetaTableColumn *field1, 
					  BrowserCanvasTable *ent2, GdaMetaTableColumn *field2,
					  guint nb_connect, guint ext)
{
	GSList *retval = shapes;
	GooCanvasItem *item;
	GooCanvasPoints *points;
	gdouble xl1, xr1, xl2, xr2, yt1, yt2; /* X boundings and Y top of ent1 and ent2 */
	gdouble x1, x2; /* X positions of the lines extremities close to ent1 and ent2 */
	gdouble x1offset, x2offset; /* offsets for the horizontal part of the lines */
	double sq = 5.;
	double eps = 0.5;
	GooCanvasBounds bounds;

	BrowserCanvasCanvasShape *shape;
	gchar *id;

	if (!field1 || !field2)
		return browser_canvas_util_compute_anchor_shapes (parent, shapes, ent1, ent2, 1, ext, FALSE);

	/* line made of 4 points */
	points = goo_canvas_points_new (4);
	browser_canvas_table_get_anchor_bounds (ent1, &bounds);
	xl1 = bounds.x1;
	yt1 = bounds.y1;
	xr1 = bounds.x2;
	browser_canvas_table_get_anchor_bounds (ent2, &bounds);
	xl2 = bounds.x1;
	yt2 = bounds.y1;
	xr2 = bounds.x2;

	if (xl2 > xr1) {
		x1 = xr1 + eps;
		x2 = xl2 - eps;
		x1offset = 2 * sq;
		x2offset = -2 * sq;
	}
	else {
		if (xl1 >= xr2) {
			x1 = xl1 - eps;
			x2 = xr2 + eps;
			x1offset = - 2 * sq;
			x2offset = 2 * sq;
		}
		else {
			if ((xl1 + xr1) < (xl2 + xr2)) {
				x1 = xl1 - eps;
				x2 = xl2 - eps;
				x1offset = -2 * sq;
				x2offset = -2 * sq;
			}
			else {
				x1 = xr1 + eps;
				x2 = xr2 + eps;
				x1offset = 2 * sq;
				x2offset = 2 * sq;
			}
		}
	}

	points->coords[0] = x1;
	points->coords[1] = browser_canvas_table_get_column_ypos (ent1, field1) + yt1;

	points->coords[2] = x1 + x1offset;
	points->coords[3] = points->coords[1];

	points->coords[4] = x2 + x2offset;
	points->coords[5] = browser_canvas_table_get_column_ypos (ent2, field2) + yt2;

	points->coords[6] = x2;
	points->coords[7] = points->coords[5];

	id = g_strdup_printf ("c%d", nb_connect);
	shape = browser_canvas_canvas_shape_find (retval, id);
	if (shape) {
		g_object_set (shape->item, "points", points, NULL);
		shape->_used = TRUE;
		g_free (id);
	}
	else {
		item = goo_canvas_polyline_new_line (parent, 
						     points->coords[0], points->coords [1],
						     points->coords[2], points->coords [3],
						     "close-path", FALSE,
						     "points", points, NULL);
		retval = browser_canvas_canvas_shape_add_to_list (retval, id, item);
	}

	/* extension marks as text */
	if (ext & CANVAS_SHAPE_EXT_JOIN_OUTER_1) {
		gdouble mxoff = 0., myoff = 0.;
		GooCanvasAnchorType atype;
		
		compute_text_marks_offsets (points->coords[4], points->coords[5], 
					    points->coords[2], points->coords[3],
					    &mxoff, &myoff, &atype);
		
		id = g_strdup_printf ("ce%d1", nb_connect);
		shape = browser_canvas_canvas_shape_find (retval, id);
		if (shape) {
			g_object_set (shape->item, 
				      "x", points->coords[2] + mxoff,
				      "y", points->coords[3] + myoff, 
				      "anchor", atype, NULL);
			shape->_used = TRUE;
			g_free (id);
		}
		else {
			item = goo_canvas_text_new (parent, "*", 
						    points->coords[2] + mxoff,
						    points->coords[3] + myoff, -1,
						    atype, NULL);
			retval = browser_canvas_canvas_shape_add_to_list (retval, id, item);
		}
	}
	
	if (ext & CANVAS_SHAPE_EXT_JOIN_OUTER_2) {
		gdouble mxoff, myoff;
		GooCanvasAnchorType atype;
		
		compute_text_marks_offsets (points->coords[2], points->coords[3], 
					    points->coords[4], points->coords[5],
					    &mxoff, &myoff, &atype);

		id = g_strdup_printf ("ce%d2", nb_connect);
		shape = browser_canvas_canvas_shape_find (retval, id);
		if (shape) {
			g_object_set (shape->item, 
				      "x", points->coords[2] + mxoff,
				      "y", points->coords[3] + myoff, 
				      "anchor", atype, NULL);
			shape->_used = TRUE;
			g_free (id);
		}
		else {
			item = goo_canvas_text_new (parent, "*", 
						    points->coords[2] + mxoff,
						    points->coords[3] + myoff, -1,
						    atype, NULL);
			retval = browser_canvas_canvas_shape_add_to_list (retval, id, item);
		}
	}

	goo_canvas_points_unref (points);

	return retval;
}

static GSList *
browser_canvas_canvas_shape_add_to_list (GSList *list, gchar *swallow_id, GooCanvasItem *item)
{
	BrowserCanvasCanvasShape *shape = g_new (BrowserCanvasCanvasShape, 1);

	g_assert (swallow_id);
	g_assert (item);
	shape->id = swallow_id;
	shape->item = item;
	shape->_used = TRUE;
	shape->is_new = TRUE;

	/*g_print ("Shape %p (%s: %s) added\n", item, swallow_id, G_OBJECT_TYPE_NAME (item));*/

	return g_slist_append (list, shape);
}

static BrowserCanvasCanvasShape *
browser_canvas_canvas_shape_find (GSList *list, const gchar *id)
{
	BrowserCanvasCanvasShape *shape = NULL;
	GSList *l;

	for (l = list; l && !shape; l = l->next) 
		if (!strcmp (((BrowserCanvasCanvasShape*) l->data)->id, id))
			shape = (BrowserCanvasCanvasShape*) l->data;

	/*g_print ("Looking for shape %s: %s\n", id, shape ? "Found" : "Not found");*/
	return shape;
}

GSList *
browser_canvas_canvas_shapes_remove_obsolete_shapes (GSList *list)
{
	GSList *l, *ret = list;

	for (l = list; l; ) {
		if (((BrowserCanvasCanvasShape*)(l->data))->_used) {
			((BrowserCanvasCanvasShape*)(l->data))->_used = FALSE;
			l=l->next;
		}
		else {
			GSList *tmp;
			BrowserCanvasCanvasShape *shape = (BrowserCanvasCanvasShape*) l->data;

			g_free (shape->id);
			goo_canvas_item_remove (shape->item);
			g_free (shape);

			tmp = l->next;
			ret = g_slist_delete_link (ret, l);
			l = tmp;
		}
	}

	return ret;
}

void
browser_canvas_canvas_shapes_remove_all (GSList *list)
{
	GSList *l;

	for (l = list; l; l = l->next) {
		BrowserCanvasCanvasShape *shape = (BrowserCanvasCanvasShape*) l->data;
		
		g_free (shape->id);
		goo_canvas_item_remove (shape->item);
		g_free (shape);
	}

	g_slist_free (list);
}

void
browser_canvas_canvas_shapes_dump (GSList *list)
{
	GSList *l;
	g_print ("Canvas shapes...\n");
	for (l = list; l; l = l->next) 
		g_print ("\tShape %s @%p (%s: %p) %s\n", BROWSER_CANVAS_CANVAS_SHAPE (l->data)->id, 
			 BROWSER_CANVAS_CANVAS_SHAPE (l->data),
			 G_OBJECT_TYPE_NAME (BROWSER_CANVAS_CANVAS_SHAPE (l->data)->item),
			 BROWSER_CANVAS_CANVAS_SHAPE (l->data)->item,
			 BROWSER_CANVAS_CANVAS_SHAPE (l->data)->_used ? "Used": "Not used");
}
