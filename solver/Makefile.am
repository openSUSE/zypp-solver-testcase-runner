#
# Makefile.am for zypp/solver/testsuite
#

SUBDIRS = src single.out config lib solver.test
AUTOMAKE_OPTIONS = dejagnu
PACKAGE = solver

EXTRA_DIST = data.deptestomatic/*

install-data-hook:
	$(INSTALL) -d $(DESTDIR)$(testsuitedatadir);	\
	$(INSTALL) -d $(DESTDIR)$(testsuitedatadir)/solver;	\
	$(INSTALL) -d $(DESTDIR)$(testsuitedatadir)/solver/data.deptestomatic;	\
	for f in `(cd $(srcdir)/data.deptestomatic; find -type d)`; do             \
		$(INSTALL) -d $(DESTDIR)$(testsuitedatadir)/solver/data.deptestomatic/$$f;                     \
	done;                                                       \
	for f in `(cd $(srcdir)/data.deptestomatic; find -type f)`; do             \
		$(INSTALL_DATA) $(srcdir)/data.deptestomatic/$$f $(DESTDIR)$(testsuitedatadir)/solver/data.deptestomatic/$$f; \
	done;
	for f in `(cd $(srcdir)/data.deptestomatic; find -type d -name '.svn')`; do	\
		rm -rf $(DESTDIR)$(testsuitedatadir)/solver/data.deptestomatic/$$f;	\
	done

  
uninstall:
	rm -rf $(testsuitedatadir)/solver/data.deptestomatic
