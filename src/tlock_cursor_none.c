/*
 ============================================================================
 Name        : tlock_cursor_none.c
 Author      : akaan
 Version     :
 Copyright   : Cornell University - cornell.edu
 Published   : GNU Public License v2 (GPLv2)
 Description : tlock - cursor - none : transparent lock, Ansi-style
 Created     : Jan 26, 2014
 ============================================================================
 */

#include "tlock.h"

static int tlock_cursor_none_init(const char* args, struct aXInfo* xinfo) {
	int scr;
	for (scr = 0; scr < xinfo->nr_screens; scr++) {
		xinfo->cursor[scr] = None;
	}
	return 1;
}

static int tlock_cursor_none_deinit(struct aXInfo* xinfo) {
	return 1;
}

struct aCursor tlock_cursor_none = { "none", tlock_cursor_none_init,
		tlock_cursor_none_deinit };

