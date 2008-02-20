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
#if 0
#include <qapplication.h>
#include <qdialog.h>
#include <qfiledialog.h>     
#include <qpushbutton.h>
#include <qfont.h>
#endif
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
#include "zypp/ResTraits.h"
#include "zypp/ResPool.h"
#include "zypp/PoolItem.h"
#include "zypp/Capability.h"
#include "zypp/Capabilities.h"
#include "zypp/ResolverProblem.h"
#include "zypp/ProblemSolution.h"
#include "zypp/RepoManager.h"
#include "zypp/ResPool.h"
#include "zypp/ResFilters.h"
#include "zypp/CapFilters.h"
#include "zypp/ResolverProblem.h"
#include "zypp/Locale.h"
#include "zypp/ZConfig.h"

#include "zypp/base/String.h"
#include "zypp/base/Logger.h"
#include "zypp/base/LogControl.h"
#include "zypp/base/Exception.h"
#include "zypp/base/Algorithm.h"

#include "zypp/media/MediaManager.h"

#include "zypp/pool/GetResolvablesToInsDel.h"

#include "zypp/sat/Repo.h"

#include "zypp/solver/detail/Resolver.h"
#include "zypp/solver/detail/InstallOrder.h"
#include "zypp/solver/detail/Testcase.h"

#include "KeyRingCallbacks.h"
#include "XmlNode.h"
#include "satsolver/repo_helix.h"

using namespace std;
using namespace zypp;
using zypp::solver::detail::InstallOrder;
using zypp::solver::detail::Testcase;
using zypp::ResolverProblemList;
using zypp::solver::detail::XmlNode;
using zypp::solver::detail::XmlNode_Ptr;

//-----------------------------------------------------------------------------

static bool show_mediaid = false;
static string globalPath;
static LocaleSet locales;

static ZYpp::Ptr God;
static RepoManager manager;
static bool forceResolve;

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
    if (r->kind() != ResTraits<zypp::Package>::kind)
	str << r->kind() << ':';
    str  << r->name() << '-' << r->edition();
    if (r->arch() != "") {
	str << '.' << r->arch();
    }
    sat::Repo s  = r->satSolvable().repo();    
    if (s) {
	string alias = s.info().alias();
	if (!alias.empty()
	    && alias != "@system")
	{
	    str << '[' << alias << ']';
	}
//	str << '[' << s << ']';
    }
    return str;
}

//---------------------------------------------------------------------------

Resolvable::Kind
string2kind (const std::string & str)
{
    Resolvable::Kind kind = ResTraits<zypp::Package>::kind;
    if (!str.empty()) {
	if (str == "package") {
	    // empty
	}
	else if (str == "patch") {
	    kind = ResTraits<zypp::Patch>::kind;
	}
	else if (str == "atom") {
	    kind = ResTraits<zypp::Atom>::kind;
	}
	else if (str == "pattern") {
	    kind = ResTraits<zypp::Pattern>::kind;
	}
	else if (str == "script") {
	    kind = ResTraits<zypp::Script>::kind;
	}
	else if (str == "message") {
	    kind = ResTraits<zypp::Message>::kind;
	}
	else if (str == "product") {
	    kind = ResTraits<zypp::Product>::kind;
	}
	else {
	    cerr << "get_poolItem unknown kind '" << str << "'" << endl;
	}
    }
    return kind;
}

//---------------------------------------------------------------------------

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
	    res = lhs->satSolvable().repo().info().alias().compare( rhs->satSolvable().repo().info().alias() );
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
print_solution ( const ResPool & pool, bool instorder, bool mediaorder)
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
	solver::detail::PoolItemSet dummy;

	solver::detail::PoolItemSet insset( install.begin(), install.end() );

	int counter = 1;
	InstallOrder order( pool, insset, dummy );		 // sort according to prereq
	order.init();
	for ( solver::detail::PoolItemList items = order.computeNextSet(); ! items.empty(); items = order.computeNextSet() )
	{
	    RESULT << endl;
	    RESULT << counter << ". set with " << items.size() << " resolvables" << endl;
	    PoolItemOrderSet orderedset;

	    for ( solver::detail::PoolItemList::iterator iter = items.begin(); iter != items.end(); ++iter )
	    {
		orderedset.insert( *iter );
	    }
	    for (PoolItemOrderSet::const_iterator iter = orderedset.begin(); iter != orderedset.end(); iter++) {
		RESULT; printRes( cout,  *iter); cout << endl;
	    }
	    counter++;
	    order.setInstalled( items );
	}


	cout << "- - - - - - - - - -" << endl;
    }

    if (mediaorder) {
	cout << endl;
	RESULT << "Media Order:" << endl << endl;

	Target::PoolItemList dellist;
	Target::PoolItemList inslist;
	Target::PoolItemList srclist;
        pool::GetResolvablesToInsDel collect( pool );
        dellist.swap(collect._toDelete);
        inslist.swap(collect._toInstall);
        srclist.swap(collect._toSrcinstall);

	int count = 0;
	for (Target::PoolItemList::const_iterator iter = dellist.begin(); iter != dellist.end(); iter++) {
	    cout << "DEL " << ++count << ".: "; printRes (cout, (*iter)); cout << endl;
	}
	count = 0;
	for (Target::PoolItemList::const_iterator iter = inslist.begin(); iter != inslist.end(); iter++) {
	    cout << "INS " << ++count << ".:"; printRes (cout, (*iter)); cout << endl;
	}
	cout << "- - - - - - - - - -" << endl;
    }

    cout.flush();

    return;
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
	    || (!poolItem.status().isUndetermined()
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
print_pool( solver::detail::Resolver_Ptr resolver, const string & prefix = "", bool show_all = true, string show_licence = "false", bool verbose = false )
{
    SortItem info( show_all );
    cout << "Current pool:" << endl;
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
#if 0 // not supported currently
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
#endif
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

static int
load_source (const string & alias, const string & filename, const string & type, bool system_packages)
{
    Pathname pathname = globalPath + filename;
    MIL << "'" << pathname << "'" << endl;
    int count = 0;

    Repository repo;

    if (type == "url") {
	try {
          cout << "Load from Url '" << filename << "'" << endl;
          MIL << "Load from Url '" << filename << "'" << endl;
          
          RepoInfo nrepo;
          nrepo
              .setAlias      ( alias )
              .setName       ( "namen sind schall und rauch" )
              .setEnabled    ( true )
              .setAutorefresh( false )
              .addBaseUrl    ( Url(filename) );

          //manager.addRepository( nrepo );
          manager.refreshMetadata( nrepo );
          manager.buildCache( nrepo );

          manager.loadFromCache( nrepo );
	}
	catch ( Exception & excpt_r ) {
	    ZYPP_CAUGHT (excpt_r);
	    cout << "Couldn't load packages from Url '" << filename << "'" << endl;
	    return -1;
	}
    }
    else {
	try {
          cout << "Load from File '" << pathname << "'" << endl;
          MIL << "Load from File '" << pathname << "'" << endl;
          zypp::sat::Repo satRepo;

          if (alias == "@System") {
              satRepo = zypp::sat::Pool::instance().systemRepo();
          } else {
              satRepo = zypp::sat::Pool::instance().reposInsert(alias);              
          }
          
          RepoInfo nrepo;
          nrepo
              .setAlias      ( alias )
              .setName       ( "namen sind schall und rauch" )
              .setEnabled    ( true )
              .setAutorefresh( false )
              .addBaseUrl    ( pathname.asUrl() );
          
          satRepo.setInfo (nrepo);
          _Repo *intSatRepo = satRepo.get();
          string command;
          
          if (str::endsWith(filename, ".gz")) {
              command = "zcat " + filename;
          } else {
              command = "cat " + filename;              
          }
          FILE *fpHelix = popen( command.c_str(), "r" );
          if (!fpHelix)
          {
              cout << "Couldn't load packages from XML file '" << filename << "'" << endl;  
              return -1;
          }
          repo_add_helix(intSatRepo, fpHelix);
          count = satRepo.solvablesSize();
          cout << "Loaded " << satRepo.solvablesSize() << " resolvables from " << (filename.empty()?pathname.asString():filename) << "." << endl;        
          pclose( fpHelix );	          
	}
	catch ( Exception & excpt_r ) {
	    ZYPP_CAUGHT (excpt_r);
	    cout << "Couldn't load packages from XML file '" << filename << "'" << endl;
	    return -1;
	}
    }

    if(set_licence)
      set_licence_Pool();

    return count;
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

	} else if (node->equals ("system")) {

	    string file = node->getProp ("file");
	    if (load_source ("@System", file, "helix", true) <= 0) {
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
	    if (load_source (name, file, type, false) <= 0) {
		cerr << "Can't setup 'channel'" << endl;
		exit( 1 );
	    }

	} else if (node->equals ("source")) {

	    string url = node->getProp ("url");
	    string alias = node->getProp ("name");
	    if (load_source( alias, url, "url", false ) <= 0) {
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

		poolItem.status().setStatus(ResStatus::installed);
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
	}else if (node->equals("setlicencebit")){
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
    bool instorder = false;
    bool mediaorder = false;
    bool distupgrade = false;

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

    solver::detail::Resolver_Ptr resolver = new solver::detail::Resolver( pool );
    resolver->setTesting ( true );			// continue despite missing target
    resolver->setForceResolve( forceResolve );

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
		    << ((poolItem->kind() != ResTraits<zypp::Package>::kind) ? (poolItem->kind().asString() + ":") : "")
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

	    poolItem = get_poolItem ("@system", name, kind_name, version, release, architecture );
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

	    distupgrade = true;

	    RESULT << "Doing distribution upgrade ..." << endl;
	    UpgradeStatistics stats;

	    string delete_unmaintained = node->getProp ("delete_unmaintained");
	    if (delete_unmaintained == "false") {
		stats.delete_unmaintained = false;
	    } else {
		stats.delete_unmaintained = true;
            }

	    resolver->doUpgrade(stats);

	    print_pool( resolver, MARKER );


	} else if (node->equals ("instorder")) {

	    RESULT << "Calculating installation order ..." << endl;

	    instorder = true;

	} else if (node->equals ("mediaorder")) {

	    RESULT << "Calculating media installation order ..." << endl;

	    mediaorder = true;

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
            for (unsigned i=0; i < names.size(); i++) {
                resolver->addExtraConflict(Capability (names[i], string2kind (node->getProp ("kind"))));
            }
	} else if (node->equals ("addRequire")) {
            vector<string> names;
            str::split( node->getProp ("name"), back_inserter(names), "," );
            for (unsigned i=0; i < names.size(); i++) {
                resolver->addExtraRequire(Capability (names[i], string2kind (node->getProp ("kind"))));
            }
	} else if (node->equals ("reportproblems")) {
	    if (resolver->resolvePool() == true
                && node->getProp ("ignoreValidSolution").empty()) {
		RESULT << "No problems so far" << endl;
	    }
	    else {
		ResolverProblemList problems = resolver->problems ();
		problems.sort(compare_problems());
		RESULT << problems.size() << " problems found:" << endl;
		for (ResolverProblemList::iterator iter = problems.begin(); iter != problems.end(); ++iter) {
		    ResolverProblem problem = **iter;
		    RESULT << "Problem:" << endl;
		    RESULT << problem.description() << endl;
		    RESULT << problem.details() << endl;

		    ProblemSolutionList solutions = problem.solutions();
		    for (ProblemSolutionList::const_iterator iter = solutions.begin();
			 iter != solutions.end(); ++iter) {
			ProblemSolution solution = **iter;
			RESULT << "   Solution:" << endl;
			RESULT << "      " << solution.description() << endl;
			RESULT << "      " << solution.details() << endl;
		    }
		}
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
		if (resolver->resolvePool() == true) {
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
#if 0
	} else if (node->equals ("graphic")) {
            resolver->resolvePool();
            QApplication app(0, NULL);    
            QZyppSolverDialog *dialog = new QZyppSolverDialog(resolver);
            app.setMainWidget( dialog );
            dialog->setCaption("Solvertree");
            dialog->setMinimumSize ( 700, 700 );
            dialog->show();
            app.exec();
#endif
	} else if (node->equals ("lock")) {
	    string source_alias = node->getProp ("channel");
	    string package_name = node->getProp ("name");
	    if (package_name.empty())
		package_name = node->getProp ("package");            
	    string kind_name = node->getProp ("kind");

	    PoolItem poolItem;

	    poolItem = get_poolItem (source_alias, package_name, kind_name);
	    if (poolItem) {
		RESULT << "Locking " << package_name << " from channel " << source_alias << endl;
		poolItem.status().setLock (true, ResStatus::USER);
	    } else {
		cerr << "Unknown package " << source_alias << "::" << package_name << endl;
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
		source_alias = "@system";

	    if (name.empty())
	    {
		cerr << "transact need 'name' parameter" << endl;
		return;
	    }

            PoolItem poolItem;

            poolItem = get_poolItem( source_alias, name, kind_name );

            if (poolItem) {
                // first: set anything
                if (source_alias == "@system") {
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
	} else if (node->equals ("createTestcase")) {
	    string path = node->getProp ("path");
            if (path.empty())
                path = "./solverTestcase";
            Testcase testcase (path);
            testcase.createTestcase (*resolver);

	} else {
	    cerr << "Unknown tag '" << node->name() << "' in trial" << endl;
	}

	node = node->next();
    }

    bool success = false;
    
    if (verify)
	success = resolver->verifySystem ();
    else
	success = resolver->resolvePool();
    if (success) {
        print_solution (pool, instorder, mediaorder);
    } else {
        RESULT << "No valid solution found." << endl;
    }
        
}


//---------------------------------------------------------------------------------------------------------------------

static void
parse_xml_test (XmlNode_Ptr node, ResPool & pool)
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
		parse_xml_trial( node, pool );
	    } else {
		cerr << "Unknown tag '" << node->name() << "' in test" << endl;
	    }
	}

	node = node->next();
    }
}


static void
process_xml_test_file (const string & filename, ResPool & pool)
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

    parse_xml_test (root, pool);

    xmlFreeDoc (xml_doc);
}


//---------------------------------------------------------------------------------------------------------------------

int
main (int argc, char *argv[])
{

    if (argc != 2) {
	cerr << "Usage: deptestomatic testfile.xml" << endl;
	exit (0);
    }
    zypp::base::LogControl::instance().logfile( "-" );

    forceResolve = false;

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

    globalPath = argv[1];
    globalPath = globalPath.substr (0, globalPath.find_last_of ("/") +1);

    DBG << "init_libzypp() done" << endl;

    ResPool  pool = God->pool();    
    process_xml_test_file (string (argv[1]), pool);

    return 0;
}

