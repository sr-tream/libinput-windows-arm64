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
static inline int
tp_gesture_same_directions(int dir1, int dir2)
{
	/*
	 * In some cases (semi-mt touchpads) we may seen one finger move
	 * e.g. N/NE and the other W/NW so we not only check for overlapping
	 * directions, but also for neighboring bits being set.
	 * The ((dira & 0x80) && (dirb & 0x01)) checks are to check for bit 0
	 * and 7 being set as they also represent neighboring directions.
	 */
	return ((dir1 | (dir1 >> 1)) & dir2) || ((dir2 | (dir2 >> 1)) & dir1) ||
	       ((dir1 & 0x80) && (dir2 & 0x01)) || ((dir2 & 0x80) && (dir1 & 0x01));
}
