/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 * zypptestomatic.cc
 *
 * Copyright (C) 2002 Ximian, Inc.
 * Copyright (C) 2005 SUSE Linux Products GmbH
 *
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */
#include <sstream>
#include <iostream>
#include <map>
#include <set>
#include <stdio.h>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include "zypp/ZYpp.h"
#include "zypp/ZYppFactory.h"
#include "zypp/ResObjects.h"
#include "zypp/ResPool.h"
#include "zypp/PoolItem.h"
#include "zypp/Capability.h"
#include "zypp/Capabilities.h"
#include "zypp/ResolverProblem.h"
#include "zypp/ProblemSolution.h"
#include "zypp/RepoManager.h"
#include "zypp/ResPool.h"
#include "zypp/ResFilters.h"
#include "zypp/ResolverProblem.h"
#include "zypp/Locale.h"
#include "zypp/ZConfig.h"

#include "zypp/base/String.h"
#include "zypp/base/LogTools.h"
#include "zypp/base/LogControl.h"
#include "zypp/base/Exception.h"
#include "zypp/base/Algorithm.h"

#include "zypp/ui/Selectable.h"

#include "zypp/media/MediaManager.h"

#include "zypp/Repository.h"

#include "zypp/solver/detail/Resolver.h"
#include "zypp/solver/detail/Testcase.h"
#include "zypp/solver/detail/SolverQueueItem.h"
#include "zypp/solver/detail/SolverQueueItemDelete.h"
#include "zypp/solver/detail/SolverQueueItemInstall.h"
#include "zypp/solver/detail/SolverQueueItemInstallOneOf.h"
#include "zypp/solver/detail/SolverQueueItemLock.h"
#include "zypp/solver/detail/SolverQueueItemUpdate.h"
#include "zypp/solver/detail/SystemCheck.h"

#include "KeyRingCallbacks.h"
#include "XmlNode.h"

#ifndef NOUI
#include <qapplication.h>
#include <qdialog.h>
#include <qfiledialog.h>
#include <qpushbutton.h>
#include <qfont.h>
#include "zypp/QZyppSolverDialog.h"

#define YUILogComponent "example"
#include <yui/YUILog.h>
#include <yui/YUI.h>
#include <yui/YWidgetFactory.h>
#include <yui/YDialog.h>
#include <yui/YLayoutBox.h>
#include <yui/YPackageSelector.h>
#include <yui/YEvent.h>
#endif

using namespace std;
using namespace zypp;
using zypp::ui::Selectable;
using zypp::solver::detail::Testcase;
using zypp::ResolverProblemList;
using zypp::solver::detail::XmlNode;
using zypp::solver::detail::XmlNode_Ptr;

//-----------------------------------------------------------------------------

static bool show_mediaid = false;
static Pathname globalPath;
static LocaleSet locales;

static ZYpp::Ptr God;
static RepoManager manager;
static bool forceResolve;
static bool ignorealreadyrecommended;
static bool onlyRequires;
static bool allowVendorChange;
static zypp::solver::detail::SolverQueueItemList solverQueue;

typedef set<PoolItem> PoolItemSet;

#define MARKER ">!> "
#define RESULT cout << MARKER

RepoManager makeRepoManager( const Pathname & mgrdir_r )
{
  RepoManagerOptions mgropt;

  mgropt.repoCachePath    = mgrdir_r/"cache";
  mgropt.repoRawCachePath = mgrdir_r/"raw_cache";
  mgropt.knownReposPath   = mgrdir_r/"repos";

  return RepoManager( mgropt );
}

class compare_problems {
public:
    int operator() (const boost::intrusive_ptr<zypp::ResolverProblem> & p1,
		    const boost::intrusive_ptr<zypp::ResolverProblem> & p2) const
	{ return p1->description() < p2->description(); }
};

//-----------------------------------------------------------------------------

static std::ostream &
printRes ( std::ostream & str, ResObject::constPtr r )
{
    if (show_mediaid) {
	Resolvable::constPtr res = r;
	Package::constPtr pkg = asKind<Package>(res);
	if (pkg) str << "[" << pkg->mediaNr() << "]";
    }
    if (r->kind() != ResKind::package)
	str << r->kind() << ':';
    str  << r->name() << '-' << r->edition();
    if (r->arch() != "") {
	str << '.' << r->arch();
    }
    string alias = r->repoInfo().alias();
    if (!alias.empty()
        && alias != "@System")
    {
        str << '[' << alias << ']';
    }

    return str;
}

//---------------------------------------------------------------------------

Resolvable::Kind
string2kind (const std::string & str)
{
    Resolvable::Kind kind = ResKind::package;
    if (!str.empty()) {
	if (str == "package") {
	    // empty
	}
	else if (str == "patch") {
	    kind = ResKind::patch;
	}
	else if (str == "pattern") {
	    kind = ResKind::pattern;
	}
	else if (str == "product") {
	    kind = ResKind::product;
	}
	else if (str == "srcpackage") {
	    kind = ResKind::srcpackage;
	}
	else {
	    cerr << "get_poolItem unknown kind '" << str << "'" << endl;
	}
    }
    return kind;
}

//---------------------------------------------------------------------------

std::ostream & dumpHelpOn( std::ostream & str )
{
  str << "\
List of known tags. See http://en.opensuse.org/Libzypp/Testsuite_solver for details: \n\
  PkgUI YOU addConflict addQueueDelete addQueueInstall addQueueInstallOneOf addQueueLock addQueueUpdate \n\
  addRequire allowVendorChange arch availablelocales channel createTestcase current distupgrade force-install \n\
  forceResolve graphic hardwareInfo ignorealreadyrecommended install instorder keep locale lock mediaid \n\
  onlyRequires reportproblems setlicencebit showpool showstatus showselectable source subscribe system \n\
  systemCheck takesolution uninstall update upgradeRepo validate verify whatprovides" << endl;


  return str;
}
/* ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** */

//==============================================================================================================================

//---------------------------------------------------------------------------------------------------------------------
// helper functions

/** Order on PoolItem.
 * \li kind
 * \li name
 * \li edition
 * \li arch
 * \li source::alias
 * \li ResObject::constPtr as fallback.
*/
struct KNEAOrder : public std::binary_function<PoolItem,PoolItem,bool>
{
    // NOTE: operator() provides LESS semantics to order the set.
    // So LESS means 'prior in set'. We want 'better' archs and
    // 'better' editions at the beginning of the set. So we return
    // TRUE if (lhs > rhs)!
    //
    bool operator()( const PoolItem lhs, const PoolItem rhs ) const
	{
	    int res = lhs->kind().compare( rhs->kind() );
	    if ( res )
		return res < 0;
	    res = lhs->name().compare( rhs->name() );
	    if ( res )
		return res < 0;
	    res = lhs->edition().compare( rhs->edition() );
	    if ( res )
		return res < 0;
	    res = lhs->arch().compare( rhs->arch() );
	    if ( res )
		return res < 0;
	    res = lhs->satSolvable().repository().info().alias().compare( rhs->satSolvable().repository().info().alias() );
	    if ( res )
		return res < 0;
	    // no more criteria, still equal:
	    // use the ResObject::constPtr (the poiner value)
	    // (here it's arbitrary whether < or > )
	    return lhs.resolvable() < rhs.resolvable();
	}
};

typedef std::set<PoolItem, KNEAOrder> PoolItemOrderSet;

struct Unique : public resfilter::PoolItemFilterFunctor
{
    PoolItemOrderSet itemset;

    bool operator()( PoolItem poolItem )
    {
	itemset.insert (poolItem);
	return true;
    }
};

// collect all installed items in a set

PoolItemOrderSet
uniquelyInstalled (const ResPool & pool)
{
    Unique info;

    invokeOnEach( pool.begin( ),
		  pool.end ( ),
                  functor::chain( resfilter::ByUninstalled (), resfilter::ByTransact() ),
		  functor::functorRef<bool,PoolItem> (info) );
    return info.itemset;
}

// collect all uninstalled items in a set

PoolItemOrderSet
uniquelyUninstalled (const ResPool & pool)
{
    Unique info;

    invokeOnEach( pool.begin( ),
		  pool.end ( ),
                  functor::chain( resfilter::ByInstalled (), resfilter::ByTransact() ),
		  functor::functorRef<bool,PoolItem> (info) );
    return info.itemset;
}

static void
print_solution ( const ResPool & pool, bool instorder)
{
    PoolItemOrderSet install = uniquelyInstalled(pool);
    PoolItemOrderSet uninstall = uniquelyUninstalled(pool);

    RESULT << "Solution :" << endl;

    for (PoolItemOrderSet::const_iterator iter = install.begin(); iter != install.end(); iter++) {
        RESULT << "install "; printRes( cout,  *iter); cout << endl;
    }
    for (PoolItemOrderSet::const_iterator iter = uninstall.begin(); iter != uninstall.end(); iter++) {
        PoolItem item = *iter;
        if (item.status() == ResStatus::toBeUninstalledDueToUpgrade) {
            RESULT << "update "; printRes( cout,  item); cout << endl;
        } else {
            RESULT << "delete "; printRes( cout,  item); cout << endl;
        }
    }

    cout << "- - - - - - - - - -" << endl;

    if (instorder) {
	cout << endl;
	RESULT << "Installation Order:" << endl << endl;

	sat::Transaction trans( sat::Transaction::loadFromPool );
	trans.order();
	RESULT << dump(trans) << endl;

	cout << "- - - - - - - - - -" << endl;
    }

    cout.flush();

    return;
}

static void print_problems( zypp::solver::detail::Resolver_Ptr resolver )
{
  ResolverProblemList problems = resolver->problems();
  problems.sort( compare_problems() );
  RESULT << problems.size() << " problems found:" << endl;
  for ( ResolverProblemList::iterator iter = problems.begin(); iter != problems.end(); ++iter )
  {
    ResolverProblem problem = **iter;
    RESULT << "Problem:" << endl;
    RESULT << problem.description() << endl;
    RESULT << problem.details() << endl;

    ProblemSolutionList solutions = problem.solutions();
    for ( ProblemSolutionList::const_iterator iter = solutions.begin(); iter != solutions.end(); ++iter )
    {
      ProblemSolution solution = **iter;
      RESULT << "   Solution:" << endl;
      RESULT << "      " << solution.description() << endl;
      RESULT << "      " << solution.details() << endl;
    }
  }
}

//---------------------------------------------------------------------------------------------------------------------

struct IsStatisfied : public resfilter::ResObjectFilterFunctor
{
    Resolvable::Kind kind;

    IsStatisfied (Resolvable::Kind k)
	: kind (k)
    {
    }

    bool operator()( PoolItem p)
    {
        if (p.isSatisfied())
            RESULT << p << " IS SATISFIED" << endl;
        else if(p.isBroken())
            RESULT << p << " IS BROKEN" << endl;
        else if(p.isRelevant())
            RESULT << p << " IS RELEVANT" << endl;
	return true;
    }
};


void isSatisfied (const string & kind_name) {
    if (kind_name.empty()) {
        IsStatisfied info (ResKind::package);
        invokeOnEach( God->pool().begin( ),
                      God->pool().end ( ),
		      functor::functorRef<bool,PoolItem> (info) );
    } else {
        Resolvable::Kind kind = string2kind (kind_name);
	IsStatisfied info (kind);

	invokeOnEach( God->pool().byKindBegin( kind ),
		      God->pool().byKindEnd( kind ),
		      functor::functorRef<bool,PoolItem> (info) );
    }
}


//---------------------------------------------------------------------------------------------------------------------
struct FindPackage : public resfilter::ResObjectFilterFunctor
{
    PoolItem poolItem;
    Resolvable::Kind kind;
    bool edition_set;
    Edition edition;
    bool arch_set;
    Arch arch;

    FindPackage (Resolvable::Kind k, const string & v, const string & r, const string & a)
	: kind (k)
	, edition_set( !v.empty() )
	, edition( v, r )
	, arch_set( !a.empty() )
	, arch( a )
    {
    }

    bool operator()( PoolItem p)
    {
	if (arch_set && arch != p->arch()) {				// if arch requested, force this arch
	    return true;
	}
	if (!p->arch().compatibleWith( ZConfig::instance().systemArchitecture() )) {
	    return true;
	}

	if (edition_set) {
	    if (p->edition().compare( edition ) == 0) {			// if edition requested, force this edition
		poolItem = p;
		return false;
	    }
	    return true;
	}

	if (!poolItem							// none yet
	    || (poolItem->arch().compare( p->arch() ) < 0)		// new has better arch
	    || (poolItem->edition().compare( p->edition() ) < 0))	// new has better edition
	{
	    poolItem = p;
	}
	return true;
    }
};

static bool set_licence = false;

static PoolItem
get_poolItem (const string & source_alias, const string & package_name, const string & kind_name = "", const string & ver = "", const string & rel = "", const string & arch = "")
{
    PoolItem poolItem;
    Resolvable::Kind kind = string2kind (kind_name);

    try {
	FindPackage info (kind, ver, rel, arch);

	invokeOnEach( God->pool().byIdentBegin( kind,package_name ),
		      God->pool().byIdentEnd( kind,package_name ),
		      resfilter::ByRepository(source_alias),
		      functor::functorRef<bool,PoolItem> (info) );

	poolItem = info.poolItem;
        if (!poolItem) {
            // try to find the resolvable over all channel. This is useful for e.g. languages
            invokeOnEach( God->pool().byIdentBegin( kind,package_name ),
                          God->pool().byIdentEnd( kind,package_name ),
                          functor::functorRef<bool,PoolItem> (info) );
            poolItem = info.poolItem;
        }
    }
    catch (Exception & excpt_r) {
	ZYPP_CAUGHT (excpt_r);
	cerr << "Can't find kind[" << kind_name << "]:'" << package_name << "': source '" << source_alias << "' not defined" << endl;
	if (kind_name.empty())
	    cerr << "Please specify kind=\"...\" in the <install.../> request." << endl;
	return poolItem;
    }

    if (!poolItem) {
	cerr << "Can't find kind: " << kind << ":'" << package_name << "' in source '" << source_alias << "': no such name/kind" << endl;
    }

    return poolItem;
}


//---------------------------------------------------------------------------------------------------------------------
// whatprovides

static PoolItemSet
get_providing_poolItems (const string & prov_name, const string & kind_name = "")
{
    sat::WhatProvides possibleProviders(Capability(prov_name, string2kind (kind_name)));
    PoolItemSet itemset;

    for_( iter, possibleProviders.begin(), possibleProviders.end() )
        itemset.insert(ResPool::instance().find( *iter ));

    return itemset;
}


//---------------------------------------------------------------------------------------------------------------------
// setup related functions

typedef multimap<string,PoolItem> ItemMap;

struct SortItem : public resfilter::PoolItemFilterFunctor
{
    ItemMap sorted;
    bool _show_all;

    SortItem( bool show_all )
	: _show_all( show_all )
    { }

    bool operator()( PoolItem poolItem )
    {
	ostringstream ostr;
	if (_show_all
	    || (poolItem.isBroken()
		|| poolItem.status().transacts()))
	{
	    printRes (ostr, poolItem);
	    sorted.insert (ItemMap::value_type(ostr.str(), poolItem));
	}
	return true;
    }
};


// collect all installed items in a set

void
print_pool( zypp::solver::detail::Resolver_Ptr resolver, const string & prefix = "", bool show_all = true, string show_licence = "false", bool verbose = false )
{
    SortItem info( show_all );
    cout << "Current pool: " <<  God->pool().size() << endl;
    invokeOnEach( God->pool().begin( ),
		  God->pool().end ( ),
		  functor::functorRef<bool,PoolItem> (info) );

    int count = 0;
    for (ItemMap::const_iterator it = info.sorted.begin(); it != info.sorted.end(); ++it) {
	cout << prefix << ++count << ": ";
	if(show_licence == "true"){
		cout << it->second << " Licence: " << it->second.status().isLicenceConfirmed();
	}else{
		cout << it->second;
	}
	cout << endl;

        if (verbose) {
            zypp::solver::detail::ItemCapKindList selectedBy = resolver->isInstalledBy(it->second);
            zypp::solver::detail::ItemCapKindList select = resolver->installs(it->second);
            for (zypp::solver::detail::ItemCapKindList::const_iterator iter = selectedBy.begin(); iter != selectedBy.end(); ++iter) {
                if (iter == selectedBy.begin()) {
                    cout << prefix << ++count << ": ";
                    cout << "   will be selected by:" << endl;
                }
                cout << prefix << ++count << ": ";
                cout << "         " << iter->item << " " << iter->cap << " " << iter->capKind << " " << iter->initialInstallation << endl;
            }
            for (zypp::solver::detail::ItemCapKindList::const_iterator iter = select.begin(); iter != select.end(); ++iter) {
                if (iter == select.begin()) {
                    cout << prefix << ++count << ": ";
                    cout << "   will select:" << endl;
                }
                cout << prefix << ++count << ": ";
                cout << "         " << iter->item << " " << iter->cap << " " << iter->capKind << " " << iter->initialInstallation << endl;
            }
        }
    }
    cout << "Pool End." << endl;
    return;
}

// set licence-bit on all items in the pool
void
set_licence_Pool()
{

	SortItem info( true );
	invokeOnEach( God->pool().begin( ),
		  God->pool().end ( ),
		  functor::functorRef<bool,PoolItem> (info) );

    for (ItemMap::const_iterator it = info.sorted.begin(); it != info.sorted.end(); ++it) {
		it->second.status().setLicenceConfirmed();
	}

	return;
}

static bool
load_source (const string & alias, const string & filename, const string & type, bool system_packages, unsigned priority)
{
    Pathname pathname = globalPath + filename;
    MIL << "'" << pathname << "'" << endl;

    Repository repo;

    if (type == "url") {
	try {
          cout << "Load from Url '" << filename << "'" << endl;
          MIL << "Load from Url '" << filename << "'" << endl;

          RepoInfo nrepo;
          nrepo.setAlias      ( alias );
          nrepo.setName       ( alias );
          nrepo.setEnabled    ( true );
          nrepo.setAutorefresh( false );
          nrepo.setPriority   ( priority );
          nrepo.addBaseUrl   ( Url(filename) );

          //manager.addRepository( nrepo );
          manager.refreshMetadata( nrepo );
          manager.buildCache( nrepo );

          manager.loadFromCache( nrepo );
	}
	catch ( Exception & excpt_r ) {
	    ZYPP_CAUGHT (excpt_r);
	    cout << "Couldn't load packages from Url '" << filename << "'" << endl;
	    return false;
	}
    }
    else {
	try {
          cout << "Load from File '" << pathname << "'" << endl;
          MIL << "Load from File '" << pathname << "'" << endl;
          zypp::Repository satRepo;

          if (alias == "@System") {
              satRepo = zypp::sat::Pool::instance().systemRepo();
          } else {
              satRepo = zypp::sat::Pool::instance().reposInsert(alias);
          }

          RepoInfo nrepo;

          nrepo.setAlias      ( alias );
          nrepo.setName       ( alias );
          nrepo.setEnabled    ( true );
          nrepo.setAutorefresh( false );
          nrepo.setPriority   ( priority );
          nrepo.addBaseUrl   ( pathname.asUrl() );

          satRepo.setInfo (nrepo);
          satRepo.addHelix( pathname );
          cout << "Loaded " << satRepo.solvablesSize() << " resolvables from " << (filename.empty()?pathname.asString():filename) << "." << endl;
	}
	catch ( Exception & excpt_r ) {
	    ZYPP_CAUGHT (excpt_r);
	    cout << "Couldn't load packages from XML file '" << filename << "'" << endl;
	    return false;
	}
    }

    if(set_licence)
      set_licence_Pool();

    return true;
}

static bool done_setup = false;

static void
parse_xml_setup (XmlNode_Ptr node)
{
    if (!node->equals("setup")) {
	ZYPP_THROW (Exception ("Node not 'setup' in parse_xml_setup():"+node->name()));
    }

    if (done_setup) {
	cerr << "Multiple <setup>..</setup> sections not allowed!" << endl;
	exit (0);
    }
    done_setup = true;

    string architecture = node->getProp( "arch" );				// allow <setup arch="...">
    if (!architecture.empty()) {
	MIL << "Setting architecture to '" << architecture << "'" << endl;
	try {
            ZConfig::instance().setSystemArchitecture(Arch( architecture ));
            setenv ("ZYPP_TESTSUITE_FAKE_ARCH", architecture.c_str(), 1);
	}
	catch( const Exception & excpt_r ) {
	    ZYPP_CAUGHT( excpt_r );
	    cerr << "Bad architecture '" << architecture << "' in <setup...>" << endl;
	    return;
	}
    }

    node = node->children();
    while (node != NULL) {
	if (!node->isElement()) {
	    node = node->next();
	    continue;
	}
	if (node->equals ("forceResolve")) {
	    forceResolve = true;

	} else if (node->equals ("ignorealreadyrecommended")) {
	    ignorealreadyrecommended = true;

	} else if (node->equals ("onlyRequires")) {
	    onlyRequires = true;

	} else if (node->equals ("allowVendorChange")) {
	    allowVendorChange = true;

	} else if (node->equals ("system")) {

	    string file = node->getProp ("file");
	    if (!load_source ("@System", file, "helix", true, 99 )) {
		cerr << "Can't setup 'system'" << endl;
		exit( 1 );
	    }

	} else if (node->equals ("hardwareInfo")) {

	    Pathname pathname = globalPath + node->getProp ("path");
	    setenv ("ZYPP_MODALIAS_SYSFS", pathname.asString().c_str(), 1);
	    RESULT << "setting HardwareInfo to: " << pathname.asString() << endl;
	} else if (node->equals ("channel")) {

	    string name = node->getProp ("name");
	    string file = node->getProp ("file");
	    string type = node->getProp ("type");
      	    string priority = node->getProp ("priority");
            unsigned prio = 99;
            if (!priority.empty())
                prio = str::strtonum<unsigned>( priority );
	    if (!load_source (name, file, type, false, prio)) {
		cerr << "Can't setup 'channel'" << endl;
		exit( 1 );
	    }

	} else if (node->equals ("source")) {

	    string url = node->getProp ("url");
	    string alias = node->getProp ("name");
	    if (!load_source( alias, url, "url", false, 99 )) {
		cerr << "Can't setup 'source'" << endl;
		exit( 1 );
	    }

	} else if (node->equals ("force-install")) {

	    string source_alias = node->getProp ("channel");
	    string package_name = node->getProp ("package");
	    string kind_name = node->getProp ("kind");

	    PoolItem poolItem;

	    poolItem = get_poolItem (source_alias, package_name, kind_name);
	    if (poolItem) {
		RESULT << "Force-installing " << package_name << " from channel " << source_alias << endl;;

		poolItem.status().setToBeInstalled(ResStatus::USER);
	    } else {
		cerr << "Unknown package " << source_alias << "::" << package_name << endl;
	    }
	} else if (node->equals ("mediaid")) {
	    show_mediaid = true;
	} else if (node->equals ("arch")) {
	    cerr << "<arch...> deprecated, use <setup arch=\"...\"> instead" << endl;
	    architecture = node->getProp ("name");
	    if (architecture.empty()) {
		cerr << "Property 'name=' in <arch.../> missing or empty" << endl;
	    }
	    else {
		MIL << "Setting architecture to '" << architecture << "'" << endl;
                ZConfig::instance().setSystemArchitecture(Arch( architecture ));
                setenv ("ZYPP_TESTSUITE_FAKE_ARCH", architecture.c_str(), 1);
	    }
	} else if (node->equals ("locale")) {
	    string loc = node->getProp ("name");
	    if (loc.empty())
		cerr << "Bad or missing name in <locale...>" << endl;
	    else {
		RESULT << "Requesting locale " << loc << endl;
		locales.insert( Locale( loc ) );
	    }
	} else if (node->equals ("systemCheck")) {
	    Pathname pathname = globalPath + node->getProp ("path");
	    RESULT << "setting systemCheck to: " << pathname.asString() << endl;
            SystemCheck::instance().setFile (pathname);
	} else if (node->equals("setlicencebit")) {
		set_licence = true;
	} else {
	    cerr << "Unrecognized tag '" << node->name() << "' in setup" << endl;
	}

	node = node->next();
    }
}



//-------------
static void
parse_xml_trial (XmlNode_Ptr node, ResPool & pool)
{
    static bool first_trial = true;

    bool verify = false;
    bool doUpdate = false;
    bool instorder = false;

    if (!node->equals ("trial")) {
	ZYPP_THROW (Exception ("Node not 'trial' in parse_xml_trial()"));
    }

    DBG << "parse_xml_trial()" << endl;

    // reset pool on subsequent trials.

    if (first_trial) {
	first_trial = false;
    }
    else {
	for (ResPool::const_iterator it = pool.begin(); it != pool.end(); ++it) {
	    if (it->status().transacts()) it->status().resetTransact( ResStatus::USER );
	}
    }

    if (! done_setup) {
	cerr << "Any trials must be preceeded by the setup!" << endl;
	exit (0);
    }

    sat::Pool satpool( sat::Pool::instance() );

    zypp::solver::detail::Resolver_Ptr resolver = new zypp::solver::detail::Resolver( pool );
    resolver->setForceResolve( forceResolve );
    resolver->setIgnoreAlreadyRecommended( ignorealreadyrecommended );
    resolver->setOnlyRequires( onlyRequires );
    if ( allowVendorChange )
      resolver->setAllowVendorChange( true );

    if (!locales.empty()) {
        pool.setRequestedLocales( locales );
    }

    node = node->children();
    while (node) {
	if (!node->isElement()) {
	    node = node->next();
	    continue;
	}

	if (node->equals("note")) {

	    string note = node->getContent ();
	    cout << "NOTE: " << note << endl;
	} else if (node->equals ("verify")) {
	    verify = true;
	} else if (node->equals ("current")) {
            // unsupported
	    string source_alias = node->getProp ("channel");
	} else if (node->equals ("subscribe")) {
            // unsupported
	    string source_alias = node->getProp ("channel");
	} else if (node->equals ("install")) {

	    string source_alias = node->getProp ("channel");
	    string name = node->getProp ("name");
	    if (name.empty())
		name = node->getProp ("package");
	    string kind_name = node->getProp ("kind");
	    string soft = node->getProp ("soft");
	    string version = node->getProp ("ver");
	    string release = node->getProp ("rel");
	    string architecture = node->getProp ("arch");

	    PoolItem poolItem;

	    poolItem = get_poolItem( source_alias, name, kind_name, version, release, architecture );
	    if (poolItem) {
		RESULT << "Installing "
		    << ((poolItem->kind() != ResKind::package) ? (poolItem->kind().asString() + ":") : "")
		    << name
		    << (version.empty()?"":(string("-")+poolItem->edition().version()))
		    << (release.empty()?"":(string("-")+poolItem->edition().release()))
		    << (architecture.empty()?"":(string(".")+poolItem->arch().asString()))
		    << " from channel " << source_alias << endl;;
		poolItem.status().setToBeInstalled(ResStatus::USER);
		if (!soft.empty())
		    poolItem.status().setSoftInstall(true);
	    } else {
		cerr << "Unknown item " << source_alias << "::" << name << endl;
		exit( 1 );
	    }

	} else if (node->equals ("uninstall")) {

	    string name = node->getProp ("name");
	    if (name.empty())
		name = node->getProp ("package");
	    string kind_name = node->getProp ("kind");
	    string soft = node->getProp ("soft");
	    string version = node->getProp ("ver");
	    string release = node->getProp ("rel");
	    string architecture = node->getProp ("arch");

	    PoolItem poolItem;

	    poolItem = get_poolItem ("@System", name, kind_name, version, release, architecture );
	    if (poolItem) {
		RESULT << "Uninstalling " << name
		    << (version.empty()?"":(string("-")+poolItem->edition().version()))
		    << (release.empty()?"":(string("-")+poolItem->edition().release()))
		    << (architecture.empty()?"":(string(".")+poolItem->arch().asString()))
		    << endl;
		poolItem.status().setToBeUninstalled(ResStatus::USER);
		if (!soft.empty())
		    poolItem.status().setSoftUninstall(true);
	    } else {
		cerr << "Unknown system item " << name << endl;
		exit( 1 );
	    }

	} else if (node->equals ("distupgrade")) {

	    RESULT << "Doing distribution upgrade ..." << endl;
	    resolver->doUpgrade();

	    print_pool( resolver, MARKER );

	} else if (node->equals ("update")) {

	    RESULT << "Doing update ..." << endl;
	    resolver->doUpdate();
            print_solution (pool, instorder);
            doUpdate = true;

	} else if (node->equals ("instorder") || node->equals ("mediaorder")/*legacy*/) {

	    RESULT << "Calculating installation order ..." << endl;

	    instorder = true;

	} else if (node->equals ("whatprovides")) {

	    string kind_name = node->getProp ("kind");
	    string prov_name = node->getProp ("provides");

	    PoolItemSet poolItems;

	    cout << "poolItems providing '" << prov_name << "'" << endl;

	    poolItems = get_providing_poolItems (prov_name, kind_name);

	    if (poolItems.empty()) {
		cerr << "None found" << endl;
	    } else {
		for (PoolItemSet::const_iterator iter = poolItems.begin(); iter != poolItems.end(); ++iter) {
		    cout << (*iter) << endl;
		}
	    }
	} else if (node->equals ("addConflict")) {
            vector<string> names;
            str::split( node->getProp ("name"), back_inserter(names), "," );
            for (unsigned i=0; i < names.size(); ++i) {
                resolver->addExtraConflict(Capability (names[i], string2kind (node->getProp ("kind"))));
            }
	} else if (node->equals ("addRequire")) {
            vector<string> names;
            str::split( node->getProp ("name"), back_inserter(names), "," );
            for (unsigned i=0; i < names.size(); ++i) {
                resolver->addExtraRequire(Capability (names[i], string2kind (node->getProp ("kind"))));
            }
	} else if (node->equals ("upgradeRepo")) {
            vector<string> names;
            str::split( node->getProp ("name"), back_inserter(names), "," );
            for (unsigned i=0; i < names.size(); ++i) {
              Repository r = satpool.reposFind( names[i] );
              if ( ! r )
              {
                ERR << "upgradeRepo '" << r << "' not found." << endl;
                cerr << "upgradeRepo '" << r << "' not found." << endl;
		exit( 1 );
              }
              else
                resolver->addUpgradeRepo( r );
            }
	} else if (node->equals ("reportproblems")) {
            bool success;
            if (!solverQueue.empty())
                success = resolver->resolveQueue(solverQueue);
            else
                success = resolver->resolvePool();
	    if (success
                && node->getProp ("ignoreValidSolution").empty()) {
		RESULT << "No problems so far" << endl;
	    }
	    else {
		print_problems( resolver );
            }
	} else if (node->equals ("takesolution")) {
	    string problemNrStr = node->getProp ("problem");
	    string solutionNrStr = node->getProp ("solution");
	    assert (!problemNrStr.empty());
	    assert (!solutionNrStr.empty());
	    int problemNr = atoi (problemNrStr.c_str());
	    int solutionNr = atoi (solutionNrStr.c_str());
	    RESULT << "Taking solution: " << solutionNr << endl;
	    RESULT << "For problem:     " << problemNr << endl;
	    ResolverProblemList problems = resolver->problems ();

	    int problemCounter = -1;
	    int solutionCounter = -1;
	    // find problem
	    for (ResolverProblemList::iterator probIter = problems.begin();
		 probIter != problems.end(); ++probIter) {
		problemCounter++;
		if (problemCounter == problemNr) {
		    ResolverProblem problem = **probIter;
		    ProblemSolutionList solutionList = problem.solutions();
		    //find solution
		    for (ProblemSolutionList::iterator solIter = solutionList.begin();
			 solIter != solutionList.end(); ++solIter) {
			solutionCounter++;
			if (solutionCounter == solutionNr) {
			    ProblemSolution_Ptr solution = *solIter;
			    cout << "Taking solution: " << endl << *solution << endl;
			    cout << "For problem: " << endl << problem << endl;
			    ProblemSolutionList doList;
			    doList.push_back (solution);
			    resolver->applySolutions (doList);
			    break;
			}
		    }
		    break;
		}
	    }

	    if (problemCounter != problemNr) {
		RESULT << "Wrong problem number (0-" << problemCounter << ")" << endl;
	    } else if (solutionCounter != solutionNr) {
		RESULT << "Wrong solution number (0-" << solutionCounter << ")" <<endl;
	    } else {
		// resolve and check it again
                bool success;
                if (!solverQueue.empty())
                    success = resolver->resolveQueue(solverQueue);
                else
                    success = resolver->resolvePool();

		if (success) {
		    RESULT << "No problems so far" << endl;
		}
		else {
		    ResolverProblemList problems = resolver->problems ();
		    RESULT << problems.size() << " problems found:" << endl;
		    for (ResolverProblemList::iterator iter = problems.begin(); iter != problems.end(); ++iter) {
			cout << **iter << endl;
		    }
		}
	    }
	} else if (node->equals ("showpool")) {
	    string prefix = node->getProp ("prefix");
	    string all = node->getProp ("all");
            string get_licence = node->getProp ("getlicence");
            string verbose = node->getProp ("verbose");
	    print_pool( resolver, prefix, !all.empty(), get_licence, !verbose.empty() );

	}
	else if (node->equals ("showstatus")) {
	    resolver->resolvePool();
	    string prefix = node->getProp ("prefix");
	    string all = node->getProp ("all");
            string get_licence = node->getProp ("getlicence");
            string verbose = node->getProp ("verbose");
	    print_pool( resolver, "", true, "false", true );

	}
        else if (node->equals ("showselectable")){
                Selectable::Ptr item;
                string kind_name = node->getProp ("kind");
                if ( kind_name.empty() )
                    kind_name = "package";
                string name = node->getProp ("name");
                item = Selectable::get( ResKind(kind_name), name );
                if ( item )
                    dumpOn(cout, *item);
                else
                    cout << "Selectable '" << name << "' not valid" << endl;
        }
        else if (node->equals ("graphic")) {
#ifndef NOUI
            resolver->resolvePool();
            QApplication app(NULL, 0);
            QZyppSolverDialog *dialog = new QZyppSolverDialog(resolver);
            app.setMainWidget( dialog );
            dialog->setCaption("Solvertree");
            dialog->setMinimumSize ( 700, 700 );
            dialog->show();
            app.exec();
#else
            RESULT << "<graphic> is not supported by deptestomatic.noui" << endl;
#endif
        } else if (node->equals ("YOU") || node->equals ("PkgUI") ) {
#ifndef NOUI

            resolver->resolvePool();

            YUILog::setLogFileName( "/tmp/testUI.log" );
            YUILog::enableDebugLogging();

            YDialog *dialog = YUI::widgetFactory()->createMainDialog();
            YLayoutBox *vbox    = YUI::widgetFactory()->createVBox( dialog );

            long modeFlags = 0;
            if ( node->equals ("YOU") )
                modeFlags = YPkg_OnlineUpdateMode;

            YPackageSelector *pkgSelector = YUI::widgetFactory()->createPackageSelector( vbox,
                                                                                         modeFlags );
            dialog->setInitialSize();

            static YUI *myUI = YUI::ui();
            myUI->runPkgSelection( pkgSelector );

            dialog->destroy();
#else
            RESULT << "<YOU> or <PkgUI> are not supported by deptestomatic.noui" << endl;
#endif
	} else if (node->equals ("lock")) {
	    string source_alias = node->getProp ("channel");
	    string package_name = node->getProp ("name");
	    if (package_name.empty())
		package_name = node->getProp ("package");
	    string kind_name = node->getProp ("kind");
	    string version = node->getProp ("ver");
	    string release = node->getProp ("rel");
	    string architecture = node->getProp ("arch");

	    PoolItem poolItem;

	    poolItem = get_poolItem (source_alias, package_name, kind_name, version, release, architecture );
	    if (poolItem) {
		RESULT << "Locking " << package_name << " from channel " << source_alias << endl;
		poolItem.status().setLock (true, ResStatus::USER);
	    } else {
		cerr << "Unknown package " << source_alias << "::" << package_name << endl;
	    }
	} else if (node->equals ("validate")) {
	    string source_alias = node->getProp ("channel");
	    string package_name = node->getProp ("name");
	    if (package_name.empty())
		package_name = node->getProp ("package");
	    string kind_name = node->getProp ("kind");
	    string version = node->getProp ("ver");
	    string release = node->getProp ("rel");
	    string architecture = node->getProp ("arch");

            // Solving is needed
            if (!solverQueue.empty())
                resolver->resolveQueue(solverQueue);
            else
                resolver->resolvePool();

            if (!package_name.empty()) {
                PoolItem poolItem;
                poolItem = get_poolItem (source_alias, package_name, kind_name, version, release, architecture );
                if (poolItem) {
                    if (poolItem.isSatisfied())
                        RESULT <<  package_name << " from channel " << source_alias << " IS SATISFIED" << endl;
                    else
                        RESULT <<  package_name << " from channel " << source_alias << " IS NOT SATISFIED" << endl;
                } else {
                    cerr << "Unknown package " << source_alias << "::" << package_name << endl;
                }
            } else {
                // Checking all resolvables
                isSatisfied (kind_name);
            }
	} else if (node->equals ("availablelocales")) {
	    RESULT << "Available locales: ";
	    LocaleSet locales = pool.getAvailableLocales();
	    for (LocaleSet::const_iterator it = locales.begin(); it != locales.end(); ++it) {
		if (it != locales.begin()) std::cout << ", ";
		std::cout << it->code();
	    }
	    std::cout << endl;

	} else if (node->equals ("keep")) {
	    string kind_name = node->getProp ("kind");
	    string name = node->getProp ("name");
	    if (name.empty())
		name = node->getProp ("package");

	    string source_alias = node->getProp ("channel");
	    if (source_alias.empty())
		source_alias = "@System";

	    if (name.empty())
	    {
		cerr << "transact need 'name' parameter" << endl;
		return;
	    }

            PoolItem poolItem;

            poolItem = get_poolItem( source_alias, name, kind_name );

            if (poolItem) {
                // first: set anything
                if (source_alias == "@System") {
                    poolItem.status().setToBeUninstalled( ResStatus::USER );
                }
                else {
                    poolItem.status().setToBeInstalled( ResStatus::USER );
                }
                // second: keep old state
                poolItem.status().setTransact( false, ResStatus::USER );
            }
            else {
                cerr << "Unknown item " << source_alias << "::" << name << endl;
            }
	} else if (node->equals ("addQueueInstall")) {
	    string name = node->getProp ("name");
	    string soft = node->getProp ("soft");

	    if (name.empty())
	    {
		cerr << "addQueueInstall need 'name' parameter" << endl;
		return;
	    }
            zypp::solver::detail::SolverQueueItemInstall_Ptr install =
                new zypp::solver::detail::SolverQueueItemInstall(pool, name, (soft.empty() ? false : true));
            solverQueue.push_back (install);
	} else if (node->equals ("addQueueDelete")) {
	    string name = node->getProp ("name");
	    string soft = node->getProp ("soft");

	    if (name.empty())
	    {
		cerr << "addQueueDelete need 'name' parameter" << endl;
		return;
	    }
            zypp::solver::detail::SolverQueueItemDelete_Ptr del =
                new zypp::solver::detail::SolverQueueItemDelete(pool, name, (soft.empty() ? false : true));
            solverQueue.push_back (del);
	} else if (node->equals ("addQueueLock")) {
	    string soft = node->getProp ("soft");
	    string kind_name = node->getProp ("kind");
	    string name = node->getProp ("name");
	    if (name.empty())
		name = node->getProp ("package");

	    string source_alias = node->getProp ("channel");
	    if (source_alias.empty())
		source_alias = "@System";

	    if (name.empty())
	    {
		cerr << "transact need 'name' parameter" << endl;
		return;
	    }

            PoolItem poolItem = get_poolItem( source_alias, name, kind_name );
            if (poolItem) {
                zypp::solver::detail::SolverQueueItemLock_Ptr lock =
                    new zypp::solver::detail::SolverQueueItemLock(pool, poolItem, (soft.empty() ? false : true));
                solverQueue.push_back (lock);
            }
            else {
                cerr << "Unknown item " << source_alias << "::" << name << endl;
            }
	} else if (node->equals ("addQueueUpdate")) {
	    string kind_name = node->getProp ("kind");
	    string name = node->getProp ("name");
	    if (name.empty())
		name = node->getProp ("package");

	    string source_alias = node->getProp ("channel");
	    if (source_alias.empty())
		source_alias = "@System";

	    if (name.empty())
	    {
		cerr << "transact need 'name' parameter" << endl;
		return;
	    }

            PoolItem poolItem = get_poolItem( source_alias, name, kind_name );
            if (poolItem) {
                zypp::solver::detail::SolverQueueItemUpdate_Ptr lock =
                    new zypp::solver::detail::SolverQueueItemUpdate(pool, poolItem);
                solverQueue.push_back (lock);
            }
            else {
                cerr << "Unknown item " << source_alias << "::" << name << endl;
            }
	} else if (node->equals ("addQueueInstallOneOf")) {
            XmlNode_Ptr requestNode = node->children();
            zypp::solver::detail::PoolItemList poolItemList;
            while (requestNode) {
                if (requestNode->equals("item")) {
                string kind_name = requestNode->getProp ("kind");
                string name = requestNode->getProp ("name");
                if (name.empty())
                    name = requestNode->getProp ("package");

                string source_alias = requestNode->getProp ("channel");
                if (source_alias.empty())
                    source_alias = "@System";

                if (name.empty())
                {
                    cerr << "transact need 'name' parameter" << endl;
                } else {
                    PoolItem poolItem = get_poolItem( source_alias, name, kind_name );
                    if (poolItem) {
                        poolItemList.push_back(poolItem);
                    }
                    else {
                        cerr << "Unknown item " << source_alias << "::" << name << endl;
                    }
                }
                } else {
                    cerr << "addQueueInstallOneOf: cannot find flag 'item'" << endl;
                }
                requestNode = requestNode->next();
            }
            if (poolItemList.empty()) {
                cerr << "addQueueInstallOneOf has an empty list" << endl;
                return;
            } else {
                zypp::solver::detail::SolverQueueItemInstallOneOf_Ptr install =
                    new zypp::solver::detail::SolverQueueItemInstallOneOf(pool, poolItemList);
                solverQueue.push_back (install);
            }
	} else if (node->equals ("createTestcase")) {
	    string path = node->getProp ("path");
            if (path.empty())
                path = "./solverTestcase";
            Testcase testcase (path);
            testcase.createTestcase (*resolver);

	} else {
	    ERR << "Unknown tag '" << node->name() << "' in trial" << endl;
	    cerr << "Unknown tag '" << node->name() << "' in trial" << endl;
	}

	node = node->next();
    }

    bool success = false;

    if (!doUpdate) {
        if (verify) {
            success = resolver->verifySystem ();
	    print_pool( resolver );
        } else {
            if (!solverQueue.empty())
                success = resolver->resolveQueue(solverQueue);
            else
                success = resolver->resolvePool();
        }
        if (success) {
            print_solution (pool, instorder);
        } else {
            RESULT << "No valid solution found." << endl;
            print_problems( resolver );
        }
    }

}


//---------------------------------------------------------------------------------------------------------------------

static void
parse_xml_test (XmlNode_Ptr node)
{
    if (!node->equals("test")) {
	ZYPP_THROW (Exception("Node not 'test' in parse_xml_test():"+node->name()));
    }

    node = node->children();

    while (node) {
	if (node->type() == XML_ELEMENT_NODE) {
	    if (node->equals( "setup" )) {
		parse_xml_setup( node );
	    } else if (node->equals( "trial" )) {
                ResPool pool = God->pool();
		parse_xml_trial( node, pool );
	    } else {
		cerr << "Unknown tag '" << node->name() << "' in test" << endl;
	    }
	}

	node = node->next();
    }
}


static void
process_xml_test_file (const Pathname & filename)
{
    xmlDocPtr xml_doc;
    XmlNode_Ptr root;

    xml_doc = xmlParseFile (filename.c_str());
    if (xml_doc == NULL) {
	cerr << "Can't parse test file '" << filename << "'" << endl;
	exit (0);
    }

    root = new XmlNode (xmlDocGetRootElement (xml_doc));

    DBG << "Parsing file '" << filename << "'" << endl;

    parse_xml_test (root);

    xmlFreeDoc (xml_doc);
}


//---------------------------------------------------------------------------------------------------------------------

int
main (int argc, char *argv[])
{
  Pathname testFile( "solver-test.xml" );

  for ( --argc,++argv; argc; --argc,++argv )
  {
    if ( *argv == string("-h") || *argv == string("--help") )
    {
      dumpHelpOn( cerr );
      exit( 0 );
    }
    else if ( *argv == string("-v") )
    {
      zypp::base::LogControl::instance().logfile( "-" );
    }
    else
    {
      testFile =  *argv;
    }
  }

  if ( ! PathInfo(testFile).isFile() )
  {
    cerr << "Usage: deptestomatic [-h|-v] [solver-test.xml]" << endl;
    exit (0);
  }

  forceResolve = false;
  onlyRequires = false;
  allowVendorChange = false;
  ignorealreadyrecommended = false;

  solverQueue.clear();

  manager = makeRepoManager( "/tmp/myrepos" );

  try {
    God = zypp::getZYpp();
  }
  catch (const Exception & excpt_r ) {
    ZYPP_CAUGHT( excpt_r );
    cerr << "Can't aquire ZYpp lock" << endl;
    return 1;
  }

  KeyRingCallbacks keyring_callbacks;
  DigestCallbacks digest_callbacks;

  globalPath = testFile.dirname();

  DBG << "init_libzypp() done" << endl;
  process_xml_test_file ( testFile );

  return 0;
}


