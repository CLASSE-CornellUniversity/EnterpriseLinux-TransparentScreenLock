/*
 ============================================================================
 Name        : tlock_xs_auth_pam.c
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
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <grp.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>

#include <security/pam_appl.h>
#include <security/pam_modules.h>

#define MAX_NO_GROUPS 50

static const char* PAM_xs_username = NULL;
static const char* PAM_xs_password = NULL;
static struct passwd* pwd_xs_entry = NULL;
static const char* gid_entry[10];
static int initialized = 0;

static int nulll_conv(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr)
{
	struct pam_response *reply;
	*resp = NULL;

	// assign memory according the number of messages.
	reply = (struct pam_response *) malloc(sizeof(struct pam_response));
	reply[0].resp = (char *)PAM_xs_password;
	reply[0].resp_retcode = 0;

	*resp = reply;
	_PRINT_;
	return PAM_SUCCESS;
}

/*------------------------------------------------------------------ */

static struct pam_conv PAM_xs_conversation =
	{ &nulll_conv, NULL };

/*------------------------------------------------------------------*/

/**
 *
 */
static int authenticate(char *service, char *user, char *pass)
{

	pam_handle_t *pamh = NULL;
	int retval = pam_start(service, user, &PAM_xs_conversation, &pamh);
	_PRINT_;
	if (retval == PAM_SUCCESS)
	{

		//_SYSLOG_("user=%s, password=%s\n", user, pass);

		retval = pam_authenticate(pamh, 0);

		_PRINT_;
		if (retval == PAM_SUCCESS)
		{
			_SYSLOG_("authenticated!! service=%s, user=%s\n", service, user);
		} else
		{
			_SYSLOG_("authentication failed. service=%s, user=%s [return=%d]\n", service, user, retval);
		}

		_PRINT_;
		pam_end(pamh, PAM_SUCCESS);
		_PRINT_;
		return retval;
	}_PRINT_;

	return retval;
}

/**
 *
 */
static int tlock_auth_xspam_init(const char* args)
{
	errno = 0;
	char* _temp = strdup(args);

	if (initialized)
	{
		perror("already initialized");
		return 0;
	}

	if (!pwd_xs_entry)
	{
		if (args != NULL)
		{
			pwd_xs_entry = getpwnam(args);
			if (pwd_xs_entry)
			{
				_SYSLOG_("pwd entry by args=%s\n", _temp);
			}
		}
		if (!pwd_xs_entry && getenv("USER"))
		{
			pwd_xs_entry = getpwnam(getenv("USER"));
			_SYSLOG_("pwd entry by env=%s\n", getenv("USER"));
		}
		if (!pwd_xs_entry)
		{
			pwd_xs_entry = getpwuid(getuid());
			_SYSLOG_("pwd entry by uid()=%u\n", getuid());
		}
	}
	if (!pwd_xs_entry)
	{
		perror("password entry for uid not found");
		return 0;
	}

	/* Notation xspam,gid,gid,gid */

	int ic = 0;
	// ignore the first one since the format is xspam,500,0
	strtok(_temp, ","); // Splits spaces between words in str
	// maximum of groups that can be specified is 10
	for (; ic < 10; ic++)
	{
		gid_entry[ic] = strtok(NULL, ",");
		if (!gid_entry[ic])
		{
			gid_entry[ic] = 0;
		}
	}
	//ic = 1;
	for (ic = 0; gid_entry[ic] && ic < 10; ic++)
	{
		_SYSLOG_("xs_auth_pam.%s: group[%d]=%s\n", __FUNCTION__, ic, gid_entry[ic]);
	}

	initialized = 1;
	/* we can be installed setuid root to support shadow passwords,
	 and we don't need root privileges any longer.  --marekm */
	//setuid(getuid());
	return 1;
}

static int tlock_auth_xspam_deinit()
{
	pwd_xs_entry = NULL;
	return 0;
}

/* Main Sequence when PAM Auth gets invoked */
static int tlock_auth_xspam_auth(const char* user, const char* pass, int as_gid)
{
	struct group *gr;
	gid_t gids[15 + 1];
	int count, ret, i, j = 0;

	_PRINTF_("entering %s = %s\n", strdup(user), strdup(pass));

	for (i = 0; gid_entry[i] && i < 10; i++)
	{
		_SYSLOG_("xs_auth_pam: group[%d]=%s\n", i, gid_entry[i]);
	}

	PAM_xs_username = user == NULL ? pwd_xs_entry->pw_name : user;
	PAM_xs_password = pass;

	syslog( LOG_NOTICE, "xs_auth_pam: starting pam_conv(%s)\n", PAM_SERVICE_NAME);

	// groups delimited by comma
	//fprintf(stderr, "xs_auth_pam: authenticate(%s=%s)\n", PAM_xs_username, PAM_xs_password);

	if (authenticate(PAM_SERVICE_NAME, (char *)PAM_xs_username, (char *)PAM_xs_password) != PAM_SUCCESS)
	{
		_PRINT_;
		PAM_xs_username = NULL;
		PAM_xs_password = NULL;
		return 0;
	}_PRINT_;

	/* pwd is not null, and no error reported.... lets get the group list
	 * from the native interface the gecos.
	 */

	/* Retrieve group list */
	count = MAX_NO_GROUPS;
	ret = getgrouplist(PAM_xs_username, pwd_xs_entry->pw_gid, gids, &count);
	_SYSLOG_(
	      "xs_auth_pam: user '%s' found in %d group(s) [%s-%s].\n",
	      PAM_xs_username,
	      count,
	      pwd_xs_entry->pw_passwd,
	      pwd_xs_entry->pw_name);

	if (ret != -1)
	{
		for (i = 0; i < count; i++)
		{
			_PRINTF_("xs_auth_pam: counting %d of %d\n", i, count);
			_SYSLOG_("xs_auth_pam: getgrouplist %d: %u. %s\n", i, gids[i], gid_entry[0]);
			for (j = 0; gid_entry[j] && j < 10; j++)
			{
				if (as_gid)
				{
					_SYSLOG_("xs_auth_pam: gid[%d][%s]=%s (gid=%d)\n", i, PAM_xs_username, gid_entry[j], gids[i]);
					int grp = atoi(gid_entry[j]);
					if ((unsigned) gids[i] == (unsigned) grp)
					{
						_SYSLOG_("xs_auth_pam: match found for %s in group %u == %u\n", PAM_xs_username, grp, gids[i]);
						PAM_xs_username = NULL;
						PAM_xs_password = NULL;
						return 1;
					}
				} else
				{
					_SYSLOG_("xs_auth_pam: gid[%d][%s]=%s\n", i, PAM_xs_username, gid_entry[j]);
					if ((gr = getgrgid(gids[i])) == NULL)
					{
						_SYSLOG_("xs_auth_pam: unable to get group for %u\n", gids[i]);
						PAM_xs_username = NULL;
						PAM_xs_password = NULL;
						return 1;
					}
					if (strcmp(gr->gr_name, gid_entry[j]) == 0)
					{
						_SYSLOG_("xs_auth_pam: match found for %s in group %u\n", PAM_xs_username, gids[i]);
						PAM_xs_username = NULL;
						PAM_xs_password = NULL;
						return 1;
					}

				}
			}
		}
		_SYSLOG_("xs_auth_pam: no match found for %s\n", PAM_xs_username);
	} else
	{
		_SYSLOG_("xs_auth_pam: getgrouplist() returned -1 for %s.\n", PAM_xs_username);
	}
	PAM_xs_username = NULL;
	PAM_xs_password = NULL;
	return 0;
}

struct aAuth tlock_auth_xspam =
	{ "xspam", tlock_auth_xspam_init, tlock_auth_xspam_auth, tlock_auth_xspam_deinit };

