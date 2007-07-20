// test for HelixSourceImpl
//

#include <iostream>
#include <fstream>
#include <map>
#include "zypp/base/Logger.h"
#include "zypp/base/LogControl.h"
#include "zypp/media/MediaManager.h"

#include "src/helix/HelixSourceImpl.h"
#include "zypp/Repository.h"
#include "zypp/RepoManager.h"

using namespace std;
using namespace zypp;

/******************************************************************
**
**
**	FUNCTION NAME : main
**	FUNCTION TYPE : int
**
**	DESCRIPTION :
*/
int main( int argc, char * argv[] )
{
    if (argc < 2) {
	cerr << "Usage: rcparse <helixname>" << endl;
	exit (1);
    }

    zypp::base::LogControl::instance().logfile( "-" );

    INT << "===[START]==========================================" << endl;

    Pathname p = argv[1];
    Url url("file:/");
    string alias("helixparse");

#if 0	// old SourceManager
    media::MediaManager mmgr;
    media::MediaId mediaid = mmgr.open(Url("file://"));
    Source_Ref::Impl_Ptr impl = new HelixSourceImpl (mediaid, p);
    SourceFactory _f;
#endif
#if 0 // via SourceFactory
    Pathname cache_dir("");
    Source_Ref src( SourceFactory().createFrom(url, p, alias, cache_dir) );


    // via HelixSourceImpl

    media::MediaManager mmgr;
    media::MediaId mediaid = mmgr.open(url);
    HelixSourceImpl *impl = new HelixSourceImpl ();
    MIL << "Calling factoryCtor()" << endl;
    impl->factoryCtor (mediaid, p, alias);
    MIL << "Calling createFromImpl()" << endl;
    Source_Ref src( SourceFactory().createFrom(impl) );

    ResStore store = src.resolvables();
#endif
    Repository repo;    
    RepoInfo nrepo;
    nrepo
	.setAlias      ( alias )
	.setName       ( "namen sind schall und rauch" )
	.setEnabled    ( true )
	.setAutorefresh( false )
	.addBaseUrl    ( p.asUrl() );

    zypp::repo::RepositoryImpl_Ptr impl( new HelixSourceImpl( nrepo ) );

    repo = Repository( impl );
    ResStore store = repo.resolvables();    
    
    for (ResStore::const_iterator it = store.begin();
	it != store.end(); it++)
    {
	ResObject::constPtr ptr( *it );
	ERR << *ptr << endl;
	if (argc > 2) ERR << (ptr->deps()) << endl;
    }
    ERR << store << endl;
    INT << "===[END]============================================" << endl;
    return 0;
}
