#
# Makefile.am for zypp/testsuite/solver/src
#
noinst_PROGRAMS = deptestomatic

testsuitesolversrcbindir = ${testsuitebindir}
testsuitesolversrcbin_DATA = deptestomatic

deptestomatic_SOURCES =		\
	deptestomatic.cc 	\
	KeyRingCallbacks.h	\
	XmlNode.cc		\
	XmlNode.h

deptestomatic_INCLUDES = -I/usr/include/libxml2 -I$(prefix)/include
deptestomatic_CXXFLAGS = -DZYPP_BASE_LOGGER_LOGGROUP=\"testsuite\" -DNOUI
deptestomatic_LDADD    =-lzypp -lxml2 -lexpat
