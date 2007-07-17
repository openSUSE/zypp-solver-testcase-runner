/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/solver/temporary/HelixSelectionImpl.cc
 *
*/

#include "HelixSelectionImpl.h"
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
//        CLASS NAME : HelixSelectionImpl
//
///////////////////////////////////////////////////////////////////

/** Default ctor
*/
HelixSelectionImpl::HelixSelectionImpl (Repository source_r, const zypp::HelixParser & parsed)
    : _source (source_r)
    , _summary(parsed.summary)
    , _description(parsed.description)
{
}

Repository
HelixSelectionImpl::source() const
{ return _source; }

/** Selection summary */
TranslatedText HelixSelectionImpl::summary() const
{ return _summary; }

/** Selection description */
TranslatedText HelixSelectionImpl::description() const
{ return _description; }


  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
