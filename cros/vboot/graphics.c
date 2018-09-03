// SPDX-License-Identifier: BSD-3-Clause
/*
 * Taken from coreboot file of the same name
 *
 * TODO(sjg@chromium.org): Use U-Boot BMP support instead of rewriting it here
 * (may need scaling support)
 *
 * Copyright 2018 Google LLC
 */

#define LOG_CATEGORY	LOGC_VBOOT

#include <common.h>
#include <bmp_layout.h>
#include <dm.h>
#include <log.h>
#include <mapmem.h>
#include <malloc.h>
#include <video.h>
#include <cros/cb_gfx.h>
#include <cros/fpmath.h>
#include <cros/vboot.h>
#include <linux/log2.h>
#include <u-boot/lz4.h>

/*
 * 'canvas' is the drawing area located in the center of the screen. It's a
 * square area, stretching vertically to the edges of the screen, leaving
 * non-drawing areas on the left and right. The screen is assumed to be
 * landscape.
 */
static struct rect canvas;
static struct rect screen;

static u8 *gfx_buffer;

/* Panel orientation, matches drm_connector.h in the Linux kernel. */
enum cb_fb_orientation {
	CB_FB_ORIENTATION_NORMAL = 0,
	CB_FB_ORIENTATION_BOTTOM_UP = 1,
	CB_FB_ORIENTATION_LEFT_UP = 2,
	CB_FB_ORIENTATION_RIGHT_UP = 3,
};

struct cb_framebuffer {
	u32 tag;
	u32 size;

	u64 physical_address;
	u32 x_resolution;
	u32 y_resolution;
	u32 bytes_per_line;
	u8 bits_per_pixel;
	u8 red_mask_pos;
	u8 red_mask_size;
	u8 green_mask_pos;
	u8 green_mask_size;
	u8 blue_mask_pos;
	u8 blue_mask_size;
	u8 reserved_mask_pos;
	u8 reserved_mask_size;
	u8 orientation;
};

/*
 * Framebuffer is assumed to assign a higher coordinate (larger x, y) to
 * a higher address
 */
static struct cb_framebuffer *fbinfo;

/* Shorthand for up-to-date virtual framebuffer address */
#define REAL_FB ((unsigned char *)phys_to_virt(fbinfo->physical_address))
#define FB	(gfx_buffer ? gfx_buffer : REAL_FB)

#define PIVOT_H_MASK	(PIVOT_H_LEFT | PIVOT_H_CENTER | PIVOT_H_RIGHT)
#define PIVOT_V_MASK	(PIVOT_V_TOP | PIVOT_V_CENTER | PIVOT_V_BOTTOM)
#define ROUNDUP(x, y)	((((x) + ((y) - 1)) / (y)) * (y))
#define ABS(x)		((x) < 0 ? -(x) : (x))

static char initialized = 0;

static const struct vector vzero = {
	.x = 0,
	.y = 0,
};

struct color_transformation {
	uint8_t base;
	int16_t scale;
};

struct color_mapping {
	struct color_transformation red;
	struct color_transformation green;
	struct color_transformation blue;
	int enabled;
};

static struct color_mapping color_map;

static inline void set_color_trans(struct color_transformation *trans,
				   uint8_t bg_color, uint8_t fg_color)
{
	trans->base = bg_color;
	trans->scale = fg_color - bg_color;
}

int set_color_map(const struct rgb_color *background,
		  const struct rgb_color *foreground)
{
	if (background == NULL || foreground == NULL)
		return CBGFX_ERROR_INVALID_PARAMETER;

	set_color_trans(&color_map.red, background->red, foreground->red);
	set_color_trans(&color_map.green, background->green,
			foreground->green);
	set_color_trans(&color_map.blue, background->blue, foreground->blue);
	color_map.enabled = 1;

	return CBGFX_SUCCESS;
}

void clear_color_map(void)
{
	color_map.enabled = 0;
}

struct blend_value {
	uint8_t alpha;
	struct rgb_color rgb;
};

static struct blend_value blend;

int set_blend(const struct rgb_color *rgb, uint8_t alpha)
{
	if (rgb == NULL)
		return CBGFX_ERROR_INVALID_PARAMETER;

	blend.alpha = alpha;
	blend.rgb = *rgb;

	return CBGFX_SUCCESS;
}

void clear_blend(void)
{
	blend.alpha = 0;
	blend.rgb.red = 0;
	blend.rgb.green = 0;
	blend.rgb.blue = 0;
}

static void add_vectors(struct vector *out,
			const struct vector *v1, const struct vector *v2)
{
	out->x = v1->x + v2->x;
	out->y = v1->y + v2->y;
}

static int fraction_equal(const struct fraction *f1, const struct fraction *f2)
{
	return (int64_t)f1->n * f2->d == (int64_t)f2->n * f1->d;
}

static int is_valid_fraction(const struct fraction *f)
{
	return f->d != 0;
}

static int is_valid_scale(const struct scale *s)
{
	return is_valid_fraction(&s->x) && is_valid_fraction(&s->y);
}

static void reduce_fraction(struct fraction *out, int64_t n, int64_t d)
{
	/* Simplest way to reduce the fraction until fitting in int32_t */
	int shift = ilog2(max(ABS(n), ABS(d)) >> 31) + 1;
	out->n = n >> shift;
	out->d = d >> shift;
}

/* out = f1 + f2 */
static void add_fractions(struct fraction *out,
			  const struct fraction *f1, const struct fraction *f2)
{
	reduce_fraction(out,
			(int64_t)f1->n * f2->d + (int64_t)f2->n * f1->d,
			(int64_t)f1->d * f2->d);
}

/* out = f1 - f2 */
static void subtract_fractions(struct fraction *out,
			       const struct fraction *f1,
			       const struct fraction *f2)
{
	reduce_fraction(out,
			(int64_t)f1->n * f2->d - (int64_t)f2->n * f1->d,
			(int64_t)f1->d * f2->d);
}

static void add_scales(struct scale *out,
		       const struct scale *s1, const struct scale *s2)
{
	add_fractions(&out->x, &s1->x, &s2->x);
	add_fractions(&out->y, &s1->y, &s2->y);
}

/*
 * Transform a vector:
 *	x' = x * a_x + offset_x
 *	y' = y * a_y + offset_y
 */
static int transform_vector(struct vector *out,
			    const struct vector *in,
			    const struct scale *a,
			    const struct vector *offset)
{
	if (!is_valid_scale(a))
		return CBGFX_ERROR_INVALID_PARAMETER;
	out->x = (int64_t)a->x.n * in->x / a->x.d + offset->x;
	out->y = (int64_t)a->y.n * in->y / a->y.d + offset->y;
	return CBGFX_SUCCESS;
}

/*
 * Returns 1 if v is exclusively within box, 0 if v is inclusively within box,
 * or -1 otherwise.
 */
static int within_box(const struct vector *v, const struct rect *bound)
{
	if (v->x > bound->offset.x &&
	    v->y > bound->offset.y &&
	    v->x < bound->offset.x + bound->size.width &&
	    v->y < bound->offset.y + bound->size.height)
		return 1;
	else if (v->x >= bound->offset.x &&
		 v->y >= bound->offset.y &&
		 v->x <= bound->offset.x + bound->size.width &&
		 v->y <= bound->offset.y + bound->size.height)
		return 0;
	else
		return -1;
}

/* Helper function that applies color_map to the color. */
static inline uint8_t apply_map(uint8_t color,
				const struct color_transformation *trans)
{
	if (!color_map.enabled)
		return color;
	return trans->base + trans->scale * color / UINT8_MAX;
}

/*
 * Helper function that applies color and opacity from blend struct
 * into the color.
 */
static inline uint8_t apply_blend(uint8_t color, uint8_t blend_color)
{
	if (blend.alpha == 0 || color == blend_color)
		return color;

	return (color * (256 - blend.alpha) +
		blend_color * blend.alpha) / 256;
}

static inline uint32_t calculate_color(const struct rgb_color *rgb,
				       uint8_t invert)
{
	uint32_t color = 0;

	color |= (apply_blend(apply_map(rgb->red, &color_map.red),
			      blend.rgb.red)
		  >> (8 - fbinfo->red_mask_size))
		 << fbinfo->red_mask_pos;
	color |= (apply_blend(apply_map(rgb->green, &color_map.green),
			      blend.rgb.green)
		  >> (8 - fbinfo->green_mask_size))
		 << fbinfo->green_mask_pos;
	color |= (apply_blend(apply_map(rgb->blue, &color_map.blue),
			      blend.rgb.blue)
		  >> (8 - fbinfo->blue_mask_size))
		 << fbinfo->blue_mask_pos;
	if (invert)
		color ^= 0xffffffff;
	return color;
}

/*
 * Plot a pixel in a framebuffer. This is called from tight loops. Keep it slim
 * and do the validation at callers' site.
 */
static inline void set_pixel(struct vector *coord, u32 color)
{
	const int bpp = fbinfo->bits_per_pixel;
	const int bpl = fbinfo->bytes_per_line;
	struct vector rcoord;
	int i;

	switch (fbinfo->orientation) {
	case CB_FB_ORIENTATION_NORMAL:
	default:
		rcoord.x = coord->x;
		rcoord.y = coord->y;
		break;
	case CB_FB_ORIENTATION_BOTTOM_UP:
		rcoord.x = screen.size.width - 1 - coord->x;
		rcoord.y = screen.size.height - 1 - coord->y;
		break;
	case CB_FB_ORIENTATION_LEFT_UP:
		rcoord.x = coord->y;
		rcoord.y = screen.size.width - 1 - coord->x;
		break;
	case CB_FB_ORIENTATION_RIGHT_UP:
		rcoord.x = screen.size.height - 1 - coord->y;
		rcoord.y = coord->x;
		break;
	}

	uint8_t * const pixel = FB + rcoord.y * bpl + rcoord.x * bpp / 8;
	for (i = 0; i < bpp / 8; i++)
		pixel[i] = (color >> (i * 8));
}

/*
 * Initializes the library. Automatically called by APIs. It sets up
 * the canvas and the framebuffer.
 */
int cbgfx_init(struct udevice *dev)
{
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);
	struct video_priv *priv = dev_get_uclass_priv(dev);

	if (initialized)
		return 0;

	fbinfo = calloc(1, sizeof(*fbinfo));
	if (!fbinfo)
		return log_msg_ret("fbinfo", -ENOMEM);

	fbinfo->physical_address = plat->base;
	fbinfo->x_resolution = priv->xsize;
	fbinfo->y_resolution = priv->ysize;
	fbinfo->bytes_per_line = priv->line_length;
	fbinfo->bits_per_pixel = 1 << priv->bpix;
	fbinfo->reserved_mask_pos = 0;
	fbinfo->reserved_mask_size = 0;
	switch (priv->bpix) {
	case VIDEO_BPP32:
		fbinfo->red_mask_pos = 16;
		fbinfo->red_mask_size = 8;
		fbinfo->green_mask_pos = 8;
		fbinfo->green_mask_size = 8;
		fbinfo->blue_mask_pos = 0;
		fbinfo->blue_mask_size = 8;
		break;
	case VIDEO_BPP16:
		fbinfo->red_mask_pos = 11;
		fbinfo->red_mask_size = 5;
		fbinfo->green_mask_pos = 5;
		fbinfo->green_mask_size = 6;
		fbinfo->blue_mask_pos = 0;
		fbinfo->blue_mask_size = 5;
		break;
	default:
		log_err("Invalid bpix %d\n", priv->bpix);
		return CBGFX_ERROR_INIT;
	}

	if (!fbinfo->physical_address)
		return CBGFX_ERROR_FRAMEBUFFER_ADDR;

	switch (fbinfo->orientation) {
	default: /* Normal or rotated 180 degrees. */
		screen.size.width = fbinfo->x_resolution;
		screen.size.height = fbinfo->y_resolution;
		break;
	case CB_FB_ORIENTATION_LEFT_UP: /* 90 degree rotation. */
	case CB_FB_ORIENTATION_RIGHT_UP:
		screen.size.width = fbinfo->y_resolution;
		screen.size.height = fbinfo->x_resolution;
		break;
	}
	screen.offset.x = 0;
	screen.offset.y = 0;

	/* Calculate canvas size & offset. Canvas is always square. */
	if (screen.size.height > screen.size.width) {
		canvas.size.height = screen.size.width;
		canvas.size.width = canvas.size.height;
		canvas.offset.x = 0;
		canvas.offset.y = (screen.size.height - canvas.size.height) / 2;
	} else {
		canvas.size.height = screen.size.height;
		canvas.size.width = canvas.size.height;
		canvas.offset.x = (screen.size.width - canvas.size.width) / 2;
		canvas.offset.y = 0;
	}

	initialized = 1;
	log_info("cbgfx initialized: screen:width=%d, height=%d, offset=%d canvas:width=%d, height=%d, offset=%d\n",
		 screen.size.width, screen.size.height, screen.offset.x,
		 canvas.size.width, canvas.size.height, canvas.offset.x);

	return 0;
}

int draw_box(const struct rect *box, const struct rgb_color *rgb)
{
	struct vector top_left;
	struct vector p, t;

	const uint32_t color = calculate_color(rgb, 0);
	const struct scale top_left_s = {
		.x = { .n = box->offset.x, .d = CANVAS_SCALE, },
		.y = { .n = box->offset.y, .d = CANVAS_SCALE, }
	};
	const struct scale bottom_right_s = {
		.x = { .n = box->offset.x + box->size.x, .d = CANVAS_SCALE, },
		.y = { .n = box->offset.y + box->size.y, .d = CANVAS_SCALE, }
	};

	transform_vector(&top_left, &canvas.size, &top_left_s, &canvas.offset);
	transform_vector(&t, &canvas.size, &bottom_right_s, &canvas.offset);
	if (within_box(&t, &canvas) < 0) {
		log_warning("Box exceeds canvas boundary\n");
		return CBGFX_ERROR_BOUNDARY;
	}

	for (p.y = top_left.y; p.y < t.y; p.y++)
		for (p.x = top_left.x; p.x < t.x; p.x++)
			set_pixel(&p, color);

	return CBGFX_SUCCESS;
}

int draw_rounded_box(const struct scale *pos_rel, const struct scale *dim_rel,
		     const struct rgb_color *rgb,
		     const struct fraction *thickness,
		     const struct fraction *radius)
{
	struct scale pos_end_rel;
	struct vector top_left;
	struct vector p, t;

	const uint32_t color = calculate_color(rgb, 0);

	if (!is_valid_scale(pos_rel) || !is_valid_scale(dim_rel))
		return CBGFX_ERROR_INVALID_PARAMETER;

	add_scales(&pos_end_rel, pos_rel, dim_rel);
	transform_vector(&top_left, &canvas.size, pos_rel, &canvas.offset);
	transform_vector(&t, &canvas.size, &pos_end_rel, &canvas.offset);
	if (within_box(&t, &canvas) < 0) {
		log_warning("Box exceeds canvas boundary\n");
		return CBGFX_ERROR_BOUNDARY;
	}

	if (!is_valid_fraction(thickness) || !is_valid_fraction(radius))
		return CBGFX_ERROR_INVALID_PARAMETER;

	struct scale thickness_scale = {
		.x = { .n = thickness->n, .d = thickness->d },
		.y = { .n = thickness->n, .d = thickness->d },
	};
	struct scale radius_scale = {
		.x = { .n = radius->n, .d = radius->d },
		.y = { .n = radius->n, .d = radius->d },
	};
	struct vector d, r, s;
	transform_vector(&d, &canvas.size, &thickness_scale, &vzero);
	transform_vector(&r, &canvas.size, &radius_scale, &vzero);
	const uint8_t has_thickness = d.x > 0 && d.y > 0;
	if (thickness->n != 0 && !has_thickness)
		log_warning("Thickness truncated to 0\n");
	const uint8_t has_radius = r.x > 0 && r.y > 0;
	if (radius->n != 0 && !has_radius)
		log_warning("Radius truncated to 0\n");
	if (has_radius) {
		if (d.x > r.x || d.y > r.y) {
			log_warning("Thickness cannot be greater than radius\n");
			return CBGFX_ERROR_INVALID_PARAMETER;
		}
		if (r.x * 2 > t.x - top_left.x || r.y * 2 > t.y - top_left.y) {
			log_warning("Radius cannot be greater than half of the box\n");
			return CBGFX_ERROR_INVALID_PARAMETER;
		}
	}

	/* Step 1: Draw edges */
	int32_t x_begin, x_end;
	if (has_thickness) {
		/* top */
		for (p.y = top_left.y; p.y < top_left.y + d.y; p.y++)
			for (p.x = top_left.x + r.x; p.x < t.x - r.x; p.x++)
				set_pixel(&p, color);
		/* bottom */
		for (p.y = t.y - d.y; p.y < t.y; p.y++)
			for (p.x = top_left.x + r.x; p.x < t.x - r.x; p.x++)
				set_pixel(&p, color);
		for (p.y = top_left.y + r.y; p.y < t.y - r.y; p.y++) {
			/* left */
			for (p.x = top_left.x; p.x < top_left.x + d.x; p.x++)
				set_pixel(&p, color);
			/* right */
			for (p.x = t.x - d.x; p.x < t.x; p.x++)
				set_pixel(&p, color);
		}
	} else {
		/* Fill the regions except circular sectors */
		for (p.y = top_left.y; p.y < t.y; p.y++) {
			if (p.y >= top_left.y + r.y && p.y < t.y - r.y) {
				x_begin = top_left.x;
				x_end = t.x;
			} else {
				x_begin = top_left.x + r.x;
				x_end = t.x - r.x;
			}
			for (p.x = x_begin; p.x < x_end; p.x++)
				set_pixel(&p, color);
		}
	}

	if (!has_radius)
		return CBGFX_SUCCESS;

	/*
	 * Step 2: Draw rounded corners
	 * When has_thickness, only the border is drawn. With fixed thickness,
	 * the time complexity is linear to the size of the box.
	 */
	if (has_thickness) {
		s.x = r.x - d.x;
		s.y = r.y - d.y;
	} else {
		s.x = 0;
		s.y = 0;
	}

	/* Use 64 bits to avoid overflow */
	int32_t x, y;
	uint64_t yy;
	const uint64_t rrx = (uint64_t)r.x * r.x, rry = (uint64_t)r.y * r.y;
	const uint64_t ssx = (uint64_t)s.x * s.x, ssy = (uint64_t)s.y * s.y;
	x_begin = 0;
	x_end = 0;
	for (y = r.y - 1; y >= 0; y--) {
		/*
		 * The inequality is valid in the beginning of each iteration:
		 * y^2 + x_end^2 < r^2
		 */
		yy = (uint64_t)y * y;
		/* Check yy/ssy + xx/ssx < 1 */
		while (yy * ssx + x_begin * x_begin * ssy < ssx * ssy)
			x_begin++;
		/* The inequality must be valid now: y^2 + x_begin >= s^2 */
		x = x_begin;
		/* Check yy/rry + xx/rrx < 1 */
		while (x < x_end || yy * rrx + x * x * rry < rrx * rry) {
			/*
			 * Example sequence of (y, x) when s = (4, 4) and
			 * r = (5, 5):
			 *   [(4, 0), (4, 1), (4, 2), (3, 3), (2, 4),
			 *    (1, 4), (0, 4)].
			 * If s.x==s.y r.x==r.y, then the sequence will be
			 * symmetric, and x and y will range from 0 to (r-1).
			 */
			/* top left */
			p.y = top_left.y + r.y - 1 - y;
			p.x = top_left.x + r.x - 1 - x;
			set_pixel(&p, color);
			/* top right */
			p.y = top_left.y + r.y - 1 - y;
			p.x = t.x - r.x + x;
			set_pixel(&p, color);
			/* bottom left */
			p.y = t.y - r.y + y;
			p.x = top_left.x + r.x - 1 - x;
			set_pixel(&p, color);
			/* bottom right */
			p.y = t.y - r.y + y;
			p.x = t.x - r.x + x;
			set_pixel(&p, color);
			x++;
		}
		x_end = x;
		/* (x_begin <= x_end) must hold now */
	}

	return CBGFX_SUCCESS;
}

int draw_line(const struct scale *pos1, const struct scale *pos2,
	      const struct fraction *thickness, const struct rgb_color *rgb)
{
	struct fraction len;
	struct vector top_left;
	struct vector size;
	struct vector p, t;

	const uint32_t color = calculate_color(rgb, 0);

	if (!is_valid_fraction(thickness))
		return CBGFX_ERROR_INVALID_PARAMETER;

	transform_vector(&top_left, &canvas.size, pos1, &canvas.offset);
	if (fraction_equal(&pos1->y, &pos2->y)) {
		/* Horizontal line */
		subtract_fractions(&len, &pos2->x, &pos1->x);
		struct scale dim = {
			.x = { .n = len.n, .d = len.d },
			.y = { .n = thickness->n, .d = thickness->d },
		};
		transform_vector(&size, &canvas.size, &dim, &vzero);
		size.y = max(size.y, 1);
	} else if (fraction_equal(&pos1->x, &pos2->x)) {
		/* Vertical line */
		subtract_fractions(&len, &pos2->y, &pos1->y);
		struct scale dim = {
			.x = { .n = thickness->n, .d = thickness->d },
			.y = { .n = len.n, .d = len.d },
		};
		transform_vector(&size, &canvas.size, &dim, &vzero);
		size.x = max(size.x, 1);
	} else {
		log_warning("Only support horizontal and vertical lines\n");
		return CBGFX_ERROR_INVALID_PARAMETER;
	}

	add_vectors(&t, &top_left, &size);
	if (within_box(&t, &canvas) < 0) {
		log_warning("Line exceeds canvas boundary\n");
		return CBGFX_ERROR_BOUNDARY;
	}

	for (p.y = top_left.y; p.y < t.y; p.y++)
		for (p.x = top_left.x; p.x < t.x; p.x++)
			set_pixel(&p, color);

	return CBGFX_SUCCESS;
}

int clear_canvas(const struct rgb_color *rgb)
{
	const struct rect box = {
		vzero,
		.size = {
			.width = CANVAS_SCALE,
			.height = CANVAS_SCALE,
		},
	};

	return draw_box(&box, rgb);
}

int clear_screen(const struct rgb_color *rgb)
{
	struct vector p;
	uint32_t color = calculate_color(rgb, 0);
	const int bpp = fbinfo->bits_per_pixel;
	const int bpl = fbinfo->bytes_per_line;

	/* If all significant bytes in color are equal, fastpath through memset.
	 * We assume that for 32bpp the high byte gets ignored anyway. */
	if ((((color >> 8) & 0xff) == (color & 0xff)) && (bpp == 16 ||
	    (((color >> 16) & 0xff) == (color & 0xff)))) {
		memset(FB, color & 0xff, fbinfo->y_resolution * bpl);
	} else {
		for (p.y = 0; p.y < screen.size.height; p.y++)
			for (p.x = 0; p.x < screen.size.width; p.x++)
				set_pixel(&p, color);
	}

	return CBGFX_SUCCESS;
}

static int pal_to_rgb(uint8_t index, const struct bmp_color_table_entry *pal,
		      size_t palcount, struct rgb_color *out)
{
	if (index >= palcount) {
		log_warning("Color index %d exceeds palette boundary\n", index);
		return CBGFX_ERROR_BITMAP_DATA;
	}

	out->red = pal[index].red;
	out->green = pal[index].green;
	out->blue = pal[index].blue;
	return CBGFX_SUCCESS;
}

/*
 * We're using the Lanczos resampling algorithm to rescale images to a new size.
 * Since output size is often not cleanly divisible by input size, an output
 * pixel (ox,oy) corresponds to a point that lies in the middle between several
 * input pixels (ix,iy), meaning that if you transformed the coordinates of the
 * output pixel into the input image space, they would be fractional. To sample
 * the color of this "virtual" pixel with fractional coordinates, we gather the
 * 6x6 grid of nearest real input pixels in a sample array. Then we multiply the
 * color values for each of those pixels (separately for red, green and blue)
 * with a "weight" value that was calculated from the distance between that
 * input pixel and the fractional output pixel coordinates. This is done for
 * both X and Y dimensions separately. The combined weights for all 36 sample
 * pixels add up to 1.0, so by adding up the multiplied color values we get the
 * interpolated color for the output pixel.
 *
 * The CONFIG_LP_CBGFX_FAST_RESAMPLE option let's the user change the 'a'
 * parameter from the Lanczos weight formula from 3 to 2, which effectively
 * reduces the size of the sample array from 6x6 to 4x4. This is a bit faster
 * but doesn't look as good. Most use cases should be fine without it.
 */
#if IS_ENABLED(CONFIG_LP_CBGFX_FAST_RESAMPLE)
#define LNCZ_A 2
#else
#define LNCZ_A 3
#endif

/*
 * When walking the sample array we often need to start at a pixel close to our
 * fractional output pixel (for convenience we choose the pixel on the top-left
 * which corresponds to the integer parts of the output pixel coordinates) and
 * then work our way outwards in both directions from there. Arrays in C must
 * start at 0 but we'd really prefer indexes to go from -2 to 3 (for 6x6)
 * instead, so that this "start pixel" could be 0. Since we cannot do that,
 * define a constant for the index of that "0th" pixel instead.
 */
#define S0 (LNCZ_A - 1)

/* The size of the sample array, which we need a lot. */
#define SSZ (LNCZ_A * 2)

/*
 * This is implementing the Lanczos kernel according to:
 * https://en.wikipedia.org/wiki/Lanczos_resampling
 *
 *         / 1							if x = 0
 * L(x) = <  a * sin(pi * x) * sin(pi * x / a) / (pi^2 * x^2)	if -a < x <= a
 *	   \ 0							otherwise
 */
static fpmath_t lanczos_weight(fpmath_t in, int off)
{
	/*
	 * |in| is the output pixel coordinate scaled into the input pixel
	 * space. |off| is the offset in the sample array for the pixel whose
	 * weight we're calculating. (off - S0) is the distance from that
	 * sample pixel to the S0 pixel, and the fractional part of |in|
	 * (in - floor(in)) is by definition the distance between S0 and the
	 * output pixel.
	 *
	 * So (off - S0) - (in - floor(in)) is the distance from the sample
	 * pixel to S0 minus the distance from S0 to the output pixel, aka
	 * the distance from the sample pixel to the output pixel.
	 */
	fpmath_t x = fpisub(off - S0, fpsubi(in, fpfloor(in)));

	if (fpequals(x, fp(0)))
		return fp(1);

	/* x * 2 / a can save some instructions if a == 2 */
	fpmath_t x2a = x;
	if (LNCZ_A != 2)
		x2a = fpmul(x, fpfrac(2, LNCZ_A));

	fpmath_t x_times_pi = fpmul(x, fppi());

	/*
	 * Rather than using sinr(pi*x), we leverage the "one-based" sine
	 * function (see <fpmath.h>) with sin1(2*x) so that the pi is eliminated
	 * since multiplication by an integer is a slightly faster operation.
	 */
	fpmath_t tmp = fpmuli(fpdiv(fpsin1(fpmuli(x, 2)), x_times_pi), LNCZ_A);
	return fpdiv(fpmul(tmp, fpsin1(x2a)), x_times_pi);
}

static int draw_bitmap_v3(const struct vector *top_left,
			  const struct vector *dim,
			  const struct vector *dim_org,
			  const struct bmp_header *header,
			  const struct bmp_color_table_entry *pal,
			  const uint8_t *pixel_array, uint8_t invert)
{
	const int bpp = header->bit_count;
	int32_t dir;
	struct vector p;
	int32_t ox, oy;		/* output (resampled) pixel coordinates */
	int32_t ix, iy;		/* input (source image) pixel coordinates */
	int sx, sy;	/* index into |sample| (not ringbuffer adjusted) */

	if (header->compression) {
		log_err("Compressed bitmaps are not supported\n");
		return CBGFX_ERROR_BITMAP_FORMAT;
	}
	if (bpp >= 16) {
		log_err("Non-palette bitmaps are not supported\n");
		return CBGFX_ERROR_BITMAP_FORMAT;
	}
	if (bpp != 8) {
		log_err("Unsupported bits per pixel: %d\n", bpp);
		return CBGFX_ERROR_BITMAP_FORMAT;
	}

	const s32 y_stride = ROUNDUP(dim_org->width * bpp / 8, 4);
	/*
	 * header->height can be positive or negative.
	 *
	 * If it's negative, pixel data is stored from top to bottom. We render
	 * image from the lowest row to the highest row.
	 *
	 * If it's positive, pixel data is stored from bottom to top. We render
	 * image from the highest row to the lowest row.
	 */
	p.y = top_left->y;
	if (header->height < 0) {
		dir = 1;
	} else {
		p.y += dim->height - 1;
		dir = -1;
	}

	/* Don't waste time resampling when the scale is 1:1. */
	if (dim_org->width == dim->width && dim_org->height == dim->height) {
		for (oy = 0; oy < dim->height; oy++, p.y += dir) {
			p.x = top_left->x;
			for (ox = 0; ox < dim->width; ox++, p.x++) {
				struct rgb_color rgb;
				if (pal_to_rgb(pixel_array[oy * y_stride + ox],
					       pal, header->colors_used, &rgb))
					return CBGFX_ERROR_BITMAP_DATA;
				set_pixel(&p, calculate_color(&rgb, invert));
			}
		}
		return CBGFX_SUCCESS;
	}

	/* Precalculate the X-weights for every possible ox so that we only have
	   to multiply weights together in the end. */
	fpmath_t (*weight_x)[SSZ] = malloc(sizeof(fpmath_t) * SSZ * dim->width);
	if (!weight_x)
		return CBGFX_ERROR_UNKNOWN;
	for (ox = 0; ox < dim->width; ox++) {
		for (sx = 0; sx < SSZ; sx++) {
			fpmath_t ixfp = fpfrac(ox * dim_org->width, dim->width);
			weight_x[ox][sx] = lanczos_weight(ixfp, sx);
		}
	}

	/*
	 * For every sy in the sample array, we directly cache a pointer into
	 * the .BMP pixel array for the start of the corresponding line. On the
	 * edges of the image (where we don't have any real pixels to fill all
	 * lines in the sample array), we just reuse the last valid lines inside
	 * the image for all lines that would lie outside.
	 */
	const uint8_t *ypix[SSZ];
	for (sy = 0; sy < SSZ; sy++) {
		if (sy <= S0)
			ypix[sy] = pixel_array;
		else if (sy - S0 >= dim_org->height)
			ypix[sy] = ypix[sy - 1];
		else
			ypix[sy] = &pixel_array[y_stride * (sy - S0)];
	}

	/* iy and ix track the input pixel corresponding to sample[S0][S0]. */
	iy = 0;
	for (oy = 0; oy < dim->height; oy++, p.y += dir) {
		struct rgb_color sample[SSZ][SSZ];

		/* Like with X weights, we also cache all Y weights. */
		fpmath_t iyfp = fpfrac(oy * dim_org->height, dim->height);
		fpmath_t weight_y[SSZ];
		for (sy = 0; sy < SSZ; sy++)
			weight_y[sy] = lanczos_weight(iyfp, sy);

		/*
		 * If we have a new input pixel line between the last oy and
		 * this one, we have to adjust iy forward. When upscaling, this
		 * is not always the case for each new output line. When
		 * downscaling, we may even cross more than one line per output
		 * pixel.
		 */
		while (fpfloor(iyfp) > iy) {
			iy++;

			/* Shift ypix array up to center around next iy line. */
			for (sy = 0; sy < SSZ - 1; sy++)
				ypix[sy] = ypix[sy + 1];

			/* Calculate the last ypix that is being shifted in,
			   but beware of reaching the end of the input image. */
			if (iy + LNCZ_A < dim_org->height)
				ypix[SSZ - 1] = &pixel_array[y_stride *
							     (iy + LNCZ_A)];
		}

		/*
		 * Initialize the sample array for this line, and also
		 * the equals counter, which counts how many of the latest
		 * pixels were exactly equal.
		 */
		int equals = 0;
		uint8_t last_equal = ypix[0][0];
		for (sx = 0; sx < SSZ; sx++) {
			for (sy = 0; sy < SSZ; sy++) {
				if (sx - S0 >= dim_org->width) {
					sample[sx][sy] = sample[sx - 1][sy];
					equals++;
					continue;
				}
				/*
				 * For pixels to the left of S0 there are no
				 * corresponding input pixels so just use
				 * ypix[sy][0].
				 */
				uint8_t i = ypix[sy][max(0, sx - S0)];
				if (pal_to_rgb(i, pal, header->colors_used,
					       &sample[sx][sy]))
					goto bitmap_error;
				if (i == last_equal) {
					equals++;
				} else {
					last_equal = i;
					equals = 1;
				}
			}
		}

		ix = 0;
		p.x = top_left->x;
		for (ox = 0; ox < dim->width; ox++, p.x++) {
			/* Adjust ix forward, same as iy above. */
			fpmath_t ixfp = fpfrac(ox * dim_org->width, dim->width);
			while (fpfloor(ixfp) > ix) {
				ix++;

				/*
				 * We want to reuse the sample columns we
				 * already have, but we don't want to copy them
				 * all around for every new column either.
				 * Instead, treat the X dimension of the sample
				 * array like a ring buffer indexed by ix. rx is
				 * the ringbuffer-adjusted offset of the new
				 * column in sample (the rightmost one) we're
				 * trying to fill.
				 */
				int rx = (SSZ - 1 + ix) % SSZ;
				for (sy = 0; sy < SSZ; sy++) {
					if (ix + LNCZ_A >= dim_org->width) {
						sample[rx][sy] = sample[(SSZ - 2
							+ ix) % SSZ][sy];
						equals++;
						continue;
					}
					uint8_t i = ypix[sy][ix + LNCZ_A];
					if (i == last_equal) {
						if (equals++ >= (SSZ * SSZ))
							continue;
					} else {
						last_equal = i;
						equals = 1;
					}
					if (pal_to_rgb(i, pal,
						       header->colors_used,
						       &sample[rx][sy]))
						goto bitmap_error;
				}
			}

			/* If all pixels in sample are equal, fast path. */
			if (equals >= (SSZ * SSZ)) {
				set_pixel(&p, calculate_color(&sample[0][0],
							      invert));
				continue;
			}

			fpmath_t red = fp(0);
			fpmath_t green = fp(0);
			fpmath_t blue = fp(0);
			for (sy = 0; sy < SSZ; sy++) {
				for (sx = 0; sx < SSZ; sx++) {
					int rx = (sx + ix) % SSZ;
					fpmath_t weight = fpmul(weight_x[ox][sx],
								weight_y[sy]);
					red = fpadd(red, fpmuli(weight,
						sample[rx][sy].red));
					green = fpadd(green, fpmuli(weight,
						sample[rx][sy].green));
					blue = fpadd(blue, fpmuli(weight,
						sample[rx][sy].blue));
				}
			}

			/*
			 * Weights *should* sum up to 1.0 (making this not
			 * necessary) but just to hedge against rounding errors
			 * we should clamp color values to their legal limits.
			 */
			struct rgb_color rgb = {
				.red = max(0, min(UINT8_MAX, fpround(red))),
				.green = max(0, min(UINT8_MAX, fpround(green))),
				.blue = max(0, min(UINT8_MAX, fpround(blue))),
			};

			set_pixel(&p, calculate_color(&rgb, invert));
		}
	}

	free(weight_x);
	return CBGFX_SUCCESS;

bitmap_error:
	free(weight_x);
	return CBGFX_ERROR_BITMAP_DATA;
}

static int get_bitmap_file_header(const void *bitmap, size_t size,
				  struct bmp_header *hdr)
{
	const struct bmp_header *fh;

	if (sizeof(*hdr) > size) {
		log_err("Invalid bitmap data\n");
		return CBGFX_ERROR_BITMAP_DATA;
	}
	fh = (struct bmp_header *)bitmap;
	if (fh->signature[0] != 'B' || fh->signature[1] != 'M') {
		log_err("Bitmap signature mismatch\n");
		return CBGFX_ERROR_BITMAP_SIGNATURE;
	}
	hdr->file_size = le32_to_cpu(fh->file_size);
	if (hdr->file_size != size) {
		log_err("Bitmap file size does not match cbfs file size\n");
		return CBGFX_ERROR_BITMAP_DATA;
	}
	hdr->data_offset = le32_to_cpu(fh->data_offset);

	return CBGFX_SUCCESS;
}

/**
 * parse_header() - Parse a BMP v3 header return
 *
 * @bitmap:	BMP bitmap to parse
 * @size:	Size of BMP file
 * @header:	Returns header
 * @palette:	Returns colour palette
 * @pixel_array: Returns a pointer to the pixel data
 * @dim_ord:	Returns image width/heght in pixels
 * @return 0 if OK, other value on error
 */
static int parse_header(const u8 *bitmap, size_t size,
			struct bmp_header *header,
			const struct bmp_color_table_entry **palette,
			const u8 **pixel_array,
			struct vector *dim_org)
{
	struct bmp_header file_header;
	struct bmp_header *h;
	int rv;

	rv = get_bitmap_file_header(bitmap, size, &file_header);
	if (rv)
		return rv;

	size_t header_size = sizeof(struct bmp_header) - 14;
	size_t palette_offset = sizeof(struct bmp_header);
	size_t file_size = file_header.file_size;

	h = (struct bmp_header *)bitmap;
	header->size = le32_to_cpu(h->size);
	if (header->size != header_size) {
		log_err("Unsupported bitmap format\n");
		return CBGFX_ERROR_BITMAP_FORMAT;
	}

	header->width = le32_to_cpu(h->width);
	header->height = le32_to_cpu(h->height);
	if (header->width == 0 || header->height == 0) {
		log_err("Invalid image width or height\n");
		return CBGFX_ERROR_BITMAP_DATA;
	}
	dim_org->width = header->width;
	dim_org->height = ABS(header->height);

	header->bit_count = le16_to_cpu(h->bit_count);
	header->compression = le32_to_cpu(h->compression);
	header->image_size = le32_to_cpu(h->image_size);
	header->colors_used = le32_to_cpu(h->colors_used);
	size_t palette_size = header->colors_used
			* sizeof(struct bmp_color_table_entry);
	size_t pixel_offset = file_header.data_offset;

	if (pixel_offset > file_size) {
		log_err("Bitmap pixel data exceeds buffer boundary\n");
		return CBGFX_ERROR_BITMAP_DATA;
	}
	if (palette_offset + palette_size > pixel_offset) {
		log_err("Bitmap palette data exceeds palette boundary\n");
		return CBGFX_ERROR_BITMAP_DATA;
	}
	*palette = (struct bmp_color_table_entry *)(bitmap +
			palette_offset);

	size_t pixel_size = header->image_size;

	if (pixel_size != dim_org->height *
		ROUNDUP(dim_org->width * header->bit_count / 8, 4)) {
		log_err("Bitmap pixel array size does not match expected size\n");
		return CBGFX_ERROR_BITMAP_DATA;
	}
	if (pixel_offset + pixel_size > file_size) {
		log_err("Bitmap pixel array exceeds buffer boundary\n");
		return CBGFX_ERROR_BITMAP_DATA;
	}
	*pixel_array = bitmap + pixel_offset;

	return CBGFX_SUCCESS;
}

/*
 * This calculates the dimension of the image projected on the canvas from the
 * dimension relative to the canvas size. If either width or height is zero, it
 * is derived from the other (non-zero) value to keep the aspect ratio.
 */
static int calculate_dimension(const struct vector *dim_org,
			       const struct scale *dim_rel,
			       struct vector *dim)
{
	if (dim_rel->x.n == 0 && dim_rel->y.n == 0)
		return CBGFX_ERROR_INVALID_PARAMETER;

	if (dim_rel->x.n > dim_rel->x.d || dim_rel->y.n > dim_rel->y.d)
		return CBGFX_ERROR_INVALID_PARAMETER;

	if (dim_rel->x.n > 0) {
		if (!is_valid_fraction(&dim_rel->x))
			return CBGFX_ERROR_INVALID_PARAMETER;
		dim->width = canvas.size.width  * dim_rel->x.n / dim_rel->x.d;
	}
	if (dim_rel->y.n > 0) {
		if (!is_valid_fraction(&dim_rel->y))
			return CBGFX_ERROR_INVALID_PARAMETER;
		dim->height = canvas.size.height * dim_rel->y.n / dim_rel->y.d;
	}

	/* Derive height from width using aspect ratio */
	if (dim_rel->y.n == 0)
		dim->height = dim->width * dim_org->height / dim_org->width;
	/* Derive width from height using aspect ratio */
	if (dim_rel->x.n == 0)
		dim->width = dim->height * dim_org->width / dim_org->height;

	return CBGFX_SUCCESS;
}

static int calculate_position(const struct vector *dim,
			      const struct scale *pos_rel, u8 pivot,
			      struct vector *top_left)
{
	int rv;

	rv = transform_vector(top_left, &canvas.size, pos_rel, &canvas.offset);
	if (rv)
		return rv;

	switch (pivot & PIVOT_H_MASK) {
	case PIVOT_H_LEFT:
		break;
	case PIVOT_H_CENTER:
		top_left->x -= dim->width / 2;
		break;
	case PIVOT_H_RIGHT:
		top_left->x -= dim->width;
		break;
	default:
		return CBGFX_ERROR_INVALID_PARAMETER;
	}

	switch (pivot & PIVOT_V_MASK) {
	case PIVOT_V_TOP:
		break;
	case PIVOT_V_CENTER:
		top_left->y -= dim->height / 2;
		break;
	case PIVOT_V_BOTTOM:
		top_left->y -= dim->height;
		break;
	default:
		return CBGFX_ERROR_INVALID_PARAMETER;
	}

	return CBGFX_SUCCESS;
}

static int check_boundary(const struct vector *top_left,
			  const struct vector *dim,
			  const struct rect *bound)
{
	struct vector v;

	add_vectors(&v, dim, top_left);
	if (top_left->x < bound->offset.x || top_left->y < bound->offset.y ||
	    within_box(&v, bound) < 0)
		return CBGFX_ERROR_BOUNDARY;
	return CBGFX_SUCCESS;
}

int draw_bitmap(const void *bitmap, size_t size,
		const struct scale *pos_rel, const struct scale *dim_rel,
		u32 flags)
{
	struct bmp_header header;
	const struct bmp_color_table_entry *palette;
	const u8 *pixel_array;
	struct vector top_left, dim, dim_org;
	int rv;
	const u8 pivot = flags & PIVOT_MASK;
	const u8 invert = (flags & INVERT_COLORS) >> INVERT_SHIFT;

	/* only v3 is supported now */
	rv = parse_header(bitmap, size, &header, &palette, &pixel_array,
			  &dim_org);
	if (rv)
		return rv;

	/* Calculate height and width of the image */
	rv = calculate_dimension(&dim_org, dim_rel, &dim);
	if (rv)
		return rv;

	/* Calculate coordinate */
	rv = calculate_position(&dim, pos_rel, pivot, &top_left);
	if (rv)
		return rv;

	rv = check_boundary(&top_left, &dim, &canvas);
	if (rv) {
		log_err("Bitmap image exceeds canvas boundary\n");
		return rv;
	}

	return draw_bitmap_v3(&top_left, &dim, &dim_org,
			      &header, palette, pixel_array, invert);
}

int draw_bitmap_direct(const void *bitmap, size_t size,
		       const struct vector *top_left)
{
	struct bmp_header header;
	const struct bmp_color_table_entry *palette;
	const uint8_t *pixel_array;
	struct vector dim;
	int rv;

	/* only v3 is supported now */
	rv = parse_header(bitmap, size,
				    &header, &palette, &pixel_array, &dim);
	if (rv)
		return rv;

	rv = check_boundary(top_left, &dim, &screen);
	if (rv) {
		log_warning("Bitmap image exceeds screen boundary\n");
		return rv;
	}

	return draw_bitmap_v3(top_left, &dim, &dim,
			      &header, palette, pixel_array, 0);
}

int get_bitmap_dimension(const void *bitmap, size_t sz, struct scale *dim_rel)
{
	struct bmp_header header;
	const struct bmp_color_table_entry *palette;
	const u8 *pixel_array;
	struct vector dim, dim_org;
	int rv;

	rv = parse_header(bitmap, sz, &header, &palette, &pixel_array,
			  &dim_org);
	if (rv)
		return rv;

	/* Calculate height and width of the image */
	rv = calculate_dimension(&dim_org, dim_rel, &dim);
	if (rv)
		return rv;

	/* Calculate size relative to the canvas */
	dim_rel->x.n = dim.width;
	dim_rel->x.d = canvas.size.width;
	dim_rel->y.n = dim.height;
	dim_rel->y.d = canvas.size.height;

	return CBGFX_SUCCESS;
}

int enable_graphics_buffer(void)
{
	struct vboot_info *vboot = vboot_get();
	const struct video_uc_plat *plat;
	int ret;

	if (gfx_buffer)
		return CBGFX_SUCCESS;

	ret = uclass_first_device_err(UCLASS_VIDEO, &vboot->video);
	if (ret) {
		log_err("Cannot find video device (err=%d)\n", ret);
		return VB2_ERROR_UNKNOWN;
	}

	ret = uclass_first_device_err(UCLASS_VIDEO_CONSOLE, &vboot->console);
	if (ret) {
		log_err("Cannot find console device (err=%d)\n", ret);
		return VB2_ERROR_UNKNOWN;
	}

	ret = uclass_first_device_err(UCLASS_PANEL, &vboot->panel);
	if (ret)
		log_warning("No panel found (cannot adjust backlight)\n");

	if (cbgfx_init(vboot->video))
		return CBGFX_ERROR_INIT;

	plat = dev_get_uclass_plat(vboot->video);
	gfx_buffer = map_sysmem(plat->base, plat->size);
	if (!gfx_buffer)
		return CBGFX_ERROR_FRAMEBUFFER_ADDR;

	return CBGFX_SUCCESS;
}

int flush_graphics_buffer(void)
{
	if (!gfx_buffer)
		return CBGFX_ERROR_GRAPHICS_BUFFER;

	memcpy(REAL_FB, gfx_buffer, fbinfo->y_resolution * fbinfo->bytes_per_line);
	return CBGFX_SUCCESS;
}

void disable_graphics_buffer(void)
{
	free(gfx_buffer);
	gfx_buffer = NULL;
}
