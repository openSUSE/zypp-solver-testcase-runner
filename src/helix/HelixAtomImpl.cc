/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/solver/temporary/HelixAtomImpl.cc
 *
*/

#include "HelixAtomImpl.h"
#include "zypp/repo/RepositoryImpl.h"
#include "zypp/base/String.h"
#include "zypp/base/Logger.h"

using namespace std;
using namespace zypp::detail;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
//
//        CLASS NAME : HelixAtomImpl
//
///////////////////////////////////////////////////////////////////

/** Default ctor
*/
HelixAtomImpl::HelixAtomImpl (Repository source_r, const zypp::HelixParser & parsed)
    : _source (source_r)
    , _vendor(parsed.vendor)    
{
}

Repository
HelixAtomImpl::repository() const
{ return _source; }

Vendor HelixAtomImpl::vendor() const
{ 
   if ( _vendor == "")
      return "SUSE LINUX Products GmbH, Nuernberg, Germany";
   return _vendor;
}


  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
