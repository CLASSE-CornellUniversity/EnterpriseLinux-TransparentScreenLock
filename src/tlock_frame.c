/*
 ============================================================================
 Name        : tlock_frame.c
 Author      : akaan
 Version     :
 Copyright   : Cornell University - cornell.edu
 Published   : GNU Public License v2 (GPLv2)
 Description : tlock - transparent lock, Ansi-style
 Created     : Jan 25, 2014
 ============================================================================
 */

#include "tlock_frame.h"
#include "tlock.h"

#include <X11/Xlib.h>
#include <stdlib.h>
#include <X11/Intrinsic.h>
#include <X11/cursorfont.h>
#include <X11/Xos.h>		/* for time() */
#include <time.h>
#include <sys/time.h>

/* ---------------------------------------------------------------- *\
\* ---------------------------------------------------------------- */

struct aSide {
	Window win;
	int width;
	int height;
	GC gc;
};

struct aFrame {
	int visible;
	struct aSide top;
	struct aSide left;
	struct aSide right;
	struct aSide bottom;
	XColor color;
};

/* ---------------------------------------------------------------- *\
\* ---------------------------------------------------------------- */

struct aFrame* tlock_create_frame(struct aXInfo* xi, int x, int y, int width, int height, int line_width) {

	Display* dpy = xi->display;
	struct aFrame* frame;
	struct aSide* side;
	int i;
	XSetWindowAttributes xswa;
	xswa.override_redirect = True;
	xswa.colormap = xi->colormap[0];
	LOG("create frame on display");

	frame = (struct aFrame*) calloc(1, sizeof(struct aFrame));

	if (frame == 0)
		return 0;
	/*

	 ascii -

	 p1 ------------------------------------------------- p2
	 |                        top                          |
	 p3 --------------------------------------------------p4
	 p4 --- p5                                     p6 --- p7
	 |   l   |                                     |   r   |
	 |   e   |                                     |   i   |
	 |   f   |                                     |   g   |
	 |   t   |                                     |   h   |
	 |       |                                     |   t   |
	 p8 --- p9                                     pa --- pb
	 pc ------------------------------------------------- pd
	 |                      bottom                         |
	 pe ------------------------------------------------- pf

	 */

	frame->top.width = width;
	frame->top.height = line_width;
	frame->top.win = XCreateWindow(
	        dpy,
	        xi->root[0],
	        x,
	        y,
	        frame->top.width,
	        frame->top.height,
	        0,
	        CopyFromParent,
	        InputOutput,
	        CopyFromParent,
	        CWOverrideRedirect | CWColormap,
	        &xswa);
	frame->bottom.width = width;
	frame->bottom.height = line_width;
	frame->bottom.win = XCreateWindow(
	        dpy,
	        xi->root[0],
	        x,
	        y + height - line_width,
	        frame->bottom.width,
	        frame->bottom.height,
	        0,
	        CopyFromParent,
	        InputOutput,
	        CopyFromParent,
	        CWOverrideRedirect | CWColormap,
	        &xswa);

	frame->left.width = line_width;
	frame->left.height = height - line_width - 1;
	frame->left.win = XCreateWindow(
	        dpy,
	        xi->root[0],
	        x,
	        y + line_width,
	        frame->left.width,
	        frame->left.height,
	        0,
	        CopyFromParent,
	        InputOutput,
	        CopyFromParent,
	        CWOverrideRedirect | CWColormap,
	        &xswa);
	frame->right.width = line_width;
	frame->right.height = height - line_width - 1;
	frame->right.win = XCreateWindow(
	        dpy,
	        xi->root[0],
	        x + width - line_width,
	        y + line_width,
	        frame->right.width,
	        frame->right.height,
	        0,
	        CopyFromParent,
	        InputOutput,
	        CopyFromParent,
	        CWOverrideRedirect | CWColormap,
	        &xswa);

//    tlock_show_frame(frame);

	side = (struct aSide*) &frame->top;
	for (i = 0; i < 4; i++) {
		side[i].gc = XCreateGC(dpy, side[i].win, 0, 0);
	}

	return frame;
}

void tlock_free_frame(struct aXInfo* xi, struct aFrame* frame) {
	struct aSide* side = (struct aSide*) &frame->top;
	fprintf(stderr, "free frame display[%p]\n", xi->display);
	int i;

	for (i = 0; i < 4; i++) {
		XFreeGC(xi->display, side[i].gc);
		XDestroyWindow(xi->display, side[i].win);
	}
	free(frame);
}

void tlock_draw_frame(struct aXInfo* xi, struct aFrame* frame, const char* color_name) {

	struct aSide* side = (struct aSide*) &frame->top;
	Display* dpy = xi->display;
	XGCValues gcvals;
	XColor tmp;
	int i;

	DEBUG_FRAME("draw frame");
	if (!frame->visible) {
		tlock_show_frame(xi, frame);
	}
#ifdef DEBUG_FLAG
	fprintf(stderr, "tlock_draw_frame         :%03d - display address - %x color=%s \n", __LINE__, dpy,color_name);
#endif

	XAllocNamedColor(dpy, xi->colormap[0], color_name, &frame->color, &tmp);

	DEBUG_FRAME("assign foreground color");
	gcvals.foreground = frame->color.pixel;

	DEBUG_FRAME("draw frame: 4 sides");
	for (i = 0; i < 4; i++) {
		//LOG("gc: switch");
		XChangeGC(dpy, side[i].gc, GCForeground, &gcvals);
		XFillRectangle(
		        dpy,
		        side[i].win,
		        side[i].gc,
		        0,
		        0,
		        side[i].width,
		        side[i].height);
	}DEBUG_FRAME("tlock: ended draw frame");
}

void tlock_show_frame(struct aXInfo* xi, struct aFrame* frame) {
	int i;
	struct aSide* side = (struct aSide*) &frame->top;

	if (frame->visible)
		return;

	DEBUG_FRAME("show frame");

	for (i = 0; i < 4; i++) {
		XMapWindow(xi->display, side[i].win);
		XRaiseWindow(xi->display, side[i].win);
	}
	frame->visible = 1;
}

void tlock_hide_frame(struct aXInfo* xi, struct aFrame* frame) {
	int i;
	struct aSide* side = (struct aSide*) &frame->top;

	if (!frame->visible)
		return;

	for (i = 0; i < 4; i++) {
		XUnmapWindow(xi->display, side[i].win);
	}

	frame->visible = 0;
}

