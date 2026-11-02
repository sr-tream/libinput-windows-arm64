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

// Unchanged function body from src/filter-low-dpi.c.
double
pointer_accel_profile_linear_low_dpi(struct motion_filter *filter,
				     void *data,
				     double speed_in, /* in device units (units/us) */
				     usec_t time)
{
	struct pointer_accelerator_low_dpi *accel_filter =
		(struct pointer_accelerator_low_dpi *)filter;

	double max_accel = accel_filter->accel;     /* unitless factor */
	double threshold = accel_filter->threshold; /* units/us */
	const double incline = accel_filter->incline;
	double dpi_factor = accel_filter->dpi / (double)DEFAULT_MOUSE_DPI;
	double factor; /* unitless */

	/* dpi_factor is always < 1.0, increase max_accel, reduce
	   the threshold so it kicks in earlier */
	max_accel /= dpi_factor;
	threshold *= dpi_factor;

	/* see pointer_accel_profile_linear for a long description */
	if (v_us2ms(speed_in) < 0.07)
		factor = 10 * v_us2ms(speed_in) + 0.3;
	else if (speed_in < threshold)
		factor = 1;
	else
		factor = incline * v_us2ms(speed_in - threshold) + 1;

	factor = min(max_accel, factor);

	return factor;
}
