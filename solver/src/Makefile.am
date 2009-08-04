#
# Makefile.am for zypp/testsuite/solver/src
#

INCLUDES = -I/usr/include/libxml2	\
	-I$(prefix)/include		\
	-I/usr/include/Qt		\
	-I/usr/include/YaST2/yui	\
	-DQT3_SUPPORT			\
	-DZYPP_BASE_LOGGER_LOGGROUP=\"testsuite\"


LDADD =	-lzypp -lexpat -lQtCore -lQtGui -lQt3Support -lqdialogsolver -lyui

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
