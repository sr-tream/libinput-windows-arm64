/*
 * Copyright © 2015 Red Hat, Inc.
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

// Extracted without function-body changes from src/evdev-mt-touchpad-gestures.c
static void
tp_gesture_apply_scroll_constraints(struct tp_dispatch *tp,
				    struct device_float_coords *raw,
				    struct normalized_coords *delta,
				    usec_t time)
{
	usec_t tdelta = usec_from_millis(0);
	struct phys_coords delta_mm, vector;
	double vector_decay, vector_length, slope;

	const usec_t ACTIVE_THRESHOLD = usec_from_millis(100),
		     INACTIVE_THRESHOLD = usec_from_millis(50),
		     EVENT_TIMEOUT = usec_from_millis(100);

	/* Both axes active == true means free scrolling is enabled */
	if (tp->scroll.active.h && tp->scroll.active.v)
		return;

	/* Determine time delta since last movement event */
	if (!usec_is_zero(tp->scroll.time_prev)) {
		usec_t diff = usec_delta(time, tp->scroll.time_prev);
		if (usec_cmp(diff, EVENT_TIMEOUT) <= 0)
			tdelta = diff;
	}

	tp->scroll.time_prev = time;

	/* Delta since last movement event in mm */
	delta_mm = tp_phys_delta(tp, *raw);

	/* Old vector data "fades" over time. This is a two-part linear
	 * approximation of an exponential function - for example, for
	 * EVENT_TIMEOUT of 100, vector_decay = (0.97)^tdelta. This linear
	 * approximation allows easier tweaking of EVENT_TIMEOUT and is faster.
	 */
	if (usec_gt(tdelta, 0)) {
		uint64_t delta = usec_as_uint64_t(tdelta);
		double recent, later;
		uint64_t timeout_us = usec_as_uint64_t(EVENT_TIMEOUT);
		recent = ((timeout_us / 2.0) - delta) / (timeout_us / 2.0);
		later = (timeout_us - delta) / (timeout_us * 2.0);
		vector_decay = delta <= (0.33 * timeout_us) ? recent : later;
	} else {
		vector_decay = 0.0;
	}

	/* Calculate windowed vector from delta + weighted historic data */
	vector.x = (tp->scroll.vector.x * vector_decay) + delta_mm.x;
	vector.y = (tp->scroll.vector.y * vector_decay) + delta_mm.y;
	vector_length = hypot(vector.x, vector.y);
	tp->scroll.vector = vector;

	/* We care somewhat about distance and speed, but more about
	 * consistency of direction over time. Keep track of the time spent
	 * primarily along each axis. If one axis is active, time spent NOT
	 * moving much in the other axis is subtracted, allowing a switch of
	 * axes in a single scroll + ability to "break out" and go diagonal.
	 *
	 * Slope to degree conversions (infinity = 90°, 0 = 0°):
	 */
	const double DEGREE_75 = 3.73;
	const double DEGREE_60 = 1.73;
	const double DEGREE_30 = 0.57;
	const double DEGREE_15 = 0.27;
	slope = (vector.x != 0) ? fabs(vector.y / vector.x) : INFINITY;

	/* Ensure vector is big enough (in mm per EVENT_TIMEOUT) to be confident
	 * of direction. Larger = harder to enable diagonal/free scrolling.
	 */
	const double MIN_VECTOR = 0.15;

	if (slope >= DEGREE_30 && vector_length > MIN_VECTOR) {
		tp->scroll.duration.v = usec_add(tp->scroll.duration.v, tdelta);
		if (usec_cmp(tp->scroll.duration.v, ACTIVE_THRESHOLD) > 0)
			tp->scroll.duration.v = ACTIVE_THRESHOLD;
		if (slope >= DEGREE_75) {
			if (usec_cmp(tp->scroll.duration.h, tdelta) > 0)
				tp->scroll.duration.h =
					usec_sub(tp->scroll.duration.h, tdelta);
			else
				tp->scroll.duration.h = usec_from_uint64_t(0);
		}
	}
	if (slope < DEGREE_60 && vector_length > MIN_VECTOR) {
		tp->scroll.duration.h = usec_add(tp->scroll.duration.h, tdelta);
		if (usec_cmp(tp->scroll.duration.h, ACTIVE_THRESHOLD) > 0)
			tp->scroll.duration.h = ACTIVE_THRESHOLD;
		if (slope < DEGREE_15) {
			if (usec_cmp(tp->scroll.duration.v, tdelta) > 0)
				tp->scroll.duration.v =
					usec_sub(tp->scroll.duration.v, tdelta);
			else
				tp->scroll.duration.v = usec_from_uint64_t(0);
		}
	}

	if (usec_cmp(tp->scroll.duration.h, ACTIVE_THRESHOLD) == 0) {
		tp->scroll.active.h = true;
		if (usec_cmp(tp->scroll.duration.v, INACTIVE_THRESHOLD) < 0)
			tp->scroll.active.v = false;
	}
	if (usec_cmp(tp->scroll.duration.v, ACTIVE_THRESHOLD) == 0) {
		tp->scroll.active.v = true;
		if (usec_cmp(tp->scroll.duration.h, INACTIVE_THRESHOLD) < 0)
			tp->scroll.active.h = false;
	}

	/* If vector is big enough in a diagonal direction, always unlock
	 * both axes regardless of thresholds
	 */
	if (vector_length > 5.0 && slope < 1.73 && slope >= 0.57) {
		tp->scroll.active.v = true;
		tp->scroll.active.h = true;
	}

	/* If only one axis is active, constrain motion accordingly. If both
	 * are set, we've detected deliberate diagonal movement; enable free
	 * scrolling for the life of the gesture.
	 */
	if (!tp->scroll.active.h && tp->scroll.active.v)
		delta->x = 0.0;
	if (tp->scroll.active.h && !tp->scroll.active.v)
		delta->y = 0.0;

	/* If we haven't determined an axis, use the slope in the meantime */
	if (!tp->scroll.active.h && !tp->scroll.active.v) {
		delta->x = (slope >= DEGREE_60) ? 0.0 : delta->x;
		delta->y = (slope < DEGREE_30) ? 0.0 : delta->y;
	}
}
