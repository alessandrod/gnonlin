DISTCHECK_CONFIGURE_FLAGS=--enable-gtk-doc

SUBDIRS = \
	gnl \
	tests \
	docs \
	m4 \
	common

DIST_SUBDIRS = $(SUBDIRS)

EXTRA_DIST = \
	gnonlin.spec depcomp \
	AUTHORS COPYING NEWS README RELEASE REQUIREMENTS \
	ChangeLog autogen.sh

ACLOCAL_AMFLAGS = -I m4 -I common/m4

DISTCLEANFILES = _stdint.h

include $(top_srcdir)/common/release.mak
include $(top_srcdir)/common/po.mak

check-valgrind:
	cd tests/check && make check-valgrind

if HAVE_CHECK
check-torture:
	cd tests/check && make torture
else
check-torture:
	true
endif

include $(top_srcdir)/common/coverage/lcov.mak

CRUFT_FILES = \
	$(top_builddir)/common/shave \
	$(top_builddir)/common/shave-libtool \
	$(top_builddir)/common/m4/libtool.m4 \
	$(top_builddir)/common/m4/ltoptions.m4 \
	$(top_builddir)/common/m4/ltsugar.m4 \
	$(top_builddir)/common/m4/ltversion.m4 \
	$(top_builddir)/common/m4/lt~obsolete.m4

include $(top_srcdir)/common/cruft.mak

all-local: check-cruft
