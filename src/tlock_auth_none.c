/*
 ============================================================================
 Name        : tlock_auth_none.c
 Author      : akaan
 Version     :
 Copyright   : Cornell University - cornell.edu
 Published   : GNU Public License v2 (GPLv2)
 Description : tlock - auth - none - transparent lock, Ansi-style
 Created     : Jan 26, 2014
 ============================================================================
 */

#include "tlock.h"

static int tlock_auth_none_init(const char* args) {
	return 1;
}

static int tlock_auth_none_deinit() {
	return 1;
}

static int tlock_auth_none_auth(const char* username, const char* passwd, int as_gid) {
	return 1;
}

struct aAuth tlock_auth_none = { "none", tlock_auth_none_init,
		tlock_auth_none_auth, tlock_auth_none_deinit };

