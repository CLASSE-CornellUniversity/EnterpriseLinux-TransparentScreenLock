/*
 ============================================================================
 Name        : tlock_cli.c
 Author      : akaan
 Version     :
 Copyright   : Cornell University - cornell.edu
 Published   : GNU Public License v2 (GPLv2)
 Description : tlock - transparent lock, Ansi-style
 Created     : Mar 16, 2014
 ============================================================================
 */

#include "tlock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <poll.h>

extern struct aAuth tlock_auth_none;
extern struct aAuth tlock_auth_pam;
extern struct aAuth tlock_auth_xspam;
static struct aAuth* tlock_authmodules[] =
	{ &tlock_auth_none, &tlock_auth_pam, &tlock_auth_xspam, NULL };

int main(int argc, char **argv) {

	int arg = 0;
	const char* password = NULL;
	const char* user = NULL;
	struct aOpts opts;
	opts.auth = NULL;

	/*  parse options */
	if (argc != 1) {
		for (arg = 1; arg <= argc; arg++) {
			/* Background options. */
			if (!strcmp(argv[arg - 1], "-auth")) {
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
								printf("%s: error, failed init of [%s].\n",
								__FILE__, auth_tmp->name);
								exit(EXIT_FAILURE);
							} else {
								printf("%s: initialized [%s].\n",
								__FILE__, argv[arg]);
							}
							opts.auth = auth_tmp;
							++arg;
							break;
						}
					}

					if (auth_tmp == NULL) {
						printf( "%s: %s",
						        __FILE__,
						        "tlock: error, couldnt find the auth-module you specified.\n");
						exit(EXIT_FAILURE);
					}

				}
			} else if (strcmp(argv[arg - 1], "-v") == 0) {
					printf( "%s-%s by André 'Bananaman' Kaan\n",
					        __FILE__,
					        VERSION);
					exit(EXIT_SUCCESS);
			} else if (strcmp(argv[arg - 1], "-u") == 0) {
				if (arg < argc) {
					printf("selecting user %s\n", argv[arg]);
					user = strdup(argv[arg]);
					//++arg;
				}
			} else if (strcmp(argv[arg - 1], "-p") == 0) {
				if (arg < argc) {
					printf("selecting password %s\n", argv[arg]);
					password = strdup(argv[arg]);
					++arg;
				}
			}

		}
	}

	char n[50], m[50];
	char * const focus[] =
		{ n, m, NULL };

	if (!user || !password) {
		fprintf(stderr, "Usage: tlock_cli -auth xspam,200,0 -u user -p password\n");
		exit(0);
	}

	fprintf(stderr, "user=%s, pwd=%s\n", user, password);
	strcpy(focus[0], user);
	strcpy(focus[1], password);

	// --
	if (!opts.auth) {
		printf("%s: error, no auth-method specified.\n", __FILE__);
		exit(EXIT_FAILURE);
	}
	// --
	if (opts.auth != NULL) {
		int ret = opts.auth->auth(user, password, 1);
		fprintf(stderr, "%s: %s[exit=%d]\n", __FILE__, focus[0], ret);
		opts.auth->deinit();
		if (ret == 1) {
			fprintf(stderr, "user is authorized based on authorized group(s)!\n");
		} else {
			fprintf(stderr, "user is not in authorized group!\n");
		}
		exit(ret);
	}
	return 1;
}
