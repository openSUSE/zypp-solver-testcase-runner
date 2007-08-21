#
# Makefile.am for zypp/testsuite/solver/src
#

INCLUDES = -I/usr/include/libxml2	\
	-I/usr/lib/qt3/include
	-DZYPP_BASE_LOGGER_LOGGROUP=\"testsuite\"

LDADD =	$(libdir)/libzypp.so $(top_srcdir)/src/helix/lib@PACKAGE@_helix.la -L$(QTLIBDIR) -lqt-mt -lqdialogsolver
noinst_PROGRAMS = solvertree

solvertree_SOURCES = solvertree.cc