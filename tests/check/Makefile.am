include $(top_srcdir)/common/check.mak

CHECK_REGISTRY = $(top_builddir)/tests/check/test-registry.xml

REGISTRY_ENVIRONMENT = \
	GST_REGISTRY=$(CHECK_REGISTRY)

TESTS_ENVIRONMENT = \
        CK_DEFAULT_TIMEOUT=120                                   \
	$(REGISTRY_ENVIRONMENT)					\
	GST_PLUGIN_SYSTEM_PATH=					\
	GST_PLUGIN_PATH=$(top_builddir)/gnl:$(GSTPB_PLUGINS_DIR):$(GST_PLUGINS_DIR):$(GST_PLUGINS_DIR)

# ths core dumps of some machines have PIDs appended
CLEANFILES = core.* test-registry.xml

clean-local: clean-local-check

TESTS = $(check_PROGRAMS)

check_PROGRAMS =	\
	gnl/simple	\
	gnl/complex	\
	gnl/gnlsource	\
	gnl/gnloperation	\
	gnl/gnlcomposition

noinst_HEADERS = \
	gnl/common.h
# these tests don't even pass
noinst_PROGRAMS =

AM_CFLAGS = $(GST_OBJ_CFLAGS) $(GST_CHECK_CFLAGS) $(CHECK_CFLAGS)
LDADD = $(GST_OBJ_LIBS) $(GST_CHECK_LIBS) $(CHECK_LIBS)

SUPPRESSIONS = $(top_srcdir)/common/gst.supp
