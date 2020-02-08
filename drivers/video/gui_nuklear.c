// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2020 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <binman.h>
#include <dm.h>
#include <gui.h>
#include <malloc.h>
#include <mouse.h>
#include <os.h>
#include <spi_flash.h>
#include <spi.h>
#include <video.h>
#include <video_console.h>
#include <dm/uclass-internal.h>
#include <nuklear/gui.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#define NK_RAWFB_IMPLEMENTATION
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_SOFTWARE_FONT

#define NK_MEMSET memset

#define STBI_NO_STDIO
#include "nuklear/stb_image.h"

void perror(const char *str)
{
	printf("Nuklear error: %s\n", str);
}

float ceilf(float fval)
{
	long val;

	if (fval >= 0)
		val = (long)(fval + 0.999999);
	else
		val = -(long)(-fval + 0.999999);

	return val;
}

#include <nuklear/nuklear.h>
#include <nuklear/nuklear_rawfb.h>

/**
 * struct gui_nuklear_priv - Private data for the driver
 *
 * @rawfb: Raw frame buffer information
 * @ctx: Nuklear context
 * @theme: Name of theme to use (e.g. "red")
 * @tex_scratch: FIXME: Not used
 */
struct gui_nuklear_priv {
	void *fb;
	int fb_size;
	struct rawfb_context *rawfb;
	struct nk_context *ctx;
	const char *theme;
	u8 *tex_scratch;
	struct nk_user_font *font_default;
	struct nk_user_font *font_bold;
	struct nk_user_font *font_large;
	struct nuklear_info info;
};

enum theme {
	THEME_BLACK_DEFAULT,
	THEME_WHITE,
	THEME_RED,
	THEME_BLUE,
	THEME_DARK,

	THEME_COUNT,
};

const char *const theme_name[THEME_COUNT] = {
	"default",
	"white",
	"red",
	"blue",
	"dark",
};

struct nk_color table[THEME_COUNT][NK_COLOR_COUNT] = {
	[THEME_WHITE] = {
		[NK_COLOR_TEXT] = {70, 70, 70, 255},
		[NK_COLOR_WINDOW] = {175, 175, 175, 255},
		[NK_COLOR_HEADER] = {175, 175, 175, 255},
		[NK_COLOR_BORDER] = {0, 0, 0, 255},
		[NK_COLOR_BUTTON] = {185, 185, 185, 255},
		[NK_COLOR_BUTTON_HOVER] = {170, 170, 170, 255},
		[NK_COLOR_BUTTON_ACTIVE] = {160, 160, 160, 255},
		[NK_COLOR_TOGGLE] = {150, 150, 150, 255},
		[NK_COLOR_TOGGLE_HOVER] = {120, 120, 120, 255},
		[NK_COLOR_TOGGLE_CURSOR] = {175, 175, 175, 255},
		[NK_COLOR_SELECT] = {190, 190, 190, 255},
		[NK_COLOR_SELECT_ACTIVE] = {175, 175, 175, 255},
		[NK_COLOR_SLIDER] = {190, 190, 190, 255},
		[NK_COLOR_SLIDER_CURSOR] = {80, 80, 80, 255},
		[NK_COLOR_SLIDER_CURSOR_HOVER] = {70, 70, 70, 255},
		[NK_COLOR_SLIDER_CURSOR_ACTIVE] = {60, 60, 60, 255},
		[NK_COLOR_PROPERTY] = {175, 175, 175, 255},
		[NK_COLOR_EDIT] = {150, 150, 150, 255},
		[NK_COLOR_EDIT_CURSOR] = {0, 0, 0, 255},
		[NK_COLOR_COMBO] = {175, 175, 175, 255},
		[NK_COLOR_CHART] = {160, 160, 160, 255},
		[NK_COLOR_CHART_COLOR] = {45, 45, 45, 255},
		[NK_COLOR_CHART_COLOR_HIGHLIGHT] = { 255, 0, 0, 255},
		[NK_COLOR_SCROLLBAR] = {180, 180, 180, 255},
		[NK_COLOR_SCROLLBAR_CURSOR] = {140, 140, 140, 255},
		[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = {150, 150, 150, 255},
		[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = {160, 160, 160, 255},
		[NK_COLOR_TAB_HEADER] = {180, 180, 180, 255},
	},
	[THEME_RED] = {
		[NK_COLOR_TEXT] = {190, 190, 190, 255},
		[NK_COLOR_WINDOW] = {30, 33, 40, 215},
		[NK_COLOR_HEADER] = {181, 45, 69, 220},
		[NK_COLOR_BORDER] = {51, 55, 67, 255},
		[NK_COLOR_BUTTON] = {181, 45, 69, 255},
		[NK_COLOR_BUTTON_HOVER] = {190, 50, 70, 255},
		[NK_COLOR_BUTTON_ACTIVE] = {195, 55, 75, 255},
		[NK_COLOR_TOGGLE] = {51, 55, 67, 255},
		[NK_COLOR_TOGGLE_HOVER] = {45, 60, 60, 255},
		[NK_COLOR_TOGGLE_CURSOR] = {181, 45, 69, 255},
		[NK_COLOR_SELECT] = {51, 55, 67, 255},
		[NK_COLOR_SELECT_ACTIVE] = {181, 45, 69, 255},
		[NK_COLOR_SLIDER] = {51, 55, 67, 255},
		[NK_COLOR_SLIDER_CURSOR] = {181, 45, 69, 255},
		[NK_COLOR_SLIDER_CURSOR_HOVER] = {186, 50, 74, 255},
		[NK_COLOR_SLIDER_CURSOR_ACTIVE] = {191, 55, 79, 255},
		[NK_COLOR_PROPERTY] = {51, 55, 67, 255},
		[NK_COLOR_EDIT] = {51, 55, 67, 225},
		[NK_COLOR_EDIT_CURSOR] = {190, 190, 190, 255},
		[NK_COLOR_COMBO] = {51, 55, 67, 255},
		[NK_COLOR_CHART] = {51, 55, 67, 255},
		[NK_COLOR_CHART_COLOR] = {170, 40, 60, 255},
		[NK_COLOR_CHART_COLOR_HIGHLIGHT] = { 255, 0, 0, 255},
		[NK_COLOR_SCROLLBAR] = {30, 33, 40, 255},
		[NK_COLOR_SCROLLBAR_CURSOR] = {64, 84, 95, 255},
		[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = {70, 90, 100, 255},
		[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = {75, 95, 105, 255},
		[NK_COLOR_TAB_HEADER] = {181, 45, 69, 220},
	},
	[THEME_BLUE] = {
		[NK_COLOR_TEXT] = {20, 20, 20, 255},
		[NK_COLOR_WINDOW] = {202, 212, 214, 215},
		[NK_COLOR_HEADER] = {137, 182, 224, 220},
		[NK_COLOR_BORDER] = {140, 159, 173, 255},
		[NK_COLOR_BUTTON] = {137, 182, 224, 255},
		[NK_COLOR_BUTTON_HOVER] = {142, 187, 229, 255},
		[NK_COLOR_BUTTON_ACTIVE] = {147, 192, 234, 255},
		[NK_COLOR_TOGGLE] = {177, 210, 210, 255},
		[NK_COLOR_TOGGLE_HOVER] = {182, 215, 215, 255},
		[NK_COLOR_TOGGLE_CURSOR] = {137, 182, 224, 255},
		[NK_COLOR_SELECT] = {177, 210, 210, 255},
		[NK_COLOR_SELECT_ACTIVE] = {137, 182, 224, 255},
		[NK_COLOR_SLIDER] = {177, 210, 210, 255},
		[NK_COLOR_SLIDER_CURSOR] = {137, 182, 224, 245},
		[NK_COLOR_SLIDER_CURSOR_HOVER] = {142, 188, 229, 255},
		[NK_COLOR_SLIDER_CURSOR_ACTIVE] = {147, 193, 234, 255},
		[NK_COLOR_PROPERTY] = {210, 210, 210, 255},
		[NK_COLOR_EDIT] = {210, 210, 210, 225},
		[NK_COLOR_EDIT_CURSOR] = {20, 20, 20, 255},
		[NK_COLOR_COMBO] = {210, 210, 210, 255},
		[NK_COLOR_CHART] = {210, 210, 210, 255},
		[NK_COLOR_CHART_COLOR] = {137, 182, 224, 255},
		[NK_COLOR_CHART_COLOR_HIGHLIGHT] = { 255, 0, 0, 255},
		[NK_COLOR_SCROLLBAR] = {190, 200, 200, 255},
		[NK_COLOR_SCROLLBAR_CURSOR] = {64, 84, 95, 255},
		[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = {70, 90, 100, 255},
		[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = {75, 95, 105, 255},
		[NK_COLOR_TAB_HEADER] = {156, 193, 220, 255},
	},
	[THEME_DARK] = {
		[NK_COLOR_TEXT] = {210, 210, 210, 255},
		[NK_COLOR_WINDOW] = {57, 67, 71, 215},
		[NK_COLOR_HEADER] = {51, 51, 56, 220},
		[NK_COLOR_BORDER] = {46, 46, 46, 255},
		[NK_COLOR_BUTTON] = {48, 83, 111, 255},
		[NK_COLOR_BUTTON_HOVER] = {58, 93, 121, 255},
		[NK_COLOR_BUTTON_ACTIVE] = {63, 98, 126, 255},
		[NK_COLOR_TOGGLE] = {50, 58, 61, 255},
		[NK_COLOR_TOGGLE_HOVER] = {45, 53, 56, 255},
		[NK_COLOR_TOGGLE_CURSOR] = {48, 83, 111, 255},
		[NK_COLOR_SELECT] = {57, 67, 61, 255},
		[NK_COLOR_SELECT_ACTIVE] = {48, 83, 111, 255},
		[NK_COLOR_SLIDER] = {50, 58, 61, 255},
		[NK_COLOR_SLIDER_CURSOR] = {48, 83, 111, 245},
		[NK_COLOR_SLIDER_CURSOR_HOVER] = {53, 88, 116, 255},
		[NK_COLOR_SLIDER_CURSOR_ACTIVE] = {58, 93, 121, 255},
		[NK_COLOR_PROPERTY] = {50, 58, 61, 255},
		[NK_COLOR_EDIT] = {50, 58, 61, 225},
		[NK_COLOR_EDIT_CURSOR] = {210, 210, 210, 255},
		[NK_COLOR_COMBO] = {50, 58, 61, 255},
		[NK_COLOR_CHART] = {50, 58, 61, 255},
		[NK_COLOR_CHART_COLOR] = {48, 83, 111, 255},
		[NK_COLOR_CHART_COLOR_HIGHLIGHT] = {255, 0, 0, 255},
		[NK_COLOR_SCROLLBAR] = {50, 58, 61, 255},
		[NK_COLOR_SCROLLBAR_CURSOR] = {48, 83, 111, 255},
		[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = {53, 88, 116, 255},
		[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = {58, 93, 121, 255},
		[NK_COLOR_TAB_HEADER] = {48, 83, 111, 255},
	},
};

static void set_style(struct nk_context *ctx, enum theme theme)
{
	if (theme == THEME_BLACK_DEFAULT)
		nk_style_default(ctx);
	else
		nk_style_from_table(ctx, table[theme]);
}

NK_LIB void *nk_malloc(nk_handle unused, void *old,nk_size size)
{
	NK_UNUSED(unused);
	NK_UNUSED(old);

	return malloc(size);
}

NK_LIB void nk_mfree(nk_handle unused, void *ptr)
{
	NK_UNUSED(unused);
	free(ptr);
}

NK_API void nk_buffer_init_default(struct nk_buffer *buffer)
{
	struct nk_allocator alloc;

	alloc.userdata.ptr = 0;
	alloc.alloc = nk_malloc;
	alloc.free = nk_mfree;
	nk_buffer_init(buffer, &alloc, 4096);
}

static struct nk_font *add_font(struct nk_font_atlas *atlas, const char *name,
				int height)
{
	struct nk_font_config cfg;
	struct nk_font *font;
	u8 *data;
	int size;

	data = console_truetype_find_font(name, &size);

	cfg = nk_font_config(height);
	cfg.ttf_blob = data;
	cfg.ttf_size = size;
	cfg.size = height;
	cfg.ttf_data_owned_by_atlas = 1;

	font = nk_font_atlas_add(atlas, &cfg);

	return font;
}

NK_API int nuk_add_fonts(struct nk_font_atlas *atlas, void *userdata_ptr)
{
	struct udevice *dev = userdata_ptr;
	struct gui_nuklear_priv *priv = dev_get_priv(dev);
	struct nk_font *font;

	font = add_font(atlas, "nimbus_sans_l_regular", 20);
	priv->font_default = &font->handle;
	atlas->default_font = font;

	font = add_font(atlas, "nimbus_sans_l_bold", 20);
	priv->font_bold = &font->handle;

	font = add_font(atlas, "nimbus_sans_l_bold", 70);
	priv->font_large = &font->handle;

	return true;
}

NK_API void nk_font_atlas_init_default(struct nk_font_atlas *atlas)
{
	NK_ASSERT(atlas);
	if (!atlas)
		return;
	memset(atlas, '\0', sizeof(*atlas));
	atlas->temporary.userdata.ptr = 0;
	atlas->temporary.alloc = nk_malloc;
	atlas->temporary.free = nk_mfree;
	atlas->permanent.userdata.ptr = 0;
	atlas->permanent.alloc = nk_malloc;
	atlas->permanent.free = nk_mfree;
}

NK_API int nk_init_default(struct nk_context *ctx,
                           const struct nk_user_font *font)
{
	struct udevice *dev = ctx->userdata.ptr;
	struct gui_nuklear_priv *priv = dev_get_priv(dev);
	struct nk_allocator alloc;
	enum theme theme;
	int ret, i;

	alloc.userdata.ptr = 0;
	alloc.alloc = nk_malloc;
	alloc.free = nk_mfree;

	ret = nk_init(ctx, &alloc, font);
	if (!ret)
		return ret;

	theme = THEME_BLACK_DEFAULT;
	for (i = 0; i < THEME_COUNT; i++)
		if (!strcmp(priv->theme, theme_name[i])) {
			theme = i;
			break;
		}
	set_style(ctx, theme);

	return 1;
}

int gui_nuklear_add_image(struct udevice *dev, const char *name,
			  struct nk_image *img)
{
	struct gui_nuklear_priv *priv = dev_get_priv(dev);
	struct rawfb_image *rimg;
	int w, h, comp;
        int img_num;
	void *buf;
	u8 *data;
	int size;
	int ret;

	if (priv->rawfb->num_images == RAWFB_MAX_IMAGES)
		return log_msg_ret("too many", -ENOSPC);
	if (IS_ENABLED(CONFIG_SANDBOX)) {
		char fname[256];

		snprintf(fname, sizeof(fname), "tools/logos/%s.png", name);
		ret = os_read_file(fname, &buf, &size);
	        if (ret)
	            return log_msg_ret("load", ret);
	} else {
		struct binman_entry entry;
		struct udevice *sf;

		ret = binman_entry_find(name, &entry);
		if (ret)
			return log_msg_ret("binman", ret);

		/* Just use the SPI driver to get the memory map */
		ret = uclass_first_device_err(UCLASS_SPI_FLASH, &sf);
		if (ret)
			return log_msg_ret("Cannot get SPI flash", ret);
		size = entry.size;
		buf = malloc(size);
		if (!buf)
			return log_msg_ret("buf", -ENOMEM);
		ret = spi_flash_read_dm(sf, entry.image_pos, size, buf);
		if (ret)
			return log_msg_ret("sf", ret);

	}
	data = stbi_load_from_memory(buf, size, &w, &h, &comp, 0);

	img_num = priv->rawfb->num_images++;
	rimg = &priv->rawfb->img[img_num];
	rimg->pixels = data;
	rimg->w = w;
	rimg->h = h;
	rimg->pitch = w * 4;
	rimg->pl = PIXEL_LAYOUT_XRGB_8888;
	rimg->format = NK_FONT_ATLAS_RGBA32;

	img->handle.id = img_num + 1;
	img->w = w;
	img->h = w;
	img->region[0] = 0;
	img->region[1] = 0;
	img->region[2] = w;
	img->region[3] = h;

	return 0;
}

static int gui_nuklear_ofdata_to_platdata(struct udevice *dev)
{
	struct gui_nuklear_priv *priv = dev_get_priv(dev);

	priv->theme = dev_read_string(dev, "theme");

	return 0;
}

static int gui_nuklear_probe(struct udevice *dev)
{
	struct udevice *vid = dev_get_parent(dev);
	struct video_priv *upriv = dev_get_uclass_priv(vid);
	struct gui_nuklear_priv *priv = dev_get_priv(dev);

	if (!IS_ENABLED(CONFIG_SANDBOX)) {
		priv->fb_size = (upriv->xsize + 10) * upriv->ysize * 4 ;
		priv->fb = (void *)0x30000000;	// FIXME: Allocate
		if (!priv->fb)
			return log_msg_ret("fb", -ENOMEM);
		memset(priv->fb, '\0', priv->fb_size);
	}

	priv->tex_scratch = malloc(2 << 20);
	if (!priv->tex_scratch)
		return log_msg_ret("mem", -ENOMEM);
	priv->rawfb = nk_rawfb_init(priv->fb ? : upriv->fb, priv->tex_scratch,
				    upriv->xsize, upriv->ysize,
				    upriv->line_length, PIXEL_LAYOUT_XRGB_8888,
				    dev);
	if (!priv->rawfb)
		return log_msg_ret("init", -ENOMEM);
	priv->ctx = &priv->rawfb->ctx;
	priv->rawfb->ctx.userdata.ptr = dev;
	priv->info.ctx = priv->ctx;
	priv->info.font_default = priv->font_default;
	priv->info.font_bold = priv->font_bold;
	priv->info.font_large = priv->font_large;

	return 0;
}

static int gui_nuklear_get_context(struct udevice *dev, void **contextp)
{
	struct gui_nuklear_priv *priv = dev_get_priv(dev);

	*contextp = &priv->info;

	return 0;
}

static int gui_nuklear_start_poll(struct udevice *dev)
{
	struct gui_nuklear_priv *priv = dev_get_priv(dev);

	nk_input_begin(priv->ctx);

	return 0;
}

static int gui_nuklear_process_mouse_event(struct udevice *dev,
					   const struct mouse_event *evt)
{
	struct gui_nuklear_priv *priv = dev_get_priv(dev);

	switch (evt->type) {
	case MOUSE_EV_NULL:
		break;
	case MOUSE_EV_MOTION: {
		const int x = evt->motion.x;
		const int y = evt->motion.y;

		nk_input_motion(priv->ctx, x, y);
		break;
	} case MOUSE_EV_BUTTON: {
		const int x = evt->button.x;
		const int y = evt->button.y;
		enum nk_buttons btn = 0;

		if (evt->button.button == BUTTON_LEFT)
			btn = NK_BUTTON_LEFT;
		else if (evt->button.button == BUTTON_MIDDLE)
			btn = NK_BUTTON_MIDDLE;
		if (evt->button.button == BUTTON_RIGHT)
			btn = NK_BUTTON_RIGHT;
		nk_input_button(priv->ctx, btn, x, y, evt->button.press_state);
		break;
	}
	}

	return 0;
}

static int gui_nuklear_input_done(struct udevice *dev)
{
	struct gui_nuklear_priv *priv = dev_get_priv(dev);

	nk_input_end(priv->ctx);

	return 0;
}

int gui_nuklear_render(struct udevice *dev)
{
	struct gui_nuklear_priv *priv = dev_get_priv(dev);

	nk_rawfb_render(priv->rawfb, nk_rgb(30,30,30), 1);

	/* Handle double-buffering if needed */
	if (priv->fb) {
		struct udevice *vid = dev_get_parent(dev);
		struct video_priv *upriv = dev_get_uclass_priv(vid);

		memcpy(upriv->fb, priv->fb, priv->fb_size);
	}

	return 0;
}

static int gui_nuklear_end_poll(struct udevice *dev)
{
	struct udevice *vid = dev_get_parent(dev);

	video_sync(vid, true);

	return 0;
}

const struct gui_ops gui_nuklear_ops = {
	.get_context	= gui_nuklear_get_context,
	.start_poll	= gui_nuklear_start_poll,
	.process_mouse_event	= gui_nuklear_process_mouse_event,
	.input_done	= gui_nuklear_input_done,
	.render		= gui_nuklear_render,
	.end_poll	= gui_nuklear_end_poll,
};

static const struct udevice_id gui_nuklear_ids[] = {
	{ .compatible = "gui,nuklear" },
	{ }
};

U_BOOT_DRIVER(gui_nuklear) = {
	.name	= "gui_nuklear",
	.id	= UCLASS_GUI,
	.of_match = gui_nuklear_ids,
	.ofdata_to_platdata	= gui_nuklear_ofdata_to_platdata,
	.probe	= gui_nuklear_probe,
	.priv_auto_alloc_size = sizeof(struct gui_nuklear_priv),
	.ops	= &gui_nuklear_ops,
};
