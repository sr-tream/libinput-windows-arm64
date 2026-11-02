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

// Unchanged function bodies from src/filter-mouse.c.

static bool
accelerator_set_speed(struct motion_filter *filter, double speed_adjustment)
{
	struct pointer_accelerator *accel_filter = (struct pointer_accelerator *)filter;

	assert(speed_adjustment >= -1.0 && speed_adjustment <= 1.0);

	/* Note: the numbers below are nothing but trial-and-error magic,
	   don't read more into them other than "they mostly worked ok" */

	/* delay when accel kicks in */
	accel_filter->threshold =
		DEFAULT_THRESHOLD - v_usec_from_millis(0.25) * speed_adjustment;
	if (accel_filter->threshold < MINIMUM_THRESHOLD)
		accel_filter->threshold = MINIMUM_THRESHOLD;

	/* adjust max accel factor */
	accel_filter->accel = DEFAULT_ACCELERATION + speed_adjustment * 1.5;

	/* higher speed -> faster to reach max */
	accel_filter->incline = DEFAULT_INCLINE + speed_adjustment * 0.75;

	filter->speed_adjustment = speed_adjustment;
	return true;
}

double
pointer_accel_profile_linear(struct motion_filter *filter,
			     void *data,
			     double speed_in, /* in normalized units */
			     usec_t time)
{
	struct pointer_accelerator *accel_filter = (struct pointer_accelerator *)filter;
	const double max_accel = accel_filter->accel;     /* unitless factor */
	const double threshold = accel_filter->threshold; /* 1000dpi units/us */
	const double incline = accel_filter->incline;
	double factor; /* unitless */

	/*
	   Our acceleration function calculates a factor to accelerate input
	   deltas with. The function is a double incline with a plateau,
	   with a rough shape like this:

	  accel
	 factor
	   ^
	   |        /
	   |  _____/
	   | /
	   |/
	   +-------------> speed in

	   The two inclines are linear functions in the form
		   y = ax + b
		   where y is speed_out
			 x is speed_in
			 a is the incline of acceleration
			 b is minimum acceleration factor

	   for speeds up to 0.07 u/ms, we decelerate, down to 30% of input
	   speed.
		   hence 1 = a * 0.07 + 0.3
		       0.7 = a * 0.07 => a := 10
		   deceleration function is thus:
			y = 10x + 0.3

	  Note:
	  * 0.07u/ms as threshold is a result of trial-and-error and
	    has no other intrinsic meaning.
	  * 0.3 is chosen simply because it is above the Nyquist frequency
	    for subpixel motion within a pixel.
	*/
	if (v_us2ms(speed_in) < 0.07) {
		factor = 10 * v_us2ms(speed_in) + 0.3;
		/* up to the threshold, we keep factor 1, i.e. 1:1 movement */
	} else if (speed_in < threshold) {
		factor = 1;

	} else {
		/* Acceleration function above the threshold:
			y = ax' + b
			where T is threshold
			      x is speed_in
			      x' is speed
			and
				y(T) == 1
			hence 1 = ax' + 1
				=> x' := (x - T)
		 */
		factor = incline * v_us2ms(speed_in - threshold) + 1;
	}

	/* Cap at the maximum acceleration factor */
	factor = min(max_accel, factor);

	return factor;
}
