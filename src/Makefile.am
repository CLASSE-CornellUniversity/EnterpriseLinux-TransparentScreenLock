bin_PROGRAMS=tlock tlock_cli
tlock_SOURCES=tlock_password_dialog.c tlock_auth_none.c tlock_auth_pam.c tlock_xs_auth_pam.c tlock_cursor_none.c tlock_bg_none.c tlock_frame.c tlock.c tlock.h tlock_password_dialog.h tlock_frame.h
tlock_LDFLAGS=-L/usr/X11R6/lib
tlock_CFLAGS=-I/usr/X11R6/include
tlock_LDADD=-lX11 -lpam 

tlock_cli_SOURCES=tlock_auth_none.c  tlock_auth_pam.c tlock_xs_auth_pam.c tlock_cli.c
tlock_cli_LDFLAGS=-L/usr/X11R6/lib
tlock_cli_CFLAGS=-I/usr/X11R6/include
tlock_cli_LDADD=-lX11 -lpam

