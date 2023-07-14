/*
 ============================================================================
 Name        : tlock_password_dialog.c
 Author      : akaan
 Version     :
 Copyright   : Cornell University - cornell.edu
 Published   : GNU Public License v2 (GPLv2)
 Description : tlock - transparent lock, Ansi-style
 Created     : Apr 8, 2014
 ============================================================================
 */

#include "tlock_password_dialog.h"
#include "tlock.h"
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/cursorfont.h>
#include <X11/Xos.h>		/* for time() */
#include <stdlib.h>

/* abstract container does not have a gc */
typedef struct aContainer {
	Position x, y;
	Dimension width;
	Dimension height;
	Dimension border_width;
	Dimension border_height;
	Dimension shadow;
	Dimension padding;
	Pixel foreground;
	Pixel background;
	Pixel border;
	XFontStruct font;
	Window win;
	GC gc;
	XColor color;
} container_t;

typedef struct aLabel {
	container_t container;
	char *text;
} label_t;

typedef struct aField {
	container_t container;
	char *text;
	char *value;
	Pixmap entry_pixmap;
} field_t;

typedef struct aButton {
	container_t container;
	Window parent;
	char *text;
	Bool button_down_p;
} button_t;

typedef struct aDialog {
	container_t container;
	Position x, y;
	Dimension width;
	Dimension height;

	Pixel shadow_top;
	Pixel shadow_bottom;

	field_t user_field;
	field_t password_field;

	button_t cancel_button;
	button_t clear_button;
	button_t login_button;

	Bool passwd_changed_p; /* Whether the user entry field needs redrawing */
	Bool caps_p; /* Whether we saw a keypress with caps-lock on */
	Dimension shadow_width;

	int visible;
	Bool redraw;
} dialog_t;

#define MAX_BYTES_PER_CHAR 8	/* UTF-8 uses no more than 3, I think */
#define MAX_PASSWD_CHARS   128	/* Longest possible passphrase */

Window get_dialog_window(struct aDialog* dialog) {
	return dialog->container.win;
}

void XSelectInputDialog(Display* dsp, dialog_t* dialog) {
	XSelectInput(dsp, dialog->container.win,
	ButtonPressMask | ButtonReleaseMask);
}

Bool is_area_pressed(struct aDialog* dialog, const char* name, int x, int y) {
#ifdef VERBOSE_FLAG
	fprintf(	stderr, "event @  coord[%d,%d] %s\n", x, y, name);
#endif

	if (strcmp("cancel_button", name) == 0) {
		if (x > dialog->container.x + dialog->cancel_button.container.x //
		    && x < dialog->container.x + dialog->cancel_button.container.x
			 + dialog->cancel_button.container.width //
		    && y > dialog->container.y + dialog->cancel_button.container.y
			 + dialog->cancel_button.container.padding //
		    && y < dialog->container.y + dialog->cancel_button.container.y
			 + dialog->cancel_button.container.height
			 + dialog->cancel_button.container.padding
			 + dialog->cancel_button.container.font.ascent) {
#ifdef VERBOSE_FLAG
			LOG_RENDER(
			        "##############switch_button_pressed @ coord[%d,%d]\n",
			        x,
			        y);
#endif
			dialog->cancel_button.button_down_p = 1;
			return True;

		}
		dialog->cancel_button.button_down_p = 0;
	} else if (strcmp("clear_button", name) == 0) {
		/*
		 * field.container.padding, field.container.y + field.container.font.ascent + field.container.padding. 
		 */
		if (x > dialog->container.x + dialog->clear_button.container.x
		    && x < dialog->container.x + dialog->clear_button.container.x
			 + dialog->clear_button.container.width //
		    && y > dialog->container.y + dialog->clear_button.container.y
			 + dialog->clear_button.container.padding //
		    && y < dialog->container.y + dialog->clear_button.container.y
			 + dialog->clear_button.container.height
			 + dialog->clear_button.container.padding
			 + dialog->clear_button.container.font.ascent) {
#ifdef VERBOSE_FLAG
			fprintf(
			        stderr,
			        "##############clear_button_pressed @ coord[%d,%d]\n",
			        x,
			        y);
#endif
			dialog->clear_button.button_down_p = 1;
			return True;
		}
	} else if (strcmp("login_button", name) == 0) {
		/*
		 * field.container.padding, field.container.y + field.container.font.ascent + field.container.padding. 
		 */
		if (x > dialog->container.x + dialog->login_button.container.x
		    && x < dialog->container.x + dialog->login_button.container.x
			 + dialog->login_button.container.width //
		    && y > dialog->container.y + dialog->login_button.container.y
			 + dialog->login_button.container.padding //
		    && y < dialog->container.y + dialog->login_button.container.y
			 + dialog->login_button.container.height
			 + dialog->login_button.container.padding
			 + dialog->login_button.container.font.ascent) {
#ifdef VERBOSE_FLAG
			fprintf(
			        stderr,
			        "##############login_button_pressed @ coord[%d,%d]\n",
			        x,
			        y);
#endif
			dialog->login_button.button_down_p = 1;
			return True;
		}
	} else if (strcmp("user_field", name) == 0) {
		if (x > dialog->container.x + dialog->user_field.container.x //
		    && x < dialog->container.x + dialog->user_field.container.width
			 + dialog->user_field.container.x //
		    && y > dialog->container.y + dialog->user_field.container.y
			 + dialog->user_field.container.padding //
		    && y < dialog->container.y + dialog->user_field.container.y
			 + dialog->user_field.container.height
			 + dialog->user_field.container.padding
			 + dialog->user_field.container.font.ascent) {
#ifdef VERBOSE_FLAG
			fprintf(
			        stderr,
			        "##############user_field_pressed @ coord[%d,%d]\n",
			        x,
			        y);
			fprintf(
			        stderr,
			        " x0 = %d, x1 = %d\n",
			        dialog->container.x + dialog->user_field.container.x,
			        dialog->container.x + dialog->user_field.container.x
				 + dialog->user_field.container.width);
			fprintf(
			        stderr,
			        " y0 = %d, y1 = %d\n",
			        dialog->container.y + dialog->user_field.container.y,
			        dialog->container.y + dialog->user_field.container.y
				 + dialog->user_field.container.height);
#endif
			return True;
		}
	} else if (strcmp("password_field", name) == 0) {
		if (x > dialog->container.x + dialog->password_field.container.x 
		    && x < dialog->container.x + dialog->password_field.container.width
			 + dialog->password_field.container.x 
		    && y > dialog->container.y + dialog->password_field.container.y
			 + dialog->password_field.container.padding 
		    && y < dialog->container.y + dialog->password_field.container.y
			 + dialog->password_field.container.height
			 + dialog->password_field.container.font.ascent) {
#ifdef VERBOSE_FLAG
			fprintf(
			        stderr,
			        "##############password_field_pressed @ coord[%d,%d]\n",
			        x,
			        y);
			fprintf(
			        stderr,
			        " x0 = %d, x1 = %d\n",
			        dialog->container.x + dialog->password_field.container.x,
			        dialog->container.x + dialog->password_field.container.x
				 + dialog->password_field.container.width);
			fprintf(
			        stderr,
			        " y0 = %d, y1 = %d\n",
			        dialog->container.y + dialog->password_field.container.y,
			        dialog->container.y + dialog->password_field.container.y
				 + dialog->password_field.container.height);

#endif
			return True;
		}
	}
	return False;
}
void flag_redraw(dialog_t* dialog) {
	if (dialog) {
		dialog->redraw = True;
	}
}

static void tlock_create_container(container_t* container,
		struct aXInfo* xi,
		int x,
		int y,
		int width,
		int height,
		int border_width) {
	container->x = 0;
	if (x > 0) {
		container->x = x;
	}
	container->y = 0;
	if (y > 0) {
		container->y = y;
	};

	LOG_RENDER("creating container");
	container->width = width;
	container->height = height;
	container->border_width = border_width;
	container->border_height = 1;
	container->padding = 3;

	container->background = BlackPixel(xi->display, 0);
	container->foreground = WhitePixel(xi->display, 0);
	container->border = WhitePixel(xi->display, 0);

	LOG_RENDER("setting font");
	XFontStruct* font = XLoadQueryFont(xi->display, "fixed");
	container->font = *font;

#ifdef RENDER_FLAG
	fprintf(	stderr, "%-20s:%-25s:%03d - %d x %d (w=%d,h=%d) \n",
			__FILE__, __FUNCTION__,	__LINE__, container->x, container->y, container->width, container->height);
#endif

}

/**
 *
 */
dialog_t* tlock_create_dialog(struct aXInfo* xi,
		int x,
		int y,
		int width,
		int height,
		int border_width) {
	dialog_t* dialog;
	XSetWindowAttributes xswa;

	LOG_RENDER("create dialog structure");
	/* create the pointer and memsize for the dialog */
	dialog = (dialog_t*) calloc(1, sizeof(dialog_t));

	if (dialog == 0) {
		return 0;
	};

#ifdef RENDER_FLAG
	("create dialog container");
#endif
	tlock_create_container(&dialog->container, xi, x, y, width, height, 1);

#ifdef RENDER_FLAG
	fprintf(
	        stderr,
	        "%-20s:%-25s:%03d - %d x %d (w=%d,h=%d) \n",
	        __FILE__,
	        __FUNCTION__,
	        __LINE__,
	        dialog->container.x,
	        dialog->container.y,
	        dialog->container.width,
	        dialog->container.height);
#endif

	xswa.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask;
	xi->dialog_window = dialog->container.win;
	dialog->container.win = XCreateWindow(
	        xi->display,
	        xi->root[0],
	        dialog->container.x,
	        dialog->container.y,
	        dialog->container.width,
	        dialog->container.height,
	        border_width,
	        CopyFromParent,
	        InputOutput,
	        CopyFromParent,
	        CWOverrideRedirect | CWColormap,
	        &xswa);
	dialog->visible = 0;
	dialog->redraw = True;

	LOG_RENDER("create dialog gc");
	dialog->container.gc = XCreateGC(xi->display, dialog->container.win, 0, 0);

	XSetWindowBackground(
	        xi->display,
	        dialog->container.win,
	        dialog->container.background);

	XSetWindowBorder(
	        xi->display,
	        dialog->container.win,
	        dialog->container.border);

	LOG_RENDER("create dialog components");

	/* USER FIELD */
	tlock_create_container(
	        &dialog->user_field.container,
	        xi,
	        15,
	        20,
	        245,
	        20,
	        1);
	dialog->user_field.text = strdup("Username");
	dialog->user_field.value = strdup("Password");

#ifdef RENDER_FLAG
	fprintf(
	        stderr,
	        "%-20s:%-25s:%03d - %d x %d (text=%s)\n",
	        __FILE__,
	        __FUNCTION__,
	        __LINE__,
	        dialog->user_field.container.x,
	        dialog->user_field.container.y,
	        dialog->user_field.text);
#endif

	dialog->user_field.container.background = XWhitePixel(xi->display, 0);
	dialog->user_field.container.foreground = XBlackPixel(xi->display, 0);

	dialog->user_field.entry_pixmap = XCreatePixmap(
	        xi->display,
	        dialog->container.win,
	        dialog->user_field.container.width,
	        dialog->user_field.container.height,
	        1);

	/* PASSWORD FIELD */
	tlock_create_container(
	        &dialog->password_field.container,
	        xi,
	        15,
	        60,
	        245,
	        20,
	        1);
	dialog->password_field.text = strdup("Password");
	dialog->password_field.value = strdup("secret99");
	dialog->password_field.container.background = XWhitePixel(xi->display, 0);
	dialog->password_field.container.foreground = XBlackPixel(xi->display, 0);

	/* login button */
	tlock_create_container(
	        &dialog->login_button.container,
	        xi,
	        15,
	        95,
	        75,
	        20,
	        1);
	dialog->login_button.text = strdup("Login");
	dialog->login_button.container.background = XWhitePixel(xi->display,0);
	dialog->login_button.container.foreground = XBlackPixel(xi->display,0);

	/* clear button*/
	tlock_create_container(
	        &dialog->clear_button.container,
	        xi,
	        100,
	        95,
	        75,
	        20,
	        1);
	dialog->clear_button.text = strdup("Clear");
	dialog->clear_button.container.background = XWhitePixel(xi->display, 0);
	dialog->clear_button.container.foreground = XBlackPixel(xi->display, 0);

	/* cancel button */
	tlock_create_container(
	        &dialog->cancel_button.container,
	        xi,
	        185,
	        95,
	        75,
	        20,
	        1);
	dialog->cancel_button.text = strdup("Cancel");
	dialog->cancel_button.container.background = XWhitePixel(xi->display,0);
	dialog->cancel_button.container.foreground = XBlackPixel(xi->display,0);

	

	LOG_RENDER("created dialog");

	return dialog;
}

/* DRAW FUNCTIONS */
static void draw_field(Display* display,
		container_t* parent,
		field_t field,
		char *value, int redraw) {

	if (redraw) {
		XFillRectangle(
			display,
			parent->win,
			parent->gc,
			field.container.x,
			field.container.y,
			field.container.width,
			field.container.height);
	}
	
	//draw text box field
	XDrawRectangle(
		display,
		parent->win,
		parent->gc,
		field.container.x,
		field.container.y + 5,	//padding
		field.container.width,
		field.container.height);
	//draw label of text box
	XDrawString(
		display,
		parent->win,
		parent->gc,
		field.container.x,
		field.container.y,
		field.text,
		strlen(field.text));
	//draw text within textbox
	XDrawString(
		display,
		parent->win,
		parent->gc,
		field.container.x + field.container.padding,
		field.container.y + field.container.font.ascent + field.container.padding + 5,
		value,
		strlen(value));

}

static void draw_button(Display* display, container_t* parent, button_t button) {
	XDrawRectangle(
		display,
		parent->win,
		parent->gc,
		button.container.x,
		button.container.y,
		button.container.width,
		button.container.height);

	XDrawString(
		display,
		parent->win,
		parent->gc,
		button.container.x + button.container.padding,
		button.container.y + button.container.font.ascent + button.container.padding,
		button.text,
		strlen(button.text));
}

void tlock_draw_dialog(struct aXInfo* xi,
        dialog_t* dialog,
        char* nameref,
        char* pwdref)
{
	XGCValues gcv;
	XColor tmp;


#ifdef RENDER_FLAG
	LOG_RENDER("tlock: draw dialog");
#endif
	if (!xi)
	{
		return;
	}
	if (!dialog)
	{
		return;
	}

	if (!dialog->visible)
	{
		tlock_show_dialog(xi, dialog);
	}

	if (dialog->redraw)
	{
		XAllocNamedColor(
		        xi->display,
		        xi->colormap[0],
		        "black",
		        &dialog->container.color,
		        &tmp);
		gcv.foreground = dialog->container.color.pixel;
		gcv.background = dialog->container.color.pixel;

		XChangeGC(xi->display, dialog->container.gc, GCForeground, &gcv);
		XFillRectangle(
		        xi->display,
		        dialog->container.win,
		        dialog->container.gc,
		        0,
		        0,
		        dialog->container.width,
		        dialog->container.height);
		dialog->redraw = False;
	}

	XAllocNamedColor(
	        xi->display,
	        xi->colormap[0],
	        "white",
	        &dialog->container.color,
	        &tmp);
	gcv.foreground = dialog->container.color.pixel;

	XChangeGC(xi->display, dialog->container.gc, GCForeground, &gcv);
	XDrawRectangle(
	        xi->display,
	        dialog->container.win,
	        dialog->container.gc,
	        0,
	        0,
	        dialog->container.width,
	        dialog->container.height);

	XAllocNamedColor(
	        xi->display,
	        xi->colormap[0],
	        "white",
	        &dialog->user_field.container.color,
	        &tmp);
	gcv.foreground = dialog->user_field.container.color.pixel;

	XChangeGC(xi->display, dialog->container.gc, GCForeground, &gcv);

	switch (tabpos) {
		case 0:
			draw_field(xi->display, &dialog->container, dialog->password_field, pwdref, dialog->redraw);
			draw_button(xi->display, &dialog->container, dialog->login_button);
			draw_button(xi->display, &dialog->container, dialog->clear_button);
			draw_button(xi->display, &dialog->container, dialog->cancel_button);
			XAllocNamedColor(
	        		xi->display,
	        		xi->colormap[0],
	        		"yellow",
	        		&dialog->container.color,
	        		&tmp);
			gcv.foreground = dialog->container.color.pixel;
			XChangeGC(xi->display, dialog->container.gc, GCForeground, &gcv);
			draw_field(xi->display, &dialog->container, dialog->user_field, nameref, dialog->redraw);
			break;
		case 1:
			draw_field(xi->display, &dialog->container, dialog->user_field, nameref, dialog->redraw);
			draw_button(xi->display, &dialog->container, dialog->login_button);
			draw_button(xi->display, &dialog->container, dialog->clear_button);
			draw_button(xi->display, &dialog->container, dialog->cancel_button);
			XAllocNamedColor(
	        		xi->display,
	        		xi->colormap[0],
	        		"yellow",
	        		&dialog->container.color,
	        		&tmp);
			gcv.foreground = dialog->container.color.pixel;
			XChangeGC(xi->display, dialog->container.gc, GCForeground, &gcv);
			draw_field(xi->display, &dialog->container, dialog->password_field, pwdref, dialog->redraw);
			break;
		case 2:
			draw_field(xi->display, &dialog->container, dialog->user_field, nameref, dialog->redraw);
			draw_field(xi->display, &dialog->container, dialog->password_field, pwdref, dialog->redraw);
			draw_button(xi->display, &dialog->container, dialog->clear_button);
			draw_button(xi->display, &dialog->container, dialog->cancel_button);
			XAllocNamedColor(
	        		xi->display,
	        		xi->colormap[0],
	        		"yellow",
	        		&dialog->container.color,
	        		&tmp);
			gcv.foreground = dialog->container.color.pixel;
			XChangeGC(xi->display, dialog->container.gc, GCForeground, &gcv);
			draw_button(xi->display, &dialog->container, dialog->login_button);
			break;
		case 3:
			draw_field(xi->display, &dialog->container, dialog->user_field, nameref, dialog->redraw);
			draw_field(xi->display, &dialog->container, dialog->password_field, pwdref, dialog->redraw);
			draw_button(xi->display, &dialog->container, dialog->login_button);
			draw_button(xi->display, &dialog->container, dialog->cancel_button);
			XAllocNamedColor(
	        		xi->display,
	        		xi->colormap[0],
	        		"yellow",
	        		&dialog->container.color,
	        		&tmp);
			gcv.foreground = dialog->container.color.pixel;
			XChangeGC(xi->display, dialog->container.gc, GCForeground, &gcv);
			draw_button(xi->display, &dialog->container, dialog->clear_button);
			break;
		case 4:
			draw_field(xi->display, &dialog->container, dialog->user_field, nameref, dialog->redraw);
			draw_field(xi->display, &dialog->container, dialog->password_field, pwdref, dialog->redraw);
			draw_button(xi->display, &dialog->container, dialog->login_button);
			draw_button(xi->display, &dialog->container, dialog->clear_button);
			XAllocNamedColor(
	        		xi->display,
	        		xi->colormap[0],
	        		"yellow",
	        		&dialog->container.color,
	        		&tmp);
			gcv.foreground = dialog->container.color.pixel;
			XChangeGC(xi->display, dialog->container.gc, GCForeground, &gcv);
			draw_button(xi->display, &dialog->container, dialog->cancel_button);
			break;
		default:
			draw_field(xi->display, &dialog->container, dialog->user_field, nameref, dialog->redraw);
			draw_field(xi->display, &dialog->container, dialog->password_field, pwdref, dialog->redraw);
			draw_button(xi->display, &dialog->container, dialog->login_button);
			draw_button(xi->display, &dialog->container, dialog->clear_button);
			draw_button(xi->display, &dialog->container, dialog->cancel_button);
			break;
	}

#ifdef RENDER_FLAG
	LOG_RENDER("tlock: ended draw dialog");
#endif

}

void tlock_show_dialog(struct aXInfo* xi, dialog_t* dialog)
{
	if (!xi)
	{
		return;
	}
	if (!dialog)
	{
		return;
	}
	if (dialog->visible)
	{
		return;
	}

	XMapWindow(xi->display, dialog->container.win);
	XRaiseWindow(xi->display, dialog->container.win);

	dialog->visible = 1;
}

void tlock_hide_dialog(struct aXInfo* xi, dialog_t* dialog)
{
	if (!xi)
	{
		return;
	}
	if (!dialog)
	{
		return;
	}

	if (!dialog->visible)
	{
		return;
	}
	XUnmapWindow(xi->display, dialog->container.win);

	dialog->visible = 0;
}

void tlock_free_dialog(struct aXInfo* xi, dialog_t* dialog)
{
	if (!xi)
	{
		return;
	}
	if (!dialog)
	{
		return;
	}

	XFreeGC(xi->display, dialog->container.gc);
	XDestroyWindow(xi->display, dialog->container.win);

	free(dialog);

}

