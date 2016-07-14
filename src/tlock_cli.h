/*
 ============================================================================
 Name        : tlock_cli.h
 Author      : akaan
 Version     : 
 Copyright   : Cornell University - cornell.edu
 Published   : GNU Public License v2 (GPLv2)
 Description : tlock - transparent lock, Ansi-style
 Created     : Jan 25, 2014
 ============================================================================
 */

#ifndef _TLOCK_CLI_H_
#define _TLOCK_CLI_H_

#include <stdio.h>
#include <X11/Xlib.h>

/* --------
 * -------------------------------------------------------- *\
\* ---------------------------------------------------------------- */
#ifdef DEBUG
#    define DBGMSG fprintf(stderr, "%s : %d\n", __FUNCTION__, __LINE__); fflush(stderr)
#else
#    define DBGMSG
#endif /* DEBUG */
/* ----------------------------------------------------------------  *
 * PAM AUTHENTICATION DEFINITIONS
/* ----------------------------------------------------------------  */
#define PAM_SERVICE_NAME "unix"
#define VERSION "1.0"

struct aAuth {
	const char* name;
	int (*init)(const char* args);
	int (*auth)(const char* pass);
	int (*deinit)();
};

struct aOpts {
	struct aAuth* auth;
};


#endif /* _TLOCK_CLI_H_ */

