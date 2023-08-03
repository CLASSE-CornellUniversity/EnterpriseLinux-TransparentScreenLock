SUBDIRS = src
EXTRA_DIST = bin
EXTRA_DIST += tlock.spec

PHONY: gitclean
gitclean: distclean
	rm -rf autom4te.cache 
	rm -f Makefile.in src/Makefile.in
	rm -f aclocal.m4 config.* configure depcomp install-sh missing
	rm -f *.tar.*
