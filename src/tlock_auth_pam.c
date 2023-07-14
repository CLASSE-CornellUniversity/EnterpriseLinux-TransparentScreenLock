/*
 ============================================================================
 Name        : tlock_auth_pam.c
 Author      : akaan
 Version     :
 Copyright   : Cornell University - cornell.edu
 Published   : GNU Public License v2 (GPLv2)
 Description : tlock - transparent lock, Ansi-style
 Created     : Mar 9, 2014
 ============================================================================
 */

#include "tlock.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#ifdef __linux
#    include <security/pam_misc.h>
#endif /* LINUX */


/* ---------------------------------------------------------------- *\
\* ---------------------------------------------------------------- */

#define PAM_EXCEPTION { \
    if (PAM_error != 0 || pam_error != PAM_SUCCESS) { \
        fprintf(stderr, "pam error:%s\n", pam_strerror(pam_handle, pam_error)); \
        pam_end(pam_handle, pam_error); \
        PAM_username = NULL; \
        PAM_password = NULL; \
        return 0;\
    } \
}

#define GET_MEM \
   size += sizeof(struct pam_response); \
   if ((reply = realloc(reply, size)) == NULL) { \
       PAM_error = 1; \
       return PAM_CONV_ERR; \
   }

static const char* PAM_username = NULL;
static const char* PAM_password = NULL;
static int PAM_error = 0;
//static int pam_error = PAM_SUCCESS;

/* PAM_putText - method to have pam_converse functionality with in XLOCK */
static void PAM_putText(const struct pam_message *msg,
			struct pam_response *resp, Bool PAM_echokeys) {
	(void) printf("PAM_putText: message of style %d received: (%s)\n",
			msg->msg_style, msg->msg);
}

static int PAM_conv(int num_msg, const struct pam_message **msgs,
		    struct pam_response **resp, void *appdata_ptr) {
	int count = 0;
	unsigned int replies = 0U;
	struct pam_response *reply = NULL;
	size_t size = (size_t) 0U;

	(void) appdata_ptr;
	*resp = NULL;

	// assign memory according the number of messages.
	reply = (struct pam_response *) malloc(
			sizeof(struct pam_response) * num_msg);

	// reply[] members is not initialized!
	// As a result - abort trap when PAM tries to free reply structure
	// after PAM_ERROR_MSG processing.
	if (!reply)
		return PAM_BUF_ERR;

	// So I just initialize reply here with default values and drop
	// initialization from code below (if code matches).
	reply[replies].resp_retcode = PAM_SUCCESS; // be optimistic
	reply[replies].resp = NULL;

	for (count = 0; count < num_msg; count++) {
#ifdef DEBUG_FLAG
		(void) printf("PAM_conv: message of style (%d) received\n",
				msgs[replies]->msg_style);
		(void) printf(" + Message is: (%s)\n", msgs[replies]->msg);
#endif
		switch (msgs[count]->msg_style) {
			case PAM_PROMPT_ECHO_ON:
#ifdef DEBUG_FLAG
				(void) printf(" + Message style: PAM_PROMPT_ECHO_ON\n");
#endif
				GET_MEM;
				memset(&reply[replies], 0, sizeof reply[replies]);
				if ((reply[replies].resp = strdup(PAM_username)) == NULL) {
#ifdef PAM_BUF_ERR
					reply[replies].resp_retcode = PAM_BUF_ERR;
#endif
					PAM_error = 1;
					return PAM_CONV_ERR;
				}
				reply[replies++].resp_retcode = PAM_SUCCESS;
				/* PAM frees resp */
				break;
			case PAM_PROMPT_ECHO_OFF:
#ifdef DEBUG_FLAG
				(void) printf(" + Message style: PAM_PROMPT_ECHO_OFF\n");
#endif
				GET_MEM;
				memset(&reply[replies], 0, sizeof reply[replies]);
				if ((reply[replies].resp = strdup(PAM_password)) == NULL) {
#ifdef PAM_BUF_ERR
					reply[replies].resp_retcode = PAM_BUF_ERR;
#endif
					PAM_error = 1;
					return PAM_CONV_ERR;
				}
				reply[replies++].resp_retcode = PAM_SUCCESS;
				/* PAM frees resp */
				break;
			case PAM_TEXT_INFO:
#ifdef DEBUG_FLAG
				(void) printf(" + Message style: PAM_TEXT_INFO\n");
#endif
				if (strstr(msgs[replies]->msg, "Password") == NULL) {
					PAM_putText(msgs[replies], &reply[replies], False);
				}
				/* PAM frees resp */
				break;
			case PAM_ERROR_MSG:
#ifdef DEBUG_FLAG
				(void) printf(" + Message style: PAM_ERROR_MSG\n");
#endif
				if (strstr(msgs[replies]->msg, "Password") == NULL) {
					PAM_putText(msgs[replies], &reply[replies], False);
				}
				/* PAM frees resp */
				break;
			default: /* Must be an error of some sort... */
#ifdef DEBUG_FLAG
				(void) printf(" + Message style: unknown\n");
#endif
				free(reply);
				PAM_error = 1;
				return PAM_CONV_ERR;
		}
#ifdef DEBUG_FLAG
		(void) printf(" + Response is: (%s). Return Code is: (%d)\n",
				reply[replies].resp, reply[replies].resp_retcode);
#endif
	}
	*resp = reply;
	return PAM_SUCCESS;
}

static struct pam_conv PAM_conversation = { &PAM_conv, NULL };

/*------------------------------------------------------------------*\
\*------------------------------------------------------------------*/

static struct passwd* pwd_entry = NULL;

static int tlock_auth_pam_init(const char* args) {
	errno = 0;
	pwd_entry = getpwuid(getuid());
	if (!pwd_entry) {
		perror("password entry for uid not found");
		return 0;
	}

	/* we can be installed setuid root to support shadow passwords,
	 and we don't need root privileges any longer.  --marekm */
	setuid(getuid());

	return 1;
}

static int tlock_auth_pam_deinit() {
	pwd_entry = NULL;
	return 0;
}

/* Main Sequence when PAM Auth gets invoked */
static int tlock_auth_pam_auth(const char* username, const char* pass, int as_gid) {

	pam_handle_t* pam_handle = NULL;
	const char* user;
	gid_t gids[15 + 1];
	int count, pam_error, ret, i;

	if (!pass || strlen(pass) < 1 || !pwd_entry)
		return 0;

	PAM_username = pwd_entry->pw_name;
	PAM_password = pass;

	fprintf(stderr, "auth: starting pam_conv(%s)\n", PAM_SERVICE_NAME);

	pam_error = pam_start(PAM_SERVICE_NAME, PAM_username, &PAM_conversation, &pam_handle);
	PAM_EXCEPTION;
	pam_error = pam_set_item(pam_handle, PAM_TTY, ttyname(0));
	PAM_EXCEPTION;
	/* At this point we have the interface to PAM for the PAM_user
	 * so we can end the session and use libc functionality to get
	 * the groups and other administrative data for the the via
	 * libc.pwnam functions.
	 */
	/* get mapped user namqe; PAM may have changed it */
	pam_error = pam_get_item(pam_handle, PAM_USER, (const void **) &user);
	if ((pwd_entry = getpwnam(user)) == NULL) {
		pam_error = 1;
	}
	PAM_EXCEPTION;

	pam_error = pam_end(pam_handle, pam_error);
	PAM_EXCEPTION;

	/* pwd is not null, and no error reported.... lets get the group list
	 * from the native interface the gecos.
	 */

	/* Retrieve group list */
	count = 11;
	ret = getgrouplist(user,pwd_entry->pw_gid, gids, &count);
	printf("User '%s' found in %d group(s).\n", PAM_username, ret+1);
	if (ret != -1) {
		for (i=0;i<count;i++) {
			printf("group#%d: %u\n", i, (unsigned)gids[i]);
		}
	}
	return 0;
}

//	if (initgroups(user, pwd_entry->pw_gid) != 0) {
//		fprintf(stderr, "exception initgroups\n", ngroups);
//		PAM_EXCEPTION;
//	}
//	else {
//		int ret = getgroups(&ngroups, groups);
//		fprintf(stderr, "alternate groups = %d\n", ngroups);
//	}
//	if (getgrouplist(user, pwd_entry->pw_gid, groups, &ngroups) < 0) {
//		fprintf(stderr, "getgrouplist() returned -1; ngroups = %d\n", ngroups);
//		gid_t *new;
//		if (ngroups == initial_buffer) {
//			ngroups *= 16;
//		}
//		new = malloc(ngroups * sizeof(groups[0]));
//		if (new == NULL) {
//			fprintf(stderr, "getgrouplist(): No memory for groups\n");
//			free(groups);
//			PAM_EXCEPTION
//		}
//		groups = new;
//		errorno = 0;
//		if (getgrouplist(user, pwd_entry->pw_gid, groups, &ngroups) < 0) {
//			if (errorno == 0) {
//				fprintf(stderr, "getgrouplist(): it appears that the username \"%s\" is in more than %d groups\n", user, ngroups);
//			}
//			else {
//				fprintf(stderr, "getgrouplist(): failed to get groups for username \"%s\" with primary GID %s.\n", user, pwd_entry->pw_gid);
//				free(groups);
//				PAM_EXCEPTION
//			}
//		}
//
//	}

	/* Display list of retrieved groups, along with group names */

//	fprintf(stderr, "ngroups = %d\n", ngroups);
//	for (j = 0; j < ngroups; j++) {
//		gr = getgrgid(groups[j]);
//		if (gr != NULL)
//			printf("%d , %d (%s)", j, groups[j], gr->gr_name);
//		else
//			continue;
//		printf("\n");
//	}


struct aAuth tlock_auth_pam = { "pam", tlock_auth_pam_init, tlock_auth_pam_auth,
		tlock_auth_pam_deinit };

