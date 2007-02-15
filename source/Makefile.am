#
# Makefile.am for testsuite/source/tests
#
#SUBDIRS = data
LDADD =	$(libdir)/libzypp.la $(top_srcdir)/src/utils/lib@PACKAGE@_testsuite_utils.la

INCLUDES = 						\
	-DZYPP_BASE_LOGGER_LOGGROUP=\"testsuite\"

noinst_PROGRAMS = \
	suseparser \
	source_read_test 

testsuitesourcebindir = ${testsuitebindir}
testsuitesourcebin_DATA = suseparser	\
						  source_read_test

# does not compile
# packagetagparsertest_SOURCES = PackageTagParserTest.cc
	
# does not find zypp/source/susetags/*
#selectiontagparsertest_SOURCES = SelectionTagParserTest.cc
#patterntagparsertest_SOURCES = PatternTagParserTest.cc
#producttagparsertest_SOURCES = ProductMetadataParserTest.cc
#mediatagparsertest_SOURCES = MediaMetadataParserTest.cc
suseparser_SOURCES = suseparser.cc KeyRingCallbacks.h
source_read_test_SOURCES = source-read-test.cc
#content_file_test_SOURCES = content_file_test.cc
#content_file_test_LDADD = $(LDADD) -lboost_unit_test_framework

EXTRA_DIST = data/*

install-data-hook:
	$(INSTALL) -d $(DESTDIR)$(testsuitedatadir);	\
	$(INSTALL) -d $(DESTDIR)$(testsuitedatadir)/source;	\
	$(INSTALL) -d $(DESTDIR)$(testsuitedatadir)/source/data;	\
	for f in `(cd $(srcdir)/data; find -type d)`; do             \
		$(INSTALL) -d $(DESTDIR)$(testsuitedatadir)/source/data/$$f;                     \
	done;                                                       \
	for f in `(cd $(srcdir)/data; find -type f)`; do             \
		$(INSTALL_DATA) $(srcdir)/data/$$f $(DESTDIR)$(testsuitedatadir)/source/data/$$f; \
	done;
	for f in `(cd $(srcdir)/data; find -type d -name '.svn')`; do	\
		rm -rf $(DESTDIR)$(testsuitedatadir)/source/data/$$f;	\
	done
  
uninstall:
	rm -rf $(testsuitedatadir)/source/data
