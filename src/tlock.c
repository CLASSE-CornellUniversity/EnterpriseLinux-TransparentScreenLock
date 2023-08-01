/*
 ============================================================================
 Name        : tlock.c
 Author      : Andre "Bananaman" Kaan
 Version     :
 Copyright   : Cornell University - cornell.edu
 Published   : GNU Public License v2 (GPLv2)
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

#include "tlock.h"
#include "tlock_frame.h"
#include "tlock_password_dialog.h"

#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xos.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <poll.h>

#define CLEAN_FIELD_ENTRY(s) for (i = 0; s >= 0 && s < 2 && i < STRING_LIMIT; i++) \
								{ focus[s][i] = 0; nameref[i] = 0; pwdref[i] = 0; shpwdref[i] = 0; rlen = 0;  } \
								flag_redraw(dialog);
#define BUTTON_PRESSED(s) is_area_pressed( dialog, s, ev.xbutton.x, ev.xbutton.y) == True


int tabpos = -1; //-1 if invalid/unknown, 0 if user field, 1 if password field, 2 if login button, 3 if clear button, 4 if cancel button

/** --------------------------------------------------------------------------------
 * tlock : authentication implementations
 * -------------------------------------------------------------------------------- */

extern struct aAuth tlock_auth_none;
extern struct aAuth tlock_auth_pam;
extern struct aAuth tlock_auth_xspam;
static struct aAuth* tlock_authmodules[] =
	{ &tlock_auth_none, &tlock_auth_xspam, &tlock_auth_pam, NULL };

/**
 * --------------------------------------------------------------------------------
 * tlock : background implementations
 * -------------------------------------------------------------------------------- */
extern struct aBackground tlock_bg_none;
static struct aBackground* tlock_backgrounds[] =
	{ &tlock_bg_none, NULL };

/** --------------------------------------------------------------------------------
 * tlock : cursors implementations
 * -------------------------------------------------------------------------------- */
extern struct aCursor tlock_cursor_none;

static struct aCursor* tlock_cursors[] =
	{ &tlock_cursor_none, NULL };
/* ---------------------------------------------------------------- */

static const char* tlock_color_swatch[] =
	{ "gray", "blue", "white" };
/* ---------------------------------------------------------------- */
static struct timeval tlock_start_time;

/* */
static void initStartTime() {
	X_GETTIMEOFDAY(&tlock_start_time);
}

int string_width(XFontStruct *font, char *s) {
	return XTextWidth(font, s, strlen(s));
}

/* */
static long elapsedTime() {

	static struct timeval end;
	static struct timeval elapsed;
	long milliseconds;

	X_GETTIMEOFDAY(&end);

	if (tlock_start_time.tv_usec > end.tv_usec) {
		end.tv_usec += 1000000;
		end.tv_sec--;
	}

	elapsed.tv_sec = end.tv_sec - tlock_start_time.tv_sec;
	elapsed.tv_usec = end.tv_usec - tlock_start_time.tv_usec;

	milliseconds = (elapsed.tv_sec * 1000) + (elapsed.tv_usec / 1000);

	return milliseconds;
}

/* */
static void displayUsage() {
	printf("%s", "tlock [-pre] [-flash] [-hv] [-bg type:options] [-cursor type:options] "
			"[-auth type:options]\n");
}

/* */
static void initXInfo(struct aXInfo* xi) {

	Display* dpy = XOpenDisplay(NULL);
	if (!dpy) {
		perror("tlock: error, can't open connection to X");
		exit(EXIT_FAILURE);
	}

	{
		xi->display = dpy;
		xi->pid_atom = XInternAtom(dpy, "_TLOCK_PID", False);
		xi->nr_screens = ScreenCount(dpy);
		xi->window = (Window*) calloc((size_t) xi->nr_screens, sizeof(Window));
		xi->root = (Window*) calloc((size_t) xi->nr_screens, sizeof(Window));
		xi->colormap = (Colormap*) calloc((size_t) xi->nr_screens, sizeof(Colormap));
		xi->cursor = (Cursor*) calloc((size_t) xi->nr_screens, sizeof(Cursor));
		xi->width_of_root = (int*) calloc(xi->nr_screens, sizeof(int));
		xi->height_of_root = (int*) calloc(xi->nr_screens, sizeof(int));
		//xi->handler = &handlers[0];
	}
	{
		XWindowAttributes xgwa;
		int scr;
		for (scr = 0; scr < xi->nr_screens; scr++) {
			xi->window[scr] = None;
			xi->root[scr] = RootWindow(dpy, scr);
			xi->colormap[scr] = DefaultColormap(dpy, scr);

			XGetWindowAttributes(dpy, xi->root[scr], &xgwa);
			xi->width_of_root[scr] = xgwa.width;
			xi->height_of_root[scr] = xgwa.height;
		}
	}LOG("initialized xinfo.");
}

/* */
enum {
	INITIAL = 0, TYPING, WRONG
};

/* */
static void visualFeedback(struct aXInfo* xi,
      struct aDialog* dialog,
      struct aFrame* frame,
      int mode,
      int toggle,
      char* nameref,
      char* pwdref) {

	static int old_mode = INITIAL;
	int redraw = 0;
	int x = toggle % 2;

	if (old_mode != mode) {
		redraw = 1;
	}
	old_mode = mode;

	switch (mode) {
		case INITIAL:
			tlock_draw_frame(xi, frame, tlock_color_swatch[x]);
			flag_redraw(dialog);
			tlock_hide_dialog(xi, dialog);
			break;
		case TYPING:
			tlock_draw_frame(xi, frame, "green");
			tlock_draw_dialog(xi, dialog, nameref, pwdref);
			DEBUG_EVENT_LOOP("redraw");
			break;
		case WRONG:
			if (redraw) {
				tlock_draw_frame(xi, frame, "red");
				tlock_hide_dialog(xi, dialog);
				tlock_draw_dialog(xi, dialog, nameref, pwdref);
			}
			tlock_draw_dialog(xi, dialog, nameref, pwdref);
			break;
		default:
			LOG("default mode");
			tlock_draw_dialog(xi, dialog, nameref, pwdref);
			break;
	};
}

/* */
static int challenge_response_feedback(struct aOpts* opts,
      struct aXInfo* xi,
      struct aFrame* frame,
      const char* username,
      const char* passwd) {

	// if the user is part of the privileged
	// group then exit otherwise
	// continue
	int ret = opts->auth->auth(strdup(username), strdup(passwd), opts->gids);

	syslog(LOG_NOTICE, "tlock: authentication : returned %d\n", ret);
	DEBUG_EVENT_LOOP("authentication");
	if (ret == 1) {
		LOG("entering Free Frame");
		tlock_free_frame(xi, frame);
		LOG("exiting Free Frame");
		return 1;
	} else {
		fprintf( stderr, "authentication: u=%s, p=(%s), exit=%d\n", strdup(username), "null",  ret);
		LOG("authentication failed.");
		tlock_free_frame(xi, frame);
		// create a new frame since different display
		// has been initiated after the login
		frame = tlock_create_frame(xi, 0, 0, xi->width_of_root[0], xi->height_of_root[0], 10);

#ifdef ATTEMPT_LIMIT
		if (--attempt < 1) {
			syslog(LOG_ALERT, "Ended tlock with incorrect match!");
			closelog();
			exit(0);
		}
#endif
	}
	return 0;
}


/* */
static int eventLoop(struct aOpts* opts, struct aXInfo* xi) {
	Display* dpy = xi->display;
	KeySym ks;
	XEvent ev;
	char cbuf[30];
	unsigned int clen, rlen = 0;
	long current_time = 0;
	long last_key_time = 0;
	const long penalty = 1000;
	long timeout = 0;
	int toggle = 0;
	int mode = INITIAL;
	char nameref[STRING_LIMIT];
	char pwdref[STRING_LIMIT];
	char shpwdref[STRING_LIMIT];
	int i = 0;
	int active_field = 0;
	tabpos = 0;	//default tab position to user field
	int shift = 0;

	/*char * const argv[] =
		{ "/usr/bin/gnome-screensaver-command", "-l", NULL };
	*/
	char * const focus[] =
		{ nameref, pwdref, NULL };

	for (i = 0; i < STRING_LIMIT; i++) {
		nameref[i] = 0;
		pwdref[i] = 0;
		shpwdref[i] = 0;
	}

	LOG("create visual response frame");
	struct aFrame* frame = tlock_create_frame(xi, 0, 0, xi->width_of_root[0], xi->height_of_root[0], 10);
	LOG("enter dialog creation");
	
	int wor = xi->width_of_root[0];
	int hor = xi->height_of_root[0];
	int dwidth = 275;
	int dheight = 130;
	// Adjust placement for multiple screens (will detect 4K screens as multiple screens)
	int x = wor > 3000 ? 4 : 2;
	int y = hor > 2000 ? 4 : 2;
	struct aDialog* dialog = tlock_create_dialog(xi, (wor-dwidth)/x, (hor-dheight)/y, dwidth, dheight, 10);

	LOG("completed dialog creation");LOG("entering event loop");

	for (;;) {
		current_time = elapsedTime();

		// check for any event occuring
		if (XCheckWindowEvent(xi->display, xi->window[0],
		KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask, &ev) == True) {
			DEBUG_EVENT_LOOP_BLANK;
			//PRINT(fprintf(stderr, "event.type %d \n", ev.xany.type));
			switch (ev.xany.type) {
				case KeyPress: { 
					last_key_time = current_time;
					if (last_key_time < timeout) {
						XBell(dpy, 0);
						break;
					}
					// swallow up first keypress to indicate "enter mode" to open dialog window
					if (mode == INITIAL) {
						mode = TYPING;
						break;
					}
					mode = TYPING;
					clen = XLookupString(&ev.xkey, cbuf, 15, &ks, 0);
					_PRINTF_( "key=%c \n", cbuf[0]);
					switch (ks) {
						case XK_Shift_L:
							shift = 1;
							break;
						case XK_Shift_R:
							shift = 1;
							break;
						case XK_Tab:
						//user pressed the TAB key
							if (shift == 1) { //want to go backwards
								int temp = (tabpos - 1) % 5;
								tabpos = (temp < 0) ? temp + 5 : temp;
							} else {
								tabpos = (tabpos + 1) % 5;
							}
							if (tabpos < 2) {
							//enter text into fields
								active_field = tabpos; //active field is the one where tab position is
								rlen = strlen(focus[active_field]);
								flag_redraw(dialog);
								_PRINTF_( "TAB_: active_field = %d, %d = %s\n", active_field, rlen, focus[active_field] );
								visualFeedback(xi, dialog, frame, mode, toggle, nameref, shpwdref);
							}
							shift = 0;
							break;
						case XK_Escape:
						case XK_Clear:
							rlen = 0;
							CLEAN_FIELD_ENTRY(active_field);
							flag_redraw(dialog);
							visualFeedback(xi, dialog, frame, mode, toggle, nameref, shpwdref);
							break;
						case XK_Delete:
						case XK_BackSpace:
							if (tabpos < 2) {
								if (rlen > 0) {
									focus[active_field][rlen] = 0;
									if (active_field == 1 ) {
										shpwdref[rlen] = 0;
									}
									rlen--;
									focus[active_field][rlen] = 0;
									if (active_field == 1 ) {
										shpwdref[rlen] = 0;
									}
								}
								flag_redraw(dialog);
								visualFeedback(xi, dialog, frame, mode, toggle, nameref, shpwdref);
								_PRINTF_("BACK: active_field = %d, %d = %s\n", active_field, rlen, focus[active_field] );
							}
							shift = 0;
							break;
						case XK_Linefeed:
						case XK_Return:
							if (tabpos == 3) {//clear button was
								CLEAN_FIELD_ENTRY(0);
								CLEAN_FIELD_ENTRY(1);
								active_field = 0;
								tabpos = 0;
								// reset mode for loop event
								mode = TYPING;
								visualFeedback(xi, dialog, frame, mode, toggle, nameref, shpwdref);
								break;
							} else if (tabpos == 4) {//cancel button was active
								CLEAN_FIELD_ENTRY(0);
								CLEAN_FIELD_ENTRY(1);
								active_field = 0;
								tabpos = 0;
								// reset mode for loop event
								mode = INITIAL;
								visualFeedback(xi, dialog, frame, mode, toggle, nameref, shpwdref);
								break;
							} else { //pressing RETURN on all other tab positions equivalent to login attempt
							
								if (rlen == 0) {
								// if nothing was entered and enter/return was
								// pressed, clear buffer and exit out of routine.
									CLEAN_FIELD_ENTRY(active_field);
									break;
								}
	
								if (rlen < sizeof(focus[active_field])) {
									focus[active_field][rlen] = 0;
									if (active_field == 1) {
										shpwdref[rlen] = 0;
									}
								}
								// some auth() methods have their own penalty system
								// so we draw a 'yellow' frame to show 'checking' state.
								DEBUG_EVENT_LOOP_BLANK;
	
								tlock_draw_frame(xi, frame, "yellow");
								DEBUG_EVENT_LOOP_BLANK;
	
								XSync(dpy, True);
								DEBUG_EVENT_LOOP_BLANK;
	
								// copy buffer in focussed value array
								if (challenge_response_feedback(opts, xi, frame, nameref, pwdref)) {
									return 1;	//login successful
								} else {
									flag_redraw(dialog);
								}
								//
								CLEAN_FIELD_ENTRY(active_field) ;
								active_field = 0;
								tabpos = 0;
								//XBell(dpy, 0);
	
								DEBUG_EVENT_LOOP_BLANK;
								mode = WRONG;
								timeout = elapsedTime() + penalty;
							}
							shift = 0;
							break;
						default: //typing a character (or any other key)
							if (tabpos < 2) {
								if (clen != 1)
									break;
								_PRINTF_("%d, len=%l, f=%l\n", active_field, rlen, sizeof(nameref));
								if (rlen < (sizeof(nameref) - 1)) {
									focus[active_field][rlen] = cbuf[0];
									if (active_field == 1) {
										shpwdref[rlen]='*';
									}
									_PRINTF_("%d,  focus=%s\n", active_field, focus[active_field]);
									rlen++;
								}
							}
							shift = 0;
							break;
					} //end keypress switch statement
				} //case KeyPress end
					break;
				case Expose: {
					XExposeEvent* eev = (XExposeEvent*) &ev;
					_PRINTF_( "expose event %d\n", eev->type);
					XClearWindow(xi->display, eev->window);
					exit(400);
				}//case Expose end
					break;
				case ButtonPress: {
				} //case Buttonpress end
					break;
				case ButtonRelease: {
					shift = 0;
					if (BUTTON_PRESSED("user_field")) { //clicking into the username field
						active_field = 0;
						tabpos = 0;
						rlen = strlen(focus[active_field]);
						flag_redraw(dialog);
						_PRINTF_( "FIELD_: active_field = %d, %d = %s\n", active_field, rlen, focus[active_field] );
						visualFeedback(xi, dialog, frame, mode, toggle, nameref, shpwdref);
						break;

					} else if (BUTTON_PRESSED("password_field")) {
						active_field = 1;
						tabpos = 1;
						rlen = strlen(focus[active_field]);
						flag_redraw(dialog);
						_PRINTF_( "FIELD_: active_field = %d, %d = %s\n", active_field, rlen, focus[active_field] );
						visualFeedback(xi, dialog, frame, mode, toggle, nameref, shpwdref);
						break;

					} else if (BUTTON_PRESSED("login_button")) {

						if (rlen == 0) {
						// if nothing was entered and login button was
						// pressed, clear buffer and exit out of routine.
							CLEAN_FIELD_ENTRY(active_field);
							break;
						}

						if (active_field < 0) {
							CLEAN_FIELD_ENTRY(0);
							CLEAN_FIELD_ENTRY(1);
							break;
						}
						if (rlen < sizeof(focus[active_field])) {
							focus[active_field][rlen] = 0;
							if (active_field == 1) {
								shpwdref[rlen] = 0;
							}
						}
						// some auth() methods have their own penalty system
						// so we draw a 'yellow' frame to show 'checking' state.
						DEBUG_EVENT_LOOP_BLANK;

						tlock_draw_frame(xi, frame, "yellow");
						DEBUG_EVENT_LOOP_BLANK;

						XSync(dpy, True);
						DEBUG_EVENT_LOOP_BLANK;

						// copy buffer in focussed value array
						if (challenge_response_feedback(opts, xi, frame, nameref, pwdref)) {
							return 1;
						} else {
							flag_redraw(dialog);
						}
						//
						CLEAN_FIELD_ENTRY(active_field) ;
						active_field = 0;
						tabpos = 0;
						//XBell(dpy, 0);

						DEBUG_EVENT_LOOP_BLANK;
						mode = WRONG;
						timeout = elapsedTime() + penalty;
						break;
					} else if (BUTTON_PRESSED("cancel_button")) { //clears field entries and quits dialog window
						CLEAN_FIELD_ENTRY(0);
						CLEAN_FIELD_ENTRY(1);
						active_field = 0;
						tabpos = 0;
						// reset mode for loop event
						mode = INITIAL;
						visualFeedback(xi, dialog, frame, mode, toggle, nameref, shpwdref);
					} else if (BUTTON_PRESSED("clear_button")) { //clears field entries, does not reset dialog window
						CLEAN_FIELD_ENTRY(0);
						CLEAN_FIELD_ENTRY(1);
						active_field = 0;
						tabpos = 0;
						// reset mode for loop event
						mode = TYPING;
						visualFeedback(xi, dialog, frame, mode, toggle, nameref, shpwdref);
					} 
				} // case BUTTON RELEASE end
					break;
				default:
					break;
			}DEBUG_EVENT_LOOP_BLANK;

		} else { // wait a bit
			DEBUG_EVENT_LOOP_BLANK;
			long delta = current_time - last_key_time;

			if (mode == TYPING && (delta > 10000)) { // user fell asleep while typing .)
				mode = INITIAL;
			} else if (mode == WRONG && (current_time > timeout)) { // end of timeout for wrong password
				mode = TYPING;
				last_key_time = timeout; // start 'idle' timer correctly by a fake keypress
			}

			if (delta > 1000) {
				toggle = 2;
				if (opts->flash) {
					toggle = (delta / 1000) % 2;
				}
			}DEBUG_EVENT_LOOP_BLANK;

			visualFeedback(xi, dialog, frame, mode, toggle, nameref, shpwdref);
			DEBUG_EVENT_LOOP_BLANK;

			poll(NULL, 0, 25);
		}
	}

// normally, we shouldn't arrive here at all
	tlock_free_frame(xi, frame);
#ifdef HAVE_DIALOG
	tlock_free_dialog(xi, dialog);
#endif
	return 0;
}

/* */
static pid_t getPidAtom(struct aXInfo* xinfo) {
	Atom ret_type;
	int ret_fmt;
	unsigned long nr_read;
	unsigned long nr_bytes_left;
	pid_t* ret_data;

	if (XGetWindowProperty(xinfo->display, xinfo->root[0], xinfo->pid_atom, 0L, 1L, False,
	    XA_CARDINAL, &ret_type, &ret_fmt, &nr_read, &nr_bytes_left, 
	    (unsigned char**) &ret_data) == Success && ret_type != None && ret_data) {
		pid_t pid = *ret_data;
		XFree(ret_data);
		return pid;
	}
	return 0;
}

/* */
static int detectOtherInstance(struct aXInfo* xinfo) {
	pid_t pid = getPidAtom(xinfo);
	int process_alive = kill(pid, 0);
	(void) printf(" -: message alive=(%d) pid=[%d]\n", process_alive, pid);

	if (pid > 0 && process_alive == 0) {
		return 1;
	}

	if (process_alive) {
		perror("tlock: info, found _TLOCK_PID");
	}
	return 0;
}

/* */
static int registerInstance(struct aXInfo* xinfo) {
	pid_t pid = getpid();
	XChangeProperty(xinfo->display, xinfo->root[0], xinfo->pid_atom,
	XA_CARDINAL, sizeof(pid_t) * 8, PropModeReplace, (unsigned char*) &pid, 1);
	LOG("registered xinfo instance");
	return 1;
}

/* */
static int unregisterInstance(struct aXInfo* xinfo) {
	XDeleteProperty(xinfo->display, xinfo->root[0], xinfo->pid_atom);
	return 1;
}

/*------------------------------------------------------------------*\
 * Getting the colors you want from a shared colormap is really pretty simple.
 * The easiest way is to consult the rgb.txt file (usually found as
 * /usr/X11/lib/X11/rgb.txt). It list all the string names associated with
 * all the colors of the rainbow! Here is how you use them:
 \*------------------------------------------------------------------*/

int main(int argc, char **argv) {

	struct aXInfo xinfo;
	struct aOpts opts;

	int arg = 0;
	const char* cursor_args = NULL;
	const char* background_args = NULL;
	int precheck = 0;

	opts.auth = NULL;
	opts.cursor = tlock_cursors[0];
	opts.background = tlock_backgrounds[0];
	opts.flash = 0;
	opts.gids = 0;

	/*  parse options */
	if (argc != 1) {
		for (arg = 1; arg <= argc; arg++) {
			/* Background options. */
			if (!strcmp(argv[arg - 1], "-bg")) {
				if (arg < argc) {
					char* char_tmp;
					struct aBackground* bg_tmp = NULL;
					struct aBackground** i;
					/* List all the available backgrounds. */
					if (strcmp(argv[arg], "list") == 0) {
						for (i = tlock_backgrounds; *i; ++i) {
							printf("%s\n", (*i)->name);
						}
						exit(EXIT_SUCCESS);
					}

					/* Assigning the backgrounds to the option array. */
					for (i = tlock_backgrounds; *i; ++i) {
						char_tmp = strstr(argv[arg], (*i)->name);
						if (char_tmp && char_tmp == argv[arg]) {
							background_args = char_tmp;
							bg_tmp = *i;
							opts.background = bg_tmp;
							++arg;
							break;
						}
					}
					/* Abort when background is not found. */
					if (bg_tmp == NULL) {
						printf("%s", "tlock: error, couldn't find the bg-module you specified.\n");
						exit(EXIT_FAILURE);
					}
				} else {
					printf("%s", "tlock, error, missing argument\n");
					displayUsage();
					exit(EXIT_FAILURE);
				}
			} else if (!strcmp(argv[arg - 1], "-auth")) {
				if (arg < argc) {
					char* char_tmp;
					struct aAuth* auth_tmp = NULL;
					struct aAuth** i;
					if (!strcmp(argv[arg], "list")) {
						for (i = tlock_authmodules; *i; ++i) {
							printf("%s\n", (*i)->name);
						}
						exit(EXIT_SUCCESS);
					}
					for (i = tlock_authmodules; *i; ++i) {
						char_tmp = strstr(argv[arg], (*i)->name);
						if (char_tmp && char_tmp == argv[arg]) {
							auth_tmp = (*i);
							if (auth_tmp->init(argv[arg]) == 0) {
								printf("%s.%s: error, failed init of [%s] with arguments [%s].\n",
								__FILE__, __FUNCTION__, auth_tmp->name, argv[arg]);
								exit(EXIT_FAILURE);
							} else {
								printf("%s.%s: initialized [%s] with arguments [%s].\n",
								__FILE__, __FUNCTION__, auth_tmp->name, argv[arg]);
							}
							opts.auth = auth_tmp;
							++arg;
							break;
						}
					}

					if (auth_tmp == NULL) {
						printf("%s", "tlock: error, couldnt find the auth-module you specified.\n");
						exit(EXIT_FAILURE);
					}

				} else {
					printf("%s", "tlock, error, missing argument\n");
					displayUsage();
					exit(EXIT_FAILURE);
				}
			} else if (strcmp(argv[arg - 1], "-cursor") == 0) {
				if (arg < argc) {
					char* char_tmp;
					struct aCursor* cursor_tmp = NULL;
					struct aCursor** i;
					if (strcmp(argv[arg], "list") == 0) {
						for (i = tlock_cursors; *i; ++i) {
							printf("%s\n", (*i)->name);
						}
						exit(EXIT_SUCCESS);
					}
					for (i = tlock_cursors; *i; ++i) {
						char_tmp = strstr(argv[arg], (*i)->name);
						if (char_tmp && char_tmp == argv[arg]) {
							cursor_args = char_tmp;
							cursor_tmp = *i;
							opts.cursor = cursor_tmp;
							++arg;
							break;
						}
					}
					if (!cursor_tmp) {
						printf("%s", "tlock: error, couldn't find the cursor-module you specified.\n");
						exit(EXIT_FAILURE);
					}
				} else {
					printf("%s", "tlock, error, missing argument\n");
					displayUsage();
					exit(EXIT_FAILURE);
				}
			} else {
				if (strcmp(argv[arg - 1], "-h") == 0) {
					displayUsage();
					exit(EXIT_SUCCESS);
				} else if (strcmp(argv[arg - 1], "-v") == 0) {
					printf("tlock-%s by André 'Bananaman' Kaan\n", VERSION);
					exit(EXIT_SUCCESS);
				} else if (strcmp(argv[arg - 1], "-pre") == 0) {
					precheck = 1;
				} else if (strcmp(argv[arg - 1], "-flash") == 0) {
					opts.flash = 1;
				} else if (strcmp(argv[arg - 1], "-gids") == 0) {
					opts.gids = 1;
				}
			}
		}
	}

	/* */
	initStartTime();
	/* */
	initXInfo(&xinfo);

	if (detectOtherInstance(&xinfo)) {
		printf("%s", "tlock: error, another instance seems to be running\n");
		exit(EXIT_FAILURE);
	}

	/* List the usage of the authentication. */
	if (!opts.auth) {
		printf("%s", "tlock: error, no auth-method specified.\n");
		displayUsage();
		exit(EXIT_FAILURE);
	}

	/* */
	if (opts.background->init(background_args, &xinfo) == 0) {
		printf("tlock: error, couldn't init [%s] with [%s].\n", opts.background->name, background_args);
		exit(EXIT_FAILURE);
	}

	if (opts.cursor->init(cursor_args, &xinfo) == 0) {
		printf("tlock: error, couldn't init [%s] with [%s].\n", opts.cursor->name, cursor_args);
		exit(EXIT_FAILURE);
	}

	LOG("initializing screens");
	{
		int scr;
		for (scr = 0; scr < xinfo.nr_screens; scr++) {
			XSelectInput(xinfo.display, xinfo.window[scr],
			ButtonReleaseMask | ButtonPressMask);
			XMapWindow(xinfo.display, xinfo.window[scr]);
			XRaiseWindow(xinfo.display, xinfo.window[scr]);
		}
	}

	LOG("initializing keyboard control");
	/* try to grab 2 times, another process (windowmanager) may have grabbed
	 * the keyboard already */
	if ((XGrabKeyboard(xinfo.display, xinfo.window[0], True, GrabModeAsync,GrabModeAsync, CurrentTime))
	!= GrabSuccess) {
		sleep(1);
		if ((XGrabKeyboard(xinfo.display, xinfo.window[0], True, GrabModeAsync, GrabModeAsync, CurrentTime)) != GrabSuccess) {
			printf("%s", "tlock: couldn't grab the keyboard.\n");
			exit(EXIT_FAILURE);
		}
	}

	/* TODO: think about it: do we really need NR_SCREEN cursors ? we grab the
	 * pointer on :*.0 anyway ... */
	LOG("initializing mouse control");
	if (XGrabPointer(xinfo.display, xinfo.window[0], False, ButtonPressMask | ButtonReleaseMask,
								// needed for the propagation of the pointer events on window[0]
	GrabModeAsync, GrabModeAsync, None, xinfo.cursor[0], CurrentTime) != GrabSuccess) {
		XUngrabKeyboard(xinfo.display, CurrentTime);
		printf("%s", "tlock: couldn't grab the pointer.\n");
		exit(EXIT_FAILURE);
	}

	openlog("tlock", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
	syslog(LOG_NOTICE, "program started by user %d(%s), accepting groups as '%s' only.", getuid(), getenv("USER"),
		opts.gids ? "id" : "names");
	// pre authorization check
	if (precheck == 1) {
		if (opts.auth != NULL) {
			int ret = opts.auth->auth(NULL, NULL, 0);
			syslog(
			LOG_NOTICE, "%s: %s[exit=%d]\n",
			__FILE__, __FUNCTION__, ret);

			if (ret != 1) {
				registerInstance(&xinfo);
				eventLoop(&opts, &xinfo);
				LOG("exiting eventloop, and unregister");
				unregisterInstance(&xinfo);
			}
		}
	} else {
		registerInstance(&xinfo);
		eventLoop(&opts, &xinfo);
		LOG("exiting eventloop, and unregister");
		unregisterInstance(&xinfo);
	}

	closelog();

	opts.auth->deinit();
	opts.cursor->deinit(&xinfo);
	opts.background->deinit(&xinfo);

	XCloseDisplay(xinfo.display);

	return EXIT_SUCCESS;
}

