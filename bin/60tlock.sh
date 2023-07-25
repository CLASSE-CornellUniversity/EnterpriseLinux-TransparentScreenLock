#!/bin/bash
# ============================================================================
# Name        : 60tlock.sh
# Author      : akaan
# Version     : 1.0
# Copyright   : Cornell University - cornell.edu
# Description : tlock - transparent lock, Ansi-style
# Created     : Apr 8, 2014
#============================================================================
#
# Usage: drop script in the /etc/X11/xinit/xinitrc.d/ and alter the group id's 
#		that allow unlocking the screen 
#
 
UNLOCK_GROUPS="cmpgrp,root"

TLOCK="/usr/local/bin/tlock"
XAUTOLOCK="/usr/bin/xautolock"

if [ -x $TLOCK ]; then
	TLOCK="$TLOCK -auth xspam,${UNLOCK_GROUPS}"
	if [ -x ${XAUTOLOCK} ]; then
		$XAUTOLOCK -noclose -time 15 -locker "$TLOCK" &
		sleep 1 && $XAUTOLOCK -locknow  &
	else
		$TLOCK
	fi
fi
