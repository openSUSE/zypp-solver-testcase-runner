/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/solver/temporary/HelixPackageImpl.h
 *
*/
#ifndef ZYPP_SOLVER_TEMPORARY_HELIXSCRIPTIMPL_H
#define ZYPP_SOLVER_TEMPORARY_HELIXSCRIPTIMPL_H

#include "zypp/detail/ScriptImpl.h"
#include "HelixParser.h"

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
//
//        CLASS NAME : HelixScriptImpl
//
/** Class representing a script
*/
class HelixScriptImpl : public detail::ScriptImplIf
{
public:

	class HelixParser;
	/** Default ctor
	*/
	HelixScriptImpl( Repository source_r, const zypp::HelixParser & data );

      /** */
      virtual ByteCount size() const;

	/** */
	virtual Repository repository() const;

        /** */
        virtual Vendor vendor() const;
    

protected:
	Repository _source;
	ByteCount _size_installed;
        Vendor _vendor;


 };
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_SOLVER_TEMPORARY_HELIXPACKAGEIMPL_H
