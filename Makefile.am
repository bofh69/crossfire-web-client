ACLOCAL_AMFLAGS = -I macros --install

SUBDIRS = ${CF_SUBDIRS}

EXTRA_DIST = \
	ChangeLog		\
	COPYING			\
	License			\
	Makefile.in		\
	NOTES			\
	README			\
	TODO			\
	aclocal.m4		\
	configure		\
	configure.ac		\
	crossfire-client.spec

dox:
	@ doxygen 2>doxygen.err
