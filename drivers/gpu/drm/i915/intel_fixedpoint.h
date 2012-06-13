/*
 * Copyright 2012 The Chromium OS Authors.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/*
 * The backlight is corrected in linear space. However the LCD correction is
 * corrected in gamma space. So to be able to compute the correction value for
 * the LCD, we have to compute the inverse gamma. To do so, we carry this
 * small fixed point module which allows us to use pow() to compute inverse
 * gamma.
 *
 * The fixed point format used here is 16.16.
 */

/* intel_fixed_exp_tbl[x*32] = exp(x) * 65536 */
static const int intel_fixed_exp_tbl[33] = {
0x00010000, 0x00010820, 0x00011083, 0x00011929, 0x00012216, 0x00012b4b,
0x000134cc, 0x00013e99, 0x000148b6, 0x00015325, 0x00015de9, 0x00016905,
0x0001747a, 0x0001804d, 0x00018c80, 0x00019916, 0x0001a613, 0x0001b378,
0x0001c14b, 0x0001cf8e, 0x0001de45, 0x0001ed74, 0x0001fd1e, 0x00020d47,
0x00021df4, 0x00022f28, 0x000240e8, 0x00025338, 0x0002661d, 0x0002799b,
0x00028db8, 0x0002a278, 0x0002b7e1
};

/* intel_fixed_log_tbl[x*32] = log(x) * 65536 */
static const int intel_fixed_log_tbl[33] = {
0x80000000, 0xfffc88c6, 0xfffd3a38, 0xfffda204, 0xfffdebaa, 0xfffe24ca,
0xfffe5376, 0xfffe7aed, 0xfffe9d1c, 0xfffebb43, 0xfffed63c, 0xfffeeea2,
0xffff04e8, 0xffff1966, 0xffff2c5f, 0xffff3e08, 0xffff4e8e, 0xffff5e13,
0xffff6cb5, 0xffff7a8c, 0xffff87ae, 0xffff942b, 0xffffa014, 0xffffab75,
0xffffb65a, 0xffffc0ce, 0xffffcad8, 0xffffd481, 0xffffddd1, 0xffffe6cd,
0xffffef7a, 0xfffff7df, 0xffffffff
};

/* e * 65536 */
#define FIXED_E (intel_fixed_exp_tbl[32])
/* 1 * 65536 */
#define FIXED_ONE 65536

static int intel_fixed_mul(int a, int b)
{
	int64_t p = (int64_t)a * b;
	do_div(p, 65536);
	return (int)p;
}

static int intel_fixed_div(int a, int b)
{
	int64_t p = (int64_t)a * 65536;
	do_div(p, b);
	return (int)p;
}

/*
 * Approximate fixed point log function.
 * Only works for inputs in [0,1[
 */
static int intel_fixed_log(int val)
{
	int index = val * 32 / FIXED_ONE;
	int remainder = (val & 0x7ff) << 5;
	int v1 = intel_fixed_log_tbl[index];
	int v2 = intel_fixed_log_tbl[index+1];
	int final = v1 + intel_fixed_mul(v2 - v1, remainder);
	return final;
}

/*
 * Approximate fixed point exp function.
 */
static int intel_fixed_exp(int val)
{
	int count = 0;
	int index, remainder;
	int int_part = FIXED_ONE, frac_part;
	int i, v, v1, v2;

	while (val < 0) {
		val += FIXED_ONE;
		count--;
	}

	while (val > FIXED_ONE) {
		val -= FIXED_ONE;
		count++;
	}

	index = val * 32 / FIXED_ONE;
	remainder = (val & 0x7ff) << 5;

	v1 = intel_fixed_exp_tbl[index];
	v2 = intel_fixed_exp_tbl[index+1];
	frac_part = v1 + intel_fixed_mul(v2 - v1, remainder);

	if (count < 0) {
		for (i = 0; i < -count; i++)
			int_part = intel_fixed_mul(int_part, FIXED_E);

		v = intel_fixed_div(frac_part, int_part);
	} else {
		for (i = 0; i < count; i++)
			int_part = intel_fixed_mul(int_part, FIXED_E);

		v = intel_fixed_mul(frac_part, int_part);
	}
	return (v >= 0) ? v : 0;
}

/*
 * Approximate fixed point pow function.
 * Only works for x in [0,1[
 */
static int intel_fixed_pow(int x, int y)
{
	int e, p, r;
	e = intel_fixed_log(x);
	p = intel_fixed_mul(e, y);
	r = intel_fixed_exp(p);
	return r;
}


