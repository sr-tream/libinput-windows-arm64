/*
 * Copyright © 2013 Jonas Ådahl
 * Copyright © 2013-2015 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

// Extracted without function-body changes from src/libinput-private.h
static inline uint32_t
xy_get_direction(double x, double y)
{
	uint32_t dir = UNDEFINED_DIRECTION;
	int d1, d2;
	double r;

	if (fabs(x) < 2.0 && fabs(y) < 2.0) {
		if (x > 0.0 && y > 0.0)
			dir = S | SE | E;
		else if (x > 0.0 && y < 0.0)
			dir = N | NE | E;
		else if (x < 0.0 && y > 0.0)
			dir = S | SW | W;
		else if (x < 0.0 && y < 0.0)
			dir = N | NW | W;
		else if (x > 0.0)
			dir = NE | E | SE;
		else if (x < 0.0)
			dir = NW | W | SW;
		else if (y > 0.0)
			dir = SE | S | SW;
		else if (y < 0.0)
			dir = NE | N | NW;
	} else {
		/* Calculate r within the interval  [0 to 8)
		 *
		 * r = [0 .. 2π] where 0 is North
		 * d_f = r / 2π  ([0 .. 1))
		 * d_8 = 8 * d_f
		 */
		r = atan2(y, x);
		r = fmod(r + 2.5 * M_PI, 2 * M_PI);
		r *= 4 * M_1_PI;

		/* Mark one or two close enough octants */
		d1 = (int)(r + 0.9) % 8;
		d2 = (int)(r + 0.1) % 8;

		dir = bit(d1) | bit(d2);
	}

	return dir;
}
