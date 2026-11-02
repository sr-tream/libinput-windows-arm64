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

// Unchanged function bodies from src/filter-touchpad.c.

static inline double
speed_factor(double s)
{
	return pow(s + 1, 2.38) * 0.95 + 0.05;
}

static bool
touchpad_accelerator_set_speed(struct motion_filter *filter, double speed_adjustment)
{
	struct touchpad_accelerator *accel_filter =
		(struct touchpad_accelerator *)filter;

	assert(speed_adjustment >= -1.0 && speed_adjustment <= 1.0);

	filter->speed_adjustment = speed_adjustment;
	accel_filter->speed_factor = speed_factor(speed_adjustment);

	return true;
}

double
touchpad_accel_profile_linear(struct motion_filter *filter,
			      void *data,
			      double speed_in, /* in device units/µs */
			      usec_t time)
{
	struct touchpad_accelerator *accel_filter =
		(struct touchpad_accelerator *)filter;
	const double threshold = accel_filter->threshold; /* mm/s */
	const double baseline = 0.9;
	double factor; /* unitless */

	/* Convert to mm/s because that's something one can understand */
	speed_in = v_us2s(speed_in) * 25.4 / accel_filter->dpi;

	/*
	   Our acceleration function calculates a factor to accelerate input
	   deltas with. The function is a double incline with a plateau,
	   with a rough shape like this:

	  accel
	 factor
	   ^         ______
	   |        )
	   |  _____)
	   | /
	   |/
	   +-------------> speed in

	   Except the second incline is a curve, but well, asciiart.

	   The first incline is a linear function in the form
		   y = ax + b
		   where y is speed_out
			 x is speed_in
			 a is the incline of acceleration
			 b is minimum acceleration factor
	   for speeds up to the lower threshold, we decelerate, down to 30%
	   of input speed.
		   hence 1 = a * 7 + 0.3
		       0.7 = a * 7  => a := 0.1
		   deceleration function is thus:
			y = 0.1x + 0.3

	   The first plateau is the baseline.

	   The second incline is a curve up, based on magic numbers
	   obtained by trial-and-error.

	   Above the second incline we have another plateau because
	   by then you're moving so fast that extra acceleration doesn't
	   help.

	  Note:
	  * The minimum threshold is a result of trial-and-error and
	    has no other special meaning.
	  * 0.3 is chosen simply because it is above the Nyquist frequency
	    for subpixel motion within a pixel.
	*/

	if (speed_in < 7.0) {
		factor = min(baseline, 0.1 * speed_in + 0.3);
		/* up to the threshold, we keep factor 1, i.e. 1:1 movement */
	} else if (speed_in < threshold) {
		factor = baseline;
	} else {

		/* Acceleration function above the threshold is a curve up
		   to four times the threshold, because why not.

		   Don't assume anything about the specific numbers though, this was
		   all just trial and error by tweaking numbers here and there, then
		   the formula was optimized doing basic maths.

		   You could replace this with some other random formula that gives
		   the same numbers and it would be just as correct.

		 */
		const double upper_threshold = threshold * 4.0;
		speed_in = min(speed_in, upper_threshold);

		factor = 0.0025 * (speed_in / threshold) * (speed_in - threshold) +
			 baseline;
	}

	factor *= accel_filter->speed_factor;
	return factor * TP_MAGIC_SLOWDOWN;
}
