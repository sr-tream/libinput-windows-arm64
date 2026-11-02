/*
 * Copyright © 2006-2009 Simon Thum
 * Copyright © 2012 Jonas Ådahl
 * Copyright © 2014-2015 Red Hat, Inc.
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

// Extracted without function-body changes from src/filter-touchpad.c
static struct normalized_coords
touchpad_constant_filter(struct motion_filter *filter,
			 const struct device_float_coords *unaccelerated,
			 void *data,
			 usec_t time)
{
	struct touchpad_accelerator *accel = (struct touchpad_accelerator *)filter;
	struct normalized_coords normalized;
	/* We need to use the same baseline here as the accelerated code,
	 * otherwise our unaccelerated speed is different to the accelerated
	 * speed on the plateau.
	 *
	 * This is a hack, the baseline should be incorporated into the
	 * TP_MAGIC_SLOWDOWN so we only have one number here but meanwhile
	 * this will do.
	 */
	const double baseline = 0.9;

	normalized = normalize_for_dpi(unaccelerated, accel->dpi);
	normalized.x = baseline * TP_MAGIC_SLOWDOWN * normalized.x;
	normalized.y = baseline * TP_MAGIC_SLOWDOWN * normalized.y;

	return normalized;
}
