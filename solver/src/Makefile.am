#
# Makefile.am for zypp/testsuite/solver/src
#

INCLUDES = -I/usr/include/libxml2	\
	-I$(prefix)/include		\
	-I/usr/lib/qt3/include
	-DZYPP_BASE_LOGGER_LOGGROUP=\"testsuite\"


LDADD =	$(libdir)/libzypp.so $(libdir)/libsatsolver.a -lexpat -L$(QTLIBDIR) -lqt-mt -lqdialogsolver

# We're not ready for this yet.
#check_PROGRAMS = deptestomatic.multi
noinst_PROGRAMS = deptestomatic.multi

testsuitesolversrcbindir = ${testsuitebindir}
testsuitesolversrcbin_DATA = deptestomatic.multi

deptestomatic_multi_SOURCES =				\
	deptestomatic.cc 				\
	KeyRingCallbacks.h				\
	XmlNode.cc					\
	XmlNode.h
