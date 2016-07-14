/*
 ============================================================================
 Name        : tlock_bg_none.c
 Author      : akaan
 Version     :
 Copyright   : Cornell University - cornell.edu
 Published   : GNU Public License v2 (GPLv2)
 Description : tlock - bg - none : transparent lock, Ansi-style
 Created     : Jan 26, 2014
 ============================================================================
 */

#include "tlock.h"
#include <stdlib.h>

static Window* window = NULL;

static int tlock_bg_none_init(const char* args, struct aXInfo* xinfo) {

	XSetWindowAttributes xswa;
	long xsmask = 0;

	if (!xinfo)
		return 0;

	window = (Window*) calloc(xinfo->nr_screens, sizeof(Window));

	xswa.override_redirect = True;
	xsmask |= CWOverrideRedirect;
	{
		int scr;
		for (scr = 0; scr < xinfo->nr_screens; scr++) {
			window[scr] = XCreateWindow(xinfo->display, xinfo->root[scr], 0, 0,
					1, 1, 0, /* borderwidth */
					CopyFromParent, /* depth */
					InputOnly, /* class */
					CopyFromParent, /* visual */
					xsmask, &xswa);

			if (window[scr])
				xinfo->window[scr] = window[scr];
		}
	}

	return 1;
}

static int tlock_bg_none_deinit(struct aXInfo* xinfo) {
	if (!xinfo || !window)
		return 0;
	{
		int scr;
		for (scr = 0; scr < xinfo->nr_screens; scr++) {
			XDestroyWindow(xinfo->display, window[scr]);
		}
	}
	return 1;
}

struct aBackground tlock_bg_none = { "none", tlock_bg_none_init,
		tlock_bg_none_deinit };


