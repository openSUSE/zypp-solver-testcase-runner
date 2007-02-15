#
# Makefile.am for testsuite/parser
#
SUBDIRS = tests config lib parser.test
AUTOMAKE_OPTIONS = dejagnu
PACKAGE = parser

LDADD = $(libdir)/libzypp.la

noinst_PROGRAMS = YUMtest YUMParserTest

testsuiteparserbindir = ${testsuitebindir}
testsuiteparserbin_DATA = YUMtest YUMParserTest

YUMtest_SOURCES = YUMtest.cc

YUMParserTest_SOURCES = YUMParserTest.cc
YUMParserTest_LDADD = $(LDADD) -lboost_unit_test_framework

#YUMLeakTest_SOURCES = YUMLeakTest.cc

EXTRA_DIST = lib/*.exp config/*.exp $(PACKAGE).test/*.exp

clean-local:
	rm -rf *.log
	rm -rf *.exp
	rm -rf tmp*

.PHONY:        always

$(noinst_PROGRAMS):    $(libdir)/libzypp.la
