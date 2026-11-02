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

// Unchanged function bodies from src/filter.c at the pinned libinput commit.

void
trackers_reset(struct pointer_trackers *trackers, usec_t time)
{
	unsigned int offset;
	struct pointer_tracker *tracker;

	for (offset = 1; offset < trackers->ntrackers; offset++) {
		tracker = trackers_by_offset(trackers, offset);
		tracker->time = usec_from_uint64_t(0);
		tracker->dir = 0;
		tracker->delta.x = 0;
		tracker->delta.y = 0;
	}

	tracker = trackers_by_offset(trackers, 0);
	tracker->time = time;
	tracker->dir = UNDEFINED_DIRECTION;
}

void
trackers_feed(struct pointer_trackers *trackers,
	      const struct device_float_coords *delta,
	      usec_t time)
{
	unsigned int i, current;
	struct pointer_tracker *ts = trackers->trackers;

	assert(trackers->ntrackers);

	for (i = 0; i < trackers->ntrackers; i++) {
		ts[i].delta.x += delta->x;
		ts[i].delta.y += delta->y;
	}

	current = (trackers->cur_tracker + 1) % trackers->ntrackers;
	trackers->cur_tracker = current;

	ts[current].delta.x = 0.0;
	ts[current].delta.y = 0.0;
	ts[current].time = time;
	ts[current].dir = device_float_get_direction(*delta);
}

struct pointer_tracker *
trackers_by_offset(struct pointer_trackers *trackers, unsigned int offset)
{
	unsigned int index = (trackers->cur_tracker + trackers->ntrackers - offset) %
			     trackers->ntrackers;
	return &trackers->trackers[index];
}

static double
calculate_trackers_velocity(const struct pointer_tracker *tracker,
			    usec_t time,
			    struct pointer_delta_smoothener *smoothener)
{
	usec_t tdelta = usec_delta(time, tracker->time);
	tdelta = usec_add(tdelta, usec_from_uint64_t(1));

	if (smoothener && usec_cmp(tdelta, smoothener->threshold) < 0)
		tdelta = smoothener->value;

	return hypot(tracker->delta.x, tracker->delta.y) /
	       (double)usec_as_uint64_t(tdelta); /* units/us */
}

static double
trackers_velocity_after_timeout(const struct pointer_tracker *tracker,
				struct pointer_delta_smoothener *smoothener)
{
	/* First movement after timeout needs special handling.
	 *
	 * When we trigger the timeout, the last event is too far in the
	 * past to use it for velocity calculation across multiple tracker
	 * values.
	 *
	 * Use the motion timeout itself to calculate the speed rather than
	 * the last tracker time. This errs on the side of being too fast
	 * for really slow movements but provides much more useful initial
	 * movement in normal use-cases (pause, move, pause, move)
	 */
	return calculate_trackers_velocity(tracker,
					   usec_add(tracker->time, MOTION_TIMEOUT),
					   smoothener);
}

double
trackers_velocity(struct pointer_trackers *trackers, usec_t time)
{
	const double MAX_VELOCITY_DIFF = v_usec_from_millis(1); /* units/us */
	double result = 0.0;
	double initial_velocity = 0.0;

	unsigned int dir = trackers_by_offset(trackers, 0)->dir;

	/* Find least recent vector within a timelimit, maximum velocity diff
	 * and direction threshold. */
	for (unsigned int offset = 1; offset < trackers->ntrackers; offset++) {
		const struct pointer_tracker *tracker =
			trackers_by_offset(trackers, offset);

		/* Bug: time running backwards */
		if (usec_cmp(tracker->time, time) > 0)
			break;

		/* Stop if too far away in time */
		usec_t tdelta = usec_delta(time, tracker->time);
		if (usec_cmp(tdelta, MOTION_TIMEOUT) > 0) {
			if (offset == 1)
				result = trackers_velocity_after_timeout(
					tracker,
					trackers->smoothener);
			break;
		}

		double velocity = calculate_trackers_velocity(tracker,
							      time,
							      trackers->smoothener);

		/* Stop if direction changed */
		dir &= tracker->dir;
		if (dir == 0) {
			/* First movement after dirchange - velocity is that
			 * of the last movement */
			if (offset == 1)
				result = velocity;
			break;
		}

		/* Always average the first two events. On some touchpads
		 * where the first event is jumpy, this somewhat reduces
		 * pointer jumps on slow motions. */
		if (initial_velocity == 0.0 || offset <= 2) {
			result = initial_velocity = velocity;
		} else {
			/* Stop if velocity differs too much from initial */
			double velocity_diff = fabs(initial_velocity - velocity);
			if (velocity_diff > MAX_VELOCITY_DIFF)
				break;

			result = velocity;
		}
	}

	return result; /* units/us */
}

double
calculate_acceleration_simpsons(struct motion_filter *filter,
				accel_profile_func_t profile,
				void *data,
				double velocity,
				double last_velocity,
				usec_t time)
{
	double factor;

	/* Use Simpson's rule to calculate the average acceleration between
	 * the previous motion and the most recent. */
	factor = profile(filter, data, velocity, time);
	factor += profile(filter, data, last_velocity, time);
	factor += 4.0 * profile(filter, data, (last_velocity + velocity) / 2, time);

	factor = factor / 6.0;

	return factor; /* unitless factor */
}
