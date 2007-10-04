/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/solver/temporary/HelixScriptImpl.cc
 *
*/

#include "HelixScriptImpl.h"
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
//        CLASS NAME : HelixScriptImpl
//
///////////////////////////////////////////////////////////////////

/** Default ctor
*/
HelixScriptImpl::HelixScriptImpl (Repository source_r, const zypp::HelixParser & parsed)
    : _source (source_r)
    , _size_installed(parsed.installedSize)
    , _vendor(parsed.vendor)    
{
}

Repository
HelixScriptImpl::repository() const
{ return _source; }

ByteCount HelixScriptImpl::size() const
{ return _size_installed; }

Vendor HelixScriptImpl::vendor() const
{ 
   if ( _vendor == "")
      return "SUSE LINUX Products GmbH, Nuernberg, Germany";
   return _vendor;
}


  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
