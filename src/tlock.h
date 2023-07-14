/*
 ============================================================================
 Name        : tlock.h
 Author      : akaan
 Version     : 
 Copyright   : Cornell University - cornell.edu
 Published   : GNU Public License v2 (GPLv2)
 Description : tlock - transparent lock, Ansi-style
 Created     : Jan 25, 2014
 ============================================================================
 */

#ifndef _TLOCK_H_
#define _TLOCK_H_

#include <X11/Xlib.h>
#include <stdio.h>

#ifndef VERSION
#define VERSION "1.0"
#endif

#ifdef VERBOSE_FLAG
#define LOG_VERBOSE printf( "%-20s:%-25s:%03d\n", __FILE__, __FUNCTION__,__LINE__ ); 
#define LOG(s)      printf( "%-20s:%-25s:%03d - %s\n",__FILE__, __FUNCTION__, __LINE__, s)
#else
#define LOG_VERBOSE
#define LOG(s)
#endif

#ifdef TRACE_FLAG
#define _PRINT_	fprintf(stderr, "%-20s:%-25s: %d\n",__FILE__,__FUNCTION__,  __LINE__)
#define _PRINTF_(...)	fprintf(stderr, "%-20s:%-25s: %d - ",__FILE__,__FUNCTION__,  __LINE__, __VA_ARGS__)
#else
#define _PRINT_ /* */
#define _PRINTF_(...) /* */
#endif

#ifdef DEBUG_FLAG
#define DEBUG_EVENT_LOOP_BLANK 	fprintf(stderr, "%-20s:%-25s:%03d - \n",__FILE__,__FUNCTION__,__LINE__)
#define DEBUG_EVENT_LOOP(s) 	fprintf(stderr, "%-20s:%-25s:%03d - %s \n",__FILE__,__FUNCTION__,__LINE__,s )
#define DEBUG_FRAME(s) 	fprintf(stderr, "%-20s:%-25s:%03d - \n",__FILE__,__FUNCTION__,__LINE__ )
#define DEBUG_AUTH(...) 	fprintf(stderr, __VA_ARGS__ )
#else
#define DEBUG_EVENT_LOOP(s)
#define DEBUG_EVENT_LOOP_BLANK
#define DEBUG_FRAME(s) do {} while(0)
#define DEBUG_AUTH(...)
#endif

#ifdef RENDER_FLAG
#define LOG_RENDER(...) fprintf(stderr, __VA_ARGS__ )
#else
#define LOG_RENDER(...)
#endif

#ifndef SYSLOG_ENABLED
#define _SYSLOG_(...) printf ( __VA_ARGS__)
#else
#define _SYSLOG_(...)  syslog(LOG_NOTICE, __VA_ARGS__)
#endif


/* ----------------------------------------------------------------  *
 * PAM AUTHENTICATION DEFINITIONS
 * ----------------------------------------------------------------  */
#define PAM_SERVICE_NAME "system-auth"
#define STRING_LIMIT 64

extern int tabpos;

struct aXInfo {
	Display* display;

	Atom pid_atom;

	int nr_screens;

	int* width_of_root;
	int* height_of_root;

	Window* root;
	Colormap* colormap;

	Window* window;
	Cursor* cursor;

	Window dialog_window;
};


struct aAuth {
	const char* name;
	int (*init)(const char* args);
	int (*auth)(const char* user, const char* pass, int as_gid);
	int (*deinit)();
};

struct aCursor {
	const char* name;
	int (*init)(const char* args, struct aXInfo* xinfo);
	int (*deinit)(struct aXInfo* xinfo);
};

struct aBackground {
	const char* name;
	int (*init)(const char* args, struct aXInfo* xinfo);
	int (*deinit)(struct aXInfo* xinfo);
};


struct aOpts {
	struct aAuth* auth;
	struct aCursor* cursor;
	struct aBackground* background;
	int flash;
	int gids;
};

#endif /* _TLOCK_H_ */

