/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* HelixSourceImpl.cc
 *
 * Copyright (C) 2000-2002 Ximian, Inc.
 * Copyright (C) 2005 SUSE Linux Products GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "HelixExtract.h"
#include "HelixSourceImpl.h"
#include "HelixPackageImpl.h"
#include "HelixScriptImpl.h"
#include "HelixMessageImpl.h"
#include "HelixPatchImpl.h"
#include "HelixPatternImpl.h"
#include "HelixProductImpl.h"

#include "zypp/solver/temporary/Channel.h"
#include "zypp/solver/temporary/extract.h"
#include "zypp/base/Logger.h"
#include "zypp/base/Exception.h"


using namespace std;
using namespace zypp;

//---------------------------------------------------------------------------

HelixSourceImpl::HelixSourceImpl(media::MediaAccess::Ptr & media_r, const Pathname & path_r)
{
    load (path_r.asString());
}


Dependencies
HelixSourceImpl::createDependencies (const HelixParser & parsed)
{
    Dependencies deps;

    deps.provides = parsed.provides;
    deps.prerequires = parsed.prerequires;
    deps.requires = parsed.requires;
    deps.conflicts = parsed.conflicts;
    deps.obsoletes = parsed.obsoletes;
    deps.recommends = parsed.recommends;
    deps.suggests = parsed.suggests;
    deps.freshens = parsed.freshens;
#warning enable 'enhances' in zypp/Dependencies
//    deps.enhances = parsed.enhances;

    return deps;
}


Package::Ptr
HelixSourceImpl::createPackage (const HelixParser & parsed)
{
    try
    {
	shared_ptr<HelixPackageImpl> impl(new HelixPackageImpl(parsed));

	// Collect basic Resolvable data
	NVRAD dataCollect( parsed.name,
			Edition( parsed.version, parsed.release, parsed.epoch ),
			Arch( parsed.arch ),
			createDependencies (parsed));
	Package::Ptr package = detail::makeResolvableFromImpl(dataCollect, impl);
	return package;
    }
    catch (const Exception & excpt_r)
    {
	ERR << excpt_r << endl;
	throw "Cannot create package object";
    }
    return NULL;
}


Message::Ptr
HelixSourceImpl::createMessage (const HelixParser & parsed)
{
    try
    {
	shared_ptr<HelixMessageImpl> impl(new HelixMessageImpl(parsed));

	// Collect basic Resolvable data
	NVRAD dataCollect( parsed.name,
			Edition( parsed.version, parsed.release, parsed.epoch ),
			Arch( parsed.arch ),
			createDependencies (parsed));
	Message::Ptr message = detail::makeResolvableFromImpl(dataCollect, impl);
	return message;
    }
    catch (const Exception & excpt_r)
    {
	ERR << excpt_r << endl;
	throw "Cannot create message object";
    }
    return NULL;
}


Script::Ptr
HelixSourceImpl::createScript (const HelixParser & parsed)
{
    try
    {
	shared_ptr<HelixScriptImpl> impl(new HelixScriptImpl(parsed));

	// Collect basic Resolvable data
	NVRAD dataCollect( parsed.name,
			Edition( parsed.version, parsed.release, parsed.epoch ),
			Arch( parsed.arch ),
			createDependencies (parsed));
	Script::Ptr script = detail::makeResolvableFromImpl(dataCollect, impl);
	return script;
    }
    catch (const Exception & excpt_r)
    {
	ERR << excpt_r << endl;
	throw "Cannot create script object";
    }
    return NULL;
}


Patch::Ptr
HelixSourceImpl::createPatch (const HelixParser & parsed)
{
    try
    {
	shared_ptr<HelixPatchImpl> impl(new HelixPatchImpl(parsed));

	// Collect basic Resolvable data
	NVRAD dataCollect( parsed.name,
			Edition( parsed.version, parsed.release, parsed.epoch ),
			Arch( parsed.arch ),
			createDependencies (parsed));
	Patch::Ptr patch = detail::makeResolvableFromImpl(dataCollect, impl);
	return patch;
    }
    catch (const Exception & excpt_r)
    {
	ERR << excpt_r << endl;
	throw "Cannot create patch object";
    }
    return NULL;
}


Pattern::Ptr
HelixSourceImpl::createPattern (const HelixParser & parsed)
{
    try
    {
	shared_ptr<HelixPatternImpl> impl(new HelixPatternImpl(parsed));

	// Collect basic Resolvable data
	NVRAD dataCollect( parsed.name,
			Edition( parsed.version, parsed.release, parsed.epoch ),
			Arch( parsed.arch ),
			createDependencies (parsed));
	Pattern::Ptr pattern = detail::makeResolvableFromImpl(dataCollect, impl);
	return pattern;
    }
    catch (const Exception & excpt_r)
    {
	ERR << excpt_r << endl;
	throw "Cannot create pattern object";
    }
    return NULL;
}


Product::Ptr
HelixSourceImpl::createProduct (const HelixParser & parsed)
{
    try
    {
	shared_ptr<HelixProductImpl> impl(new HelixProductImpl(parsed));

	// Collect basic Resolvable data
	NVRAD dataCollect( parsed.name,
			Edition( parsed.version, parsed.release, parsed.epoch ),
			Arch( parsed.arch ),
			createDependencies (parsed));
	Product::Ptr product = detail::makeResolvableFromImpl(dataCollect, impl);
	return product;
    }
    catch (const Exception & excpt_r)
    {
	ERR << excpt_r << endl;
	throw "Cannot create product object";
    }
    return NULL;
}

//-----------------------------------------------------------------------------

void
HelixSourceImpl::parserCallback (const HelixParser & parsed)
{
  try {
    if (parsed.kind == ResTraits<Package>::kind) {
	Package::Ptr p = createPackage (parsed);
	_store.insert (p);
    }
    else if (parsed.kind == ResTraits<Message>::kind) {
	Message::Ptr m = createMessage (parsed);
	_store.insert (m);
    }
    else if (parsed.kind == ResTraits<Script>::kind) {
	Script::Ptr s = createScript (parsed);
	_store.insert (s);
    }
    else if (parsed.kind == ResTraits<Patch>::kind) {
	Patch::Ptr p = createPatch (parsed);
	_store.insert (p);
    }
    else if (parsed.kind == ResTraits<Pattern>::kind) {
	Pattern::Ptr p = createPattern (parsed);
	_store.insert (p);
    }
    else if (parsed.kind == ResTraits<Product>::kind) {
	Product::Ptr p = createProduct (parsed);
	_store.insert (p);
    }
    else {
	ERR << "Unsupported kind " << parsed.kind << endl;
    }
  }
  catch (const Exception & excpt_r)
  {
    ZYPP_CAUGHT (excpt_r);
  }
}

//-----------------------------------------------------------------------------

// load filename as helix file
//   filename:	[channel:]/path/to/file
// if channel is given, it is used as the channel name
// if channel == "@system", mark all items as 'installed'
//
void
HelixSourceImpl::load (const string & filename)
{
    if (!filename.empty()) {

	string channel_name;
	string realname;

	string::size_type colon = filename.find (":");
	if (colon == string::npos) {
	    channel_name = "test";		// default channel name
	    realname = filename;
	}
	else {
	    channel_name = filename.substr (0, colon);
	    realname = filename.substr (++colon);
	}

	solver::detail::Channel_Ptr channel = new solver::detail::Channel (channel_name.c_str(),channel_name.c_str(),channel_name.c_str(),channel_name.c_str());
	channel->setType (solver::detail::CHANNEL_TYPE_HELIX);
	if (channel_name == "@system")
	    channel->setSystem (true);

	extractHelixFile (realname, channel, this);
    }
}