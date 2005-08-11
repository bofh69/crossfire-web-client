SUBDIRS=@CF_SUBDIRS@

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
	configure.in		\
	crossfire-client.spec


PIXMAPS = \
	pixmaps/16x16.png	\
	pixmaps/32x32.png	\
	pixmaps/48x48.png	\
	pixmaps/all.xpm		\
	pixmaps/applied.xbm	\
	pixmaps/applied.xpm	\
	pixmaps/bg.xpm		\
	pixmaps/clear.xbm	\
	pixmaps/close.xbm	\
	pixmaps/close.xpm	\
	pixmaps/coin.xpm	\
	pixmaps/crossfiretitle.xpm	\
	pixmaps/cursed.xbm	\
	pixmaps/cursed.xpm	\
	pixmaps/damned.xbm	\
	pixmaps/damned.xpm	\
	pixmaps/dot.xpm		\
	pixmaps/hand.xpm	\
	pixmaps/hand2.xpm	\
	pixmaps/lock.xpm	\
	pixmaps/locked.xbm	\
	pixmaps/locked.xpm	\
	pixmaps/mag.xpm		\
	pixmaps/magic.xbm	\
	pixmaps/magic.xpm	\
	pixmaps/nonmag.xpm	\
	pixmaps/question.111	\
	pixmaps/question.sdl	\
	pixmaps/question.xpm	\
	pixmaps/sign_east.xpm   \
	pixmaps/sign_flat.xpm   \
	pixmaps/sign_west.xpm   \
	pixmaps/skull.xpm	\
	pixmaps/stipple.111	\
	pixmaps/stipple.112	\
	pixmaps/test.xpm	\
	pixmaps/unlock.xpm	\
	pixmaps/unpaid.xbm	\
	pixmaps/unpaid.xpm	

HELP= \
	help/about.h		\
	help/chelp.h		\
	help/shelp.h		\
	help/bugreport.h

UTILS = \
	utils/config.guess	\
	utils/config.sub	\
	utils/depcomp		\
	utils/deftoheader.pl	\
	utils/install-sh	\
	utils/mdk.sh		\
	utils/missing		\
	utils/mkinstalldirs


all:
	@for dir in $(SUBDIRS); \
	    do \
	    echo "making all in $$dir..."; \
	    (cd $$dir;  $(MAKE) $(MFLAGS) 'MFLAGS=$(MFLAGS)' all; ) \
	done;


depend:
	@if [ "X$(DEPEND)" = "X" ] ; then \
	    echo "No makedepend - will not depend files"; \
	else \
	    for dir in $(SUBDIRS); \
		do \
	    	echo "making depend in $$dir..."; \
	    	(cd $$dir;  $(MAKE) $(MFLAGS) 'MFLAGS=$(MFLAGS)' depend; ) \
	    done; \
	fi

clean:
	@for dir in $(SUBDIRS); \
	    do \
	    echo "making clean in $$dir..."; \
	    (cd $$dir;  $(MAKE) $(MFLAGS) 'MFLAGS=$(MFLAGS)' clean; ) \
	done;

distclean:
	$(RM) -f Makefile Makefile.bak config.cache config.log config.status
	@for dir in $(SUBDIRS); \
	    do \
	    echo "making distclean in $$dir..."; \
	    (cd $$dir;  $(MAKE) $(MFLAGS) 'MFLAGS=$(MFLAGS)' distclean; ) \
	done;

proto:
	@for dir in $(SUBDIRS); \
	    do \
	    echo "making proto in $$dir..."; \
	    (cd $$dir;  $(MAKE) $(MFLAGS) 'MFLAGS=$(MFLAGS)' proto; ) \
	done;

archive:
	if [ -d /tmp/$(PACKAGE_TARNAME)-$(VERSION) ]; then $(RM) -rf /tmp/$(PACKAGE_TARNAME)-$(VERSION); fi
	$(MKDIR) /tmp/$(PACKAGE_TARNAME)-$(VERSION) /tmp/$(PACKAGE_TARNAME)-$(VERSION)/utils /tmp/$(PACKAGE_TARNAME)-$(VERSION)/help /tmp/$(PACKAGE_TARNAME)-$(VERSION)/pixmaps
	$(CP) $(EXTRA_DIST) /tmp/$(PACKAGE_TARNAME)-$(VERSION)
	$(CP) $(PIXMAPS) /tmp/$(PACKAGE_TARNAME)-$(VERSION)/pixmaps
	$(CP) $(HELP) /tmp/$(PACKAGE_TARNAME)-$(VERSION)/help
	$(CP) $(UTILS) /tmp/$(PACKAGE_TARNAME)-$(VERSION)/utils
	@for dir in $(DIST_SUBDIRS); \
	    do \
	    echo "making archive in $$dir..."; \
	    (cd $$dir;  $(MAKE) $(MFLAGS) 'MFLAGS=$(MFLAGS)' archive; ) \
	done;
	(cd /tmp; $(RM) $(PACKAGE_TARNAME)-$(VERSION).tgz; \
		$(TAR) cvfhz $(PACKAGE_TARNAME)-$(VERSION).tar.gz $(PACKAGE_TARNAME)-$(VERSION) )
	$(CP) /tmp/$(PACKAGE_TARNAME)-$(VERSION).tar.gz ../
	$(RM) -rf /tmp/$(PACKAGE_TARNAME)-$(VERSION) /tmp/$(PACKAGE_TARNAME)-$(VERSION).tar.gz

install:
	@for dir in $(SUBDIRS); \
	    do \
	    echo "making install in $$dir..."; \
	    (cd $$dir;  $(MAKE) $(MFLAGS) 'MFLAGS=$(MFLAGS)' install; ) \
	done;
