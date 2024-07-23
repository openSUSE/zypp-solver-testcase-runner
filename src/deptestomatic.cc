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
#include <string_view>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <boost/algorithm/string.hpp>

#define ZYPP_USE_RESOLVER_INTERNALS

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
#include "zypp/base/StringV.h"
#include "zypp/base/LogTools.h"
#include "zypp/base/LogControl.h"
#include "zypp/base/Exception.h"
#include "zypp/base/Algorithm.h"
#include "zypp/base/SetTracker.h"

#include "zypp/ui/Selectable.h"

#include "zypp/media/MediaManager.h"

#include "zypp/Repository.h"

#include "zypp/solver/detail/Resolver.h"
#include "zypp/solver/detail/Testcase.h"
#include "zypp/solver/detail/SystemCheck.h"

#include "zypp/solver/detail/ItemCapKind.h"
#include "zypp/solver/detail/SolverQueueItem.h"
#include "zypp/solver/detail/SolverQueueItemDelete.h"
#include "zypp/solver/detail/SolverQueueItemInstall.h"
#include "zypp/solver/detail/SolverQueueItemInstallOneOf.h"
#include "zypp/solver/detail/SolverQueueItemLock.h"
#include "zypp/solver/detail/SolverQueueItemUpdate.h"

#include "zypp/target/modalias/Modalias.h"
#include <zypp/misc/LoadTestcase.h>

#include "KeyRingCallbacks.h"

using namespace std;
using namespace zypp;
using zypp::ui::Selectable;
using zypp::solver::detail::Testcase;
using zypp::ResolverProblemList;

//-----------------------------------------------------------------------------
typedef zypp::shared_ptr<zypp::solver::detail::ResolverInternal> MyResolver_Ptr;
//-----------------------------------------------------------------------------

static zypp::misc::testcase::TestcaseSetup const *testcaseSetup = nullptr;

static ZYpp::Ptr God;
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
printRes ( std::ostream & str, PoolItem pi )
{
  if ( testcaseSetup->show_mediaid() ) {
    str << "[" << pi.mediaNr() << "]";
  }
  auto st = pi.status();
  if ( st.isInstalled() ) {
    str << pi.asString();
    if ( st.isOrphaned() )
      str << " (orphaned)";
  } else {
    str << pi.asUserString();
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
	else if (str == "application") {
	    kind = ResKind::application;
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
List of known tags. See http://old-en.opensuse.org/Libzypp/Testsuite_solver for details: \n\
  addConflict addQueueDelete addQueueInstall addQueueInstallOneOf addQueueLock addQueueUpdate \n\
  addRequire allowVendorChange arch availablelocales channel createTestcase current distupgrade force-install \n\
  forceResolve hardwareInfo ignorealreadyrecommended install instorder keep locale lock mediaid \n\
  onlyRequires reportproblems setlicencebit showpool showstatus showselectable source subscribe system \n\
  systemCheck takesolution uninstall update upgradeRepo validate verify whatprovides" << endl;

// <reportproblems/>
// <takesolution problem="0" solution="0"/>
// <takesolution problem="0" solution="0"/>
// <instorder/>

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
struct KNEAOrder
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

struct Unique
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
		  std::ref(info) );
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
		  std::ref(info) );
    return info.itemset;
}

static void
print_solution ( const ResPool & pool, bool instorder, bool printKeept )
{
    PoolItemOrderSet install = uniquelyInstalled(pool);
    PoolItemOrderSet uninstall = uniquelyUninstalled(pool);

    RESULT << "Solution :" << endl;

    if ( printKeept ) {
      for ( const auto & pi : ResPool::instance().filter( []( PoolItem pi_r ){ return pi_r.status().staysInstalled(); } ) ) {
        RESULT << "keep "; printRes( cout, pi ); cout << endl;
      }
    }
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

#if ( WITH_PROOF )
extern "C"
{
  #include "Proof.h"
}
#endif

static void print_problems( MyResolver_Ptr resolver )
{
  ResolverProblemList problems = resolver->problems();
  problems.sort( compare_problems() );
  RESULT << problems.size() << " problems found:" << endl;
  for ( ResolverProblemList::iterator iter = problems.begin(); iter != problems.end(); ++iter )
  {
    ResolverProblem problem = **iter;
    RESULT << "Problem:" << endl;
    RESULT << "====================================" << endl;
    RESULT << problem.description() << endl;
    RESULT << "------------------------------------" << endl;
    for ( const std::string &prop : problem.completeProblemInfo() )
      RESULT << "  -"  << prop << endl;
    RESULT << "------------------------------------" << endl;
    RESULT << problem.details() << endl;
    ProblemSolutionList solutions = problem.solutions();
    for ( ProblemSolutionList::const_iterator iter = solutions.begin(); iter != solutions.end(); ++iter )
    {
      ProblemSolution solution = **iter;
      RESULT << "------------------------------------" << endl;
      RESULT << "   Solution:" << endl;
      RESULT << "      " << solution.description() << endl;
      RESULT << "      " << solution.details() << endl;
    }
    RESULT << "------------------------------------" << endl;
  }

#if ( WITH_PROOF )
  RESULT << "Proof:" << endl;
  RESULT << "====================================" << endl;
  RESULT << doProof( resolver->get() ) << endl;
  RESULT << "====================================" << endl;
#endif
}

//---------------------------------------------------------------------------------------------------------------------

struct IsStatisfied
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
		      std::ref(info) );
    } else {
        Resolvable::Kind kind = string2kind (kind_name);
	IsStatisfied info (kind);

	invokeOnEach( God->pool().byKindBegin( kind ),
		      God->pool().byKindEnd( kind ),
		      std::ref(info) );
    }
}


//---------------------------------------------------------------------------------------------------------------------
struct FindPackage
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

    void _remember( PoolItem p )
    {
      poolItem = p;
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
	    if (p->edition().match( edition ) != 0)
	      return true;
	}

	if (!poolItem							// none yet
	    || (poolItem->arch().compare( p->arch() ) < 0)		// new has better arch
	    || (poolItem->edition().compare( p->edition() ) < 0))	// new has better edition
	{
	    _remember( p );
	}
	return true;
    }
};

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
		      std::ref(info) );

	poolItem = info.poolItem;
        if (!poolItem) {
            // try to find the resolvable over all channel. This is useful for e.g. languages
            invokeOnEach( God->pool().byIdentBegin( kind,package_name ),
                          God->pool().byIdentEnd( kind,package_name ),
                          std::ref(info) );
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

struct SortItem
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
print_pool( MyResolver_Ptr resolver, const string & prefix = "", bool show_all = true, string show_licence = "false", bool verbose = false )
{
    SortItem info( show_all );
    cout << "Current pool: " <<  God->pool().size() << endl;
    invokeOnEach( God->pool().begin( ),
		  God->pool().end ( ),
		  std::ref(info) );

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
                cout << "         " << iter->item() << " " << iter->cap() << " " << iter->capKind() << " " << iter->initialInstallation() << endl;
            }
            for (zypp::solver::detail::ItemCapKindList::const_iterator iter = select.begin(); iter != select.end(); ++iter) {
                if (iter == select.begin()) {
                    cout << prefix << ++count << ": ";
                    cout << "   will select:" << endl;
                }
                cout << prefix << ++count << ": ";
                cout << "         " << iter->item() << " " << iter->cap() << " " << iter->capKind() << " " << iter->initialInstallation() << endl;
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
		  std::ref(info) );

    for (ItemMap::const_iterator it = info.sorted.begin(); it != info.sorted.end(); ++it) {
		it->second.status().setLicenceConfirmed();
	}

	return;
}

static bool done_setup = false;

static void execute_setup( const zypp::misc::testcase::LoadTestcase &testcase )
{
  if ( done_setup ) {
    cerr << "Calling execute_setup multiple times is not allowed!" << endl;
    exit (0);
  }
  done_setup = true;

  const auto &setup = testcase.setupInfo();

  auto tempRepoManager = makeRepoManager( "/tmp/myrepos" );
  if ( !setup.applySetup( tempRepoManager ) ) {
    std::cerr << "Failed to apply setup from testcase" << std::endl;
    exit(1);
  }

  RESULT << "Read and applied setup from testcase" << std::endl;

  for ( const auto &fi : setup.forceInstallTasks() ) {
    PoolItem poolItem;

    poolItem = get_poolItem( fi.channel(), fi.package(), fi.kind() );
    if ( poolItem )
    {
      RESULT << "Force-installing " << fi.package() << " from channel " << fi.channel() << endl;;

      poolItem.status().setToBeInstalled( ResStatus::USER );
    }
    else
    {
      cerr << "Unknown package " << fi.channel() << "::" << fi.package() << endl;
    }
  }

  if( setup.set_licence() )
    set_licence_Pool();
}


//-------------
static void execute_trial ( const zypp::misc::testcase::TestcaseSetup &setup, const zypp::misc::testcase::TestcaseTrial &trial, ResPool & pool)
{
    static bool first_trial = true;

    bool verify = false;
    bool doUpdate = false;
    bool instorder = false;
    bool printKeept = false;

    DBG << "execute_trial()" << endl;

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

    MyResolver_Ptr resolver( new zypp::solver::detail::ResolverInternal( pool ) );
    // set solver flags:
    resolver->setFocus				( setup.resolverFocus() == ResolverFocus::Default ? ResolverFocus::Job : setup.resolverFocus() ); // 'Default' would be our ZYppConf value
    resolver->setIgnoreAlreadyRecommended	( setup.ignorealreadyrecommended() );
    resolver->setOnlyRequires			( setup.onlyRequires() );
    resolver->setForceResolve			( setup.forceResolve() );

    resolver->setCleandepsOnRemove		( setup.cleandepsOnRemove() );

    resolver->setAllowDowngrade			( setup.allowDowngrade() );
    resolver->setAllowNameChange		( setup.allowNameChange() );
    resolver->setAllowArchChange		( setup.allowArchChange() );
    resolver->setAllowVendorChange		( setup.allowVendorChange() );

    resolver->dupSetAllowDowngrade		( setup.dupAllowDowngrade() );
    resolver->dupSetAllowNameChange		( setup.dupAllowNameChange() );
    resolver->dupSetAllowArchChange		( setup.dupAllowArchChange() );
    resolver->dupSetAllowVendorChange		( setup.dupAllowVendorChange() );

    // RequestedLocales
    {
      base::SetTracker<LocaleSet> localesTracker = setup.localesTracker();
      localesTracker.removed().insert( localesTracker.current().begin(), localesTracker.current().end() );
      zypp::sat::Pool::instance().initRequestedLocales( localesTracker.removed() );
      RESULT << "initRequestedLocales " << localesTracker.removed() << endl;

      localesTracker.added().insert( localesTracker.current().begin(), localesTracker.current().end() );
      zypp::sat::Pool::instance().setRequestedLocales( localesTracker.added() );
      RESULT << "setRequestedLocales " << localesTracker.added() << endl;
    }

    // Vendor
    for ( const auto & vlist : setup.vendorLists() ) {
      VendorAttr::noTargetInstance().addVendorList( vlist );
    }

    // AutoInstalled
    zypp::sat::Pool::instance().setAutoInstalled( setup.autoinstalled() );

    // set modaliases
    RESULT << "Load " << setup.modaliasList().size() << " modaliases." << endl;
    target::Modalias::instance().modaliasList( setup.modaliasList() );

    // set multiversion packages
    RESULT << "Load " << setup.multiversionSpec().size() << " multiversion specs." << endl;
    ZConfig::instance().multiversionSpec( setup.multiversionSpec() );

    for ( const auto &node: trial.nodes() ) {
      if ( node.name() == "note" ) {
        cout << "NOTE: " << node.value() << endl;
      } else if ( node.name() == "vverify" ) {	// see libzypp@ca9bcf16, testcase writer mixed "update" and "verify"
        verify = true;
      } else if ( node.name() == "current" ) {
        // unsupported
        // string source_alias = node.getProp("channel");
      } else if ( node.name() == "subscribe" ) {
        // unsupported
        // string source_alias = node.getProp("channel");
      } else if ( str::hasPrefix( node.name(), "install" ) && ( node.name().size() == 7 || node.name()[7] == ' ' ) ) {
        if ( node.name().size() > 7 ) {
          // job: install patch:openSUSE-SLE-15.4-2022-2831
          // as convenience for editing testcases
          std::string spec { str::trim( node.name().substr(8) ) };
          Capability cap { Capability::guessPackageSpec( spec ) };
          cerr << "Guessed " << cap << std::endl
              << "   from " << spec << std::endl;
          if ( cap )
            resolver->addExtraRequire( cap );
          else
            cerr << "Unknown job item " << node.name() << endl;
          continue;
        }
        // else:
        string source_alias = node.getProp ("channel");
        string name = node.getProp ("name");
        if (name.empty())
          name = node.getProp ("package");
        string kind_name = node.getProp ("kind");
        string soft = node.getProp ("soft");
        string version = node.getProp ("version");
        string release = node.getProp ("release");
        string architecture = node.getProp ("arch");

        PoolItem poolItem;

        poolItem = get_poolItem( source_alias, name, kind_name, version, release, architecture );
        if (poolItem) {
          RESULT << "Installing " << poolItem
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
      } else if ( node.name() == "uninstall") {

        string name = node.getProp ("name");
        if (name.empty())
          name = node.getProp ("package");
        string kind_name = node.getProp ("kind");
        string soft = node.getProp ("soft");
        string version = node.getProp ("ver");
        string release = node.getProp ("rel");
        string architecture = node.getProp ("arch");

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
      } else if ( node.name() == "distupgrade" ) {

        RESULT << "Doing distribution upgrade ..." << endl;
        resolver->doUpgrade();
        printKeept = true; // in print solution
        print_pool( resolver, MARKER );
      } else if ( node.name() == "update"|| node.name() == "verify" ) {	// see libzypp@ca9bcf16, testcase writer mixed "update" and "verify"

        RESULT << "Doing update ..." << endl;
        resolver->resolvePool();
        resolver->doUpdate();
        print_solution (pool, instorder, printKeept );
        doUpdate = true;
      } else if ( node.name() == "instorder" || node.name() == "mediaorder" /*legacy*/ ) {

        RESULT << "Calculating installation order ..." << endl;
        instorder = true;
      } else if ( node.name() == "whatprovides" ) {

        string kind_name = node.getProp ("kind");
        string prov_name = node.getProp ("provides");

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
      } else if ( node.name() == "addConflict" ) {
        vector<string> names;
        str::split( node.getProp ("name"), back_inserter(names), "," );
        const auto &kind = string2kind (node.getProp ("kind"));
        for (unsigned i=0; i < names.size(); ++i) {
          resolver->addExtraConflict(Capability (names[i], kind));
        }
      } else if ( node.name() == "addRequire" ) {
        vector<string> names;
        str::split( node.getProp ("name"), back_inserter(names), "," );
        const auto &kind = string2kind (node.getProp ("kind"));
        for (unsigned i=0; i < names.size(); ++i) {
          resolver->addExtraRequire(Capability (names[i], kind ));
        }
      } else if ( node.name() == "upgradeRepo" ) {
        vector<string> names;
        str::split( node.getProp ("name"), back_inserter(names), "," );
	if ( names.empty() ) {
	  ERR << "upgradeRepo  'name' empty!" << endl;
	  cerr << "upgradeRepo 'name' empty!" << endl;
	  exit( 1 );
	}
        for (unsigned i=0; i < names.size(); ++i) {
          Repository r = satpool.reposFind( names[i] );
          if ( ! r )
          {
            ERR << "upgradeRepo '" << names[i] << "' not found. (been empty?)" << endl;
            cerr << "upgradeRepo '" << names[i] << "' not found. (been empty?)" << endl;
            exit( 1 );
          }
          else
            resolver->addUpgradeRepo( r );
        }
      } else if ( node.name() == "reportproblems" ) {
        bool success;
        if (!solverQueue.empty())
          success = resolver->resolveQueue(solverQueue);
        else
          success = resolver->resolvePool();
        if (success
            && node.getProp ("ignoreValidSolution").empty()) {
          RESULT << "No problems so far" << endl;
        }
        else {
          print_problems( resolver );
        }
      } else if ( node.name() == "takesolution" ) {
        string problemNrStr = node.getProp ("problem");
        string solutionNrStr = node.getProp ("solution");
        assert (!problemNrStr.empty());
        assert (!solutionNrStr.empty());
        int problemNr = atoi (problemNrStr.c_str());
        int solutionNr = atoi (solutionNrStr.c_str());
        RESULT << "Want solution: " << solutionNr << endl;
        RESULT << "For problem:   " << problemNr << endl;
        ResolverProblemList problems = resolver->problems ();
        RESULT << "*T*(" << resolver->problems().size() << ")" << endl;

        int problemCounter = -1;
        int solutionCounter = -1;
        // find problem
        for (ResolverProblemList::iterator probIter = problems.begin(); probIter != problems.end(); ++probIter) {
          problemCounter++;
          RESULT << "*P*(" << problemCounter << "|" << solutionCounter << ")" << endl;
          if (problemCounter == problemNr) {
            ResolverProblem problem = **probIter;
            ProblemSolutionList solutionList = problem.solutions();
            //find solution
            for (ProblemSolutionList::iterator solIter = solutionList.begin();
                 solIter != solutionList.end(); ++solIter) {
              solutionCounter++;
              RESULT << "*S*(" << problemCounter << "|" << solutionCounter << ")" << endl;
              if (solutionCounter == solutionNr) {
                ProblemSolution_Ptr solution = *solIter;
                RESULT << "Taking solution: " << endl << *solution << endl;
                RESULT << "For problem: " << endl << problem << endl;
                ProblemSolutionList doList;
                doList.push_back (solution);
                resolver->applySolutions (doList);
                break;
              }
            }
            break;
          }
        }

        if ( problemCounter != problemNr || solutionCounter != solutionNr ) {
          RESULT << "Did not find problem=" << problemCounter << ", solution="  << solutionCounter << endl;
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
      } else if ( node.name() == "showpool" ) {
        resolver->resolvePool();
        string prefix = node.getProp ("prefix");
        string all = node.getProp ("all");
        string get_licence = node.getProp ("getlicence");
        string verbose = node.getProp ("verbose");
        print_pool( resolver, prefix, !all.empty(), get_licence, !verbose.empty() );
        printKeept = true; // in print solution
      } else if ( node.name() == "showstatus" ) {
        resolver->resolvePool();
        string prefix = node.getProp ("prefix");
        string all = node.getProp ("all");
        string get_licence = node.getProp ("getlicence");
        string verbose = node.getProp ("verbose");
        print_pool( resolver, "", true, "false", true );

      } else if (node.name() == "showselectable" ){
        Selectable::Ptr item;
        string kind_name = node.getProp ("kind");
        if ( kind_name.empty() )
          kind_name = "package";
        string name = node.getProp ("name");
        item = Selectable::get( ResKind(kind_name), name );
        if ( item )
          dumpOn(cout, *item);
        else
          cout << "Selectable '" << name << "' not valid" << endl;
      } else if ( node.name() == "graphic" ) {
        RESULT << "<graphic> is no longer supported by deptestomatic" << endl;
      } else if ( node.name() == ("YOU") || node.name() == ("PkgUI") ) {
        RESULT << "<YOU> or <PkgUI> are no longer supported by deptestomatic" << endl;
      } else if ( node.name() == "lock" ) {
        string source_alias = node.getProp ("channel");
        string package_name = node.getProp ("name");
        if (package_name.empty())
          package_name = node.getProp ("package");
        string kind_name = node.getProp ("kind");
        string version = node.getProp ("ver");
        string release = node.getProp ("rel");
        string architecture = node.getProp ("arch");

        if ( version.empty() )
        {
          if ( kind_name.empty() )
            kind_name = "package";
          Selectable::Ptr item = Selectable::get( ResKind(kind_name), package_name );
          if ( item )
          {
            item->setStatus( item->hasInstalledObj() ? ui::S_Protected : ui::S_Taboo );
	    item->setStatus( ui::S_Taboo );
          }
          else
          {
            cerr << "Unknown Selectable " << kind_name << ":" << package_name << endl;
          }
        }
        else
        {
          PoolItem poolItem;
          poolItem = get_poolItem (source_alias, package_name, kind_name, version, release, architecture );
          if (poolItem) {
            RESULT << "Locking " << package_name << " from channel " << source_alias << poolItem << endl;
            poolItem.status().setLock (true, ResStatus::USER);
          } else {
            cerr << "Unknown package " << source_alias << "::" << package_name << endl;
          }
        }
      } else if ( node.name() == "validate" ) {
        string source_alias = node.getProp ("channel");
        string package_name = node.getProp ("name");
        if (package_name.empty())
          package_name = node.getProp ("package");
        string kind_name = node.getProp ("kind");
        string version = node.getProp ("ver");
        string release = node.getProp ("rel");
        string architecture = node.getProp ("arch");

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
      } else if ( node.name() == "availablelocales" ) {
        RESULT << "Available locales: ";
        LocaleSet locales = pool.getAvailableLocales();
        for (LocaleSet::const_iterator it = locales.begin(); it != locales.end(); ++it) {
          if (it != locales.begin()) std::cout << ", ";
          std::cout << it->code();
        }
        std::cout << endl;

      } else if ( node.name() == "keep" ) {
        string kind_name = node.getProp ("kind");
        string name = node.getProp ("name");
        if (name.empty())
          name = node.getProp ("package");

        string source_alias = node.getProp ("channel");
        if (source_alias.empty())
          source_alias = "@System";

        if (name.empty())
        {
          cerr << "transact need 'name' parameter" << endl;
          return;
        }

        PoolItem poolItem;

        poolItem = get_poolItem( source_alias, name, kind_name, node.getProp ("version"), node.getProp ("release") );

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
      } else if ( node.name() == "addQueueInstall" ) {
        string name = node.getProp ("name");
        string soft = node.getProp ("soft");

        if (name.empty()) {
          cerr << "addQueueInstall need 'name' parameter" << endl;
          return;
        }
        zypp::solver::detail::SolverQueueItemInstall_Ptr install =
            new zypp::solver::detail::SolverQueueItemInstall(pool, name, (soft.empty() ? false : true));
        solverQueue.push_back (install);
      } else if ( node.name() == "addQueueDelete" ) {
        string name = node.getProp ("name");
        string soft = node.getProp ("soft");

        if (name.empty()) {
          cerr << "addQueueDelete need 'name' parameter" << endl;
          return;
        }
        zypp::solver::detail::SolverQueueItemDelete_Ptr del =
            new zypp::solver::detail::SolverQueueItemDelete(pool, name, (soft.empty() ? false : true));
        solverQueue.push_back (del);
      } else if ( node.name() == "addQueueLock" ) {
        string soft = node.getProp ("soft");
        string kind_name = node.getProp ("kind");
        string name = node.getProp ("name");
        if (name.empty())
          name = node.getProp ("package");

        string source_alias = node.getProp ("channel");
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
      } else if ( node.name() == "addQueueUpdate" ) {
        string kind_name = node.getProp ("kind");
        string name = node.getProp ("name");
        if (name.empty())
          name = node.getProp ("package");

        string source_alias = node.getProp ("channel");
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
      } else if ( node.name() == "addQueueInstallOneOf" ) {
        zypp::solver::detail::PoolItemList poolItemList;
        for ( const auto &child : node.children() ) {
          if ( child->name() == "item" ) {
            string kind_name = child->getProp ("kind");
            string name = child->getProp ("name");
            if (name.empty())
              name = child->getProp ("package");

            string source_alias = child->getProp ("channel");
            if (source_alias.empty())
              source_alias = "@System";

            if (name.empty()) {
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
        }
        if (poolItemList.empty()) {
          cerr << "addQueueInstallOneOf has an empty list" << endl;
          return;
        } else {
          zypp::solver::detail::SolverQueueItemInstallOneOf_Ptr install =
              new zypp::solver::detail::SolverQueueItemInstallOneOf(pool, poolItemList);
          solverQueue.push_back (install);
        }
      } else if ( node.name() == "createTestcase" ) {
        string path = node.getProp ("path");
        if (path.empty())
          path = "./solverTestcase";
        Testcase testcase (path);
        testcase.createTestcase (*resolver);
      } else {
        ERR << "Unknown tag '" << node.name() << "' in trial" << endl;
        cerr << "Unknown tag '" << node.name() << "' in trial" << endl;
      }
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
        print_solution (pool, instorder, printKeept);
      } else {
        RESULT << "No valid solution found." << endl;
        print_problems( resolver );
      }
    }
}
//---------------------------------------------------------------------------------------------------------------------

// libsolv: order.c
enum class CycleOrderBits {
  BROKEN      = (1<<0),
  CON         = (1<<1),
  REQ_P       = (1<<2),
  PREREQ_P    = (1<<3),
  SUG         = (1<<4),
  REC         = (1<<5),
  REQ         = (1<<6),
  PREREQ      = (1<<7),
  CYCLETAIL   = (1<<16),
  CYCLEHEAD   = (1<<17),
};
ZYPP_DECLARE_FLAGS( CycleOrder, CycleOrderBits );
ZYPP_DECLARE_OPERATORS_FOR_FLAGS( CycleOrder );

inline std::ostream & operator<<( std::ostream & str, CycleOrder obj )
{
#define OUTS(V) { CycleOrderBits::V, #V }
  return str << stringify( obj, {
    OUTS( BROKEN ),
    OUTS( CON ),
    OUTS( REQ_P ),
    OUTS( PREREQ_P ),
    OUTS( SUG ),
    OUTS( REC ),
    OUTS( REQ ),
    OUTS( PREREQ ),
    OUTS( CYCLETAIL ),
    OUTS( CYCLEHEAD ),
  } );
#undef OUTS
}


inline bool addhexc2num( CycleOrder::Integral & num_r, char ch_r )
{
  if ( '0' <= ch_r && ch_r <= '9' )
    ch_r -= '0';
  else if ( 'a' <= ch_r && ch_r <= 'f' )
    ch_r -= 'a'+10;
  else if ( 'A' <= ch_r && ch_r <= 'F' )
    ch_r -= 'A'+10;
  else
    return false;
  num_r <<= 4;
  num_r += ch_r;
  return true;
}

inline CycleOrder cycleOrder( std::string_view input_r )
{
  CycleOrder::Integral ret = 0;
  for ( char c : input_r ) {
    if ( ! addhexc2num( ret, c ) )
      break;
  }
  return CycleOrder(ret);
}

// deptestomatic  'cycle: --> wicked-service-0.6.60-2.13.2.x86_64 ##45##> wicked-0.6.60-2.13.2.x86_64 --48--> '
int cycleinfo( std::string_view cycle_r )
{
  std::vector<std::string_view> words;
  strv::split( cycle_r , [&words]( std::string_view word ) { words.push_back( word ); } );

  if ( words.size() <= 2 ||  words[0] != "cycle:" || words[1] != "-->" ) {
    cerr << "Parse cycle OOPS: " << words << endl;
    return 1;
  }

  size_t sep = 1;	// get the brokenup '##%d##>'
  for ( ; sep < words.size() && words[sep][0] != '#'; sep+=2 )
  {;}
  if ( sep >= words.size() ) {
    cerr << "Parse cycle OOPS: no ##->##" << endl;
    return 2;
  }

  str::Format fmt { "%-40s %-10s %s" };
  auto writeln = [&fmt]( std::string_view nam, std::string_view req ) {
    cout << fmt % nam % req % cycleOrder( req.substr(2) ) << endl;
  };

  for ( size_t i = sep+1; i < words.size(); i+=2 )
    writeln( words[i], words[i+1] );
  for ( size_t i = 2; i < sep; i+=2 )
    writeln( words[i], words[i+1] );

  return 0;
}

//---------------------------------------------------------------------------------------------------------------------

int
main (int argc, char *argv[])
{
  Pathname testPath(".");

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
    else if ( strncmp( *argv, "cycle:", 6 ) == 0 )
    {
      return cycleinfo( *argv );
    }
    else
    {
      testPath =  *argv;
    }
  }

  if ( ! PathInfo(testPath).isDir() )
  {
    cerr << "Usage: deptestomatic [-h|-v] [testdir]" << endl;
    exit (0);
  }

  solverQueue.clear();

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
  VendorAttr::noTargetInstance() = VendorAttr();	// unset any local system settings
  DBG << "init_libzypp() done" << endl;

  std::string error;
  zypp::misc::testcase::LoadTestcase loader;
  if ( !loader.loadTestcaseAt( testPath, &error) ) {
    cerr << "Loading the testcase failed with error: " << error << std::endl;
    return 1;
  }

  testcaseSetup = &loader.setupInfo();
  execute_setup( loader );

  for ( const auto &trial : loader.trialInfo() ) {
    ResPool pool = God->pool();
    execute_trial( loader.setupInfo(), trial, pool );
  }

  return 0;
}


