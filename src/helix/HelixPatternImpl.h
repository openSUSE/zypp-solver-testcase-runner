/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/solver/temporary/HelixPatternImpl.h
 *
*/
#ifndef ZYPP_SOLVER_TEMPORARY_HELIXPATTERNIMPL_H
#define ZYPP_SOLVER_TEMPORARY_HELIXPATTERNIMPL_H

#include "zypp/detail/PatternImpl.h"
#include "HelixParser.h"

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
//
//        CLASS NAME : HelixPatternImpl
//
/** Class representing a package
*/
class HelixPatternImpl : public detail::PatternImplIf
{
public:

	class HelixParser;
	/** Default ctor
	*/
	HelixPatternImpl( Repository source_r, const zypp::HelixParser & data );

	/** Pattern summary */
	virtual TranslatedText summary() const;
	/** Pattern description */
	virtual TranslatedText description() const;
	virtual ByteCount size() const;
	/** */
	virtual PackageGroup group() const;
	/** */
	virtual ByteCount archivesize() const;
	/** */
	virtual bool installOnly() const;
	/** */
	virtual Repository repository() const;
        /** */
        virtual Vendor vendor() const;
    
protected:
	Repository _source;
	TranslatedText _summary;
	TranslatedText _description;
	PackageGroup _group;
	bool _install_only;

	ByteCount _size_installed;
	ByteCount _size_archive;
        Vendor _vendor;

 };
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_SOLVER_TEMPORARY_HELIXPACKAGEIMPL_H
