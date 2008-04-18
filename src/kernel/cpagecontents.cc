/*
 * PDFedit - free program for PDF document manipulation.
 * Copyright (C) 2006, 2007, 2008  PDFedit team: Michal Hocko,
 *                                              Miroslav Jahoda,
 *                                              Jozef Misutka,
 *                                              Martin Petricek
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in doc/LICENSE.GPL); if not, write to the 
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 * MA  02111-1307  USA
 *
 * Project is hosted on http://sourceforge.net/projects/pdfedit
 */
// vim:tabstop=4:shiftwidth=4:noexpandtab:textwidth=80

// static
#include "kernel/static.h"

#include "kernel/cpagecontents.h"
#include "utils/observer.h"
#include "kernel/cobject.h"
#include "kernel/cpage.h"
#include "kernel/cpdf.h"
#include "kernel/cpagedisplay.h"

//==========================================================
namespace pdfobjects {
//==========================================================

using namespace std;
using namespace boost;
using namespace observer;
using namespace utils;


//==========================================================
// Contents watchdog
//==========================================================


//
//
//
void 
CPageContents::ContentsWatchDog::notify (shared_ptr<IProperty> newValue, 
						  shared_ptr<const IProperty::ObserverContext> context) const throw()
{
		kernelPrintDbg (debug::DBG_DBG, "context type=" << context->getType());

	//Several scenarios can happen
	//1) page dictionary gets changed
	//	OK 1.1 - added Contents entry
	//	OK 1.2 - removed Contents entry
	//2) Contents entry
	//	OK 2.1 - changed stream
	//	2.2 - changed array
	//		OK 2.2.1 -- added entry
	//		OK 2.2.2 -- removed entry
	//		OK 2.2.3 -- changed entry
	//

	// Switch type
	switch(context->getType())
	{
		// 2.2 Stream was changed  - not supported for now on
		// 2.2.3 -- changed entry
		case BasicChangeContextType:
			break;

		// All other possibilities
		case ComplexChangeContextType:
		{
			// Is it a dictionary Page dictionary
			shared_ptr<const CDict::CDictComplexObserverContext> ctxtdict =
				dynamic_pointer_cast<const CDict::CDictComplexObserverContext, 
									 const IChangeContext<IProperty> > (context); 
			if (ctxtdict)
			{
				//
				//Several scenarios can happen
				//1) page dictionary gets changed
				//	1.1 - added Contents entry
				//	1.2 - removed Contents entry
				//
					// if it is not about Contents do nothing
					if (Specification::Page::CONTENTS != ctxtdict->getValueId())
						return;
				
				// 1.2 Contents entry was removed
				if (isNull(newValue))
				{
					shared_ptr<IProperty> oldValue = ctxtdict->getOriginalValue();
					// Unregister observer
					_cnt->unreg_observer (oldValue);
				
				// 1.1 Contents entry was added	
				}else 
				{
					_cnt->reg_observer(newValue);
				}

				break;
			}

			// Is it an array (Contents) -- do nothing just reparse
			shared_ptr<const CArray::CArrayComplexObserverContext> ctxtarray =
				dynamic_pointer_cast<const CArray::CArrayComplexObserverContext, 
									 const IChangeContext<IProperty> > (context); 
			if (ctxtarray)
			{
				//
				//2) Contents entry
				//	2.2 - changed array
				//		2.2.1 -- added entry
				//		2.2.2 -- removed entry
				//
				break;
			}

		}

		default:
				assert (!"Invalid change context - contents observer!");
			break;
	}

	// Parse content streams (add or delete of object)
	try {
		_cnt->parse ();
	}catch (...)
	{
		_cnt->_page->invalidate ();
	}
}



//==========================================================
namespace {
//==========================================================


	/**
	 * Create stream from container of objects that implement
	 * getStringRepresentation function. And inserts this stream into supplied pdf.
	 */
	template<typename Container>
	boost::shared_ptr<CStream> 
	createStreamFromObjects (const Container& cont, CPdf& pdf)
	{
		// Create stream with one default property Length
		shared_ptr<CStream> newstr (new CStream());
		
		// Insert our change tag
		std::string str;
		{
			std::string tmpop;
			ContentsChangeTag::create()->getStringRepresentation (tmpop);
			str += tmpop + " ";
		}
		
		//
		// Get string representation of new content stream
		//
		typename Container::const_iterator it = cont.begin();
		for (; it != cont.end(); ++it)
		{
			std::string tmpop;
			(*it)->getStringRepresentation (tmpop);
			str += tmpop + " ";
		}
		kernelPrintDbg (debug::DBG_DBG, str);

		// Set the stream
		newstr->setBuffer (str);

		// Set ref and indiref reserve free indiref for the new object
		IndiRef newref = pdf.addIndirectProperty (newstr);
		newstr = IProperty::getSmartCObjectPtr<CStream> (pdf.getIndirectProperty (newref));
			assert (newstr);

		return newstr;
	}


	/**
	 * Get all cstreams from a container of content streams.
	 */
	template<typename In, typename Out>
	void getAllCStreams (const In& in, Out& out)
	{
		for (typename In::const_iterator it = in.begin(); it != in.end(); ++it)
		{
			Out tmp;
			(*it)->getCStreams (tmp);
			copy (tmp.begin(), tmp.end(), back_inserter (out));
		}
	}

//==========================================================
} // namespace
//==========================================================


//==========================================================
// CPageContents
//==========================================================

CPageContents::CPageContents (CPage* page) : _page(page), _wd (new ContentsWatchDog (this))
{
	if (_page)
		_dict = _page->getDictionary();
	reg_observer ();
}

CPageContents::~CPageContents ()
{
	reset ();
}


shared_ptr<CContentStream> 
CPageContents::getContentStream (CContentStream* cc)
{
	for (CCs::iterator it = _ccs.begin(); it != _ccs.end(); ++it)
		if ((*it).get() == cc)
		  return *it;

		assert (!"Contentstream not found");
		throw CObjInvalidOperation ();
}


shared_ptr<CContentStream> 
CPageContents::getContentStream (size_t pos)
{
		if (pos >= _ccs.size())
			throw CObjInvalidOperation ();
	return _ccs[pos];
}


//
//
//
template<typename Container>
void 
CPageContents::addToFront (const Container& cont)
{ 
	// Create cstream from container of pdf operators
	CPdf* pdf = _dict->getPdf();
		assert (pdf);
	shared_ptr<CStream> stream = createStreamFromObjects (cont, *pdf);
		assert (hasValidRef (stream)); assert (hasValidPdf (stream));
		if (!hasValidPdf(stream) || !hasValidPdf(stream))
			throw CObjInvalidObject ();

	// Change the contents entry
	CRef rf (stream->getIndiRef());
	toFront (rf);

	// Parse new stream to content stream and add it to the streams
	CContentStream::CStreams streams;
	streams.push_back (stream);
	boost::shared_ptr<GfxResources> res;
	boost::shared_ptr<GfxState> state;
	_xpdf_display_params (res, state);
	CCs _tmp;
	// Init cc and save smart pointer of the content stream so pdfoperators can get it
	boost::shared_ptr<CContentStream> cc (new CContentStream(streams,state,res));
	cc->setSmartPointer (cc);

	// copy it to front
	_tmp.push_back (cc);
	std::copy (_ccs.begin(), _ccs.end(), std::back_inserter(_tmp));
	_ccs = _tmp;

	// Indicate change
	change ();
}
template void CPageContents::addToFront<vector<shared_ptr<PdfOperator> > > (const vector<shared_ptr<PdfOperator> >& cont);
template void CPageContents::addToFront<deque<shared_ptr<PdfOperator> > > (const deque<shared_ptr<PdfOperator> >& cont);

//
//
//
template<typename Container>
void 
CPageContents::addToBack (const Container& cont)
{
	// Create cstream from container of pdf operators
	CPdf* pdf = _dict->getPdf();
		if (!pdf)
			throw CObjInvalidObject ();
	shared_ptr<CStream> stream = createStreamFromObjects (cont, *pdf);
		assert (hasValidRef (stream)); assert (hasValidPdf (stream));
		if (!hasValidPdf(stream) || !hasValidPdf(stream))
			throw CObjInvalidObject ();

	// Change the contents entry
	CRef rf (stream->getIndiRef());
	toBack (rf);


	// Parse new stream to content stream and add it to the streams
	CContentStream::CStreams streams;
	streams.push_back (stream);
	boost::shared_ptr<GfxResources> res;
	boost::shared_ptr<GfxState> state;
	_xpdf_display_params (res, state);
	// Init and save smart pointer
	boost::shared_ptr<CContentStream> cc (new CContentStream(streams,state,res));
	cc->setSmartPointer (cc);

	_ccs.push_back (cc);

	// Indicate change
	change ();
}
template void CPageContents::addToBack<vector<shared_ptr<PdfOperator> > > (const vector<shared_ptr<PdfOperator> >& cont);
template void CPageContents::addToBack<deque<shared_ptr<PdfOperator> > > (const deque<shared_ptr<PdfOperator> >& cont);


//
//
//
void 
CPageContents::remove (size_t csnum)
{
		if (!_dict->getPdf())
			throw CObjInvalidObject ();
		if (csnum >= _ccs.size())
			throw OutOfRange ();

	// Change the contents entry
	remove (_ccs[csnum]);
	// Remove contentstream from container
	_ccs.erase (_ccs.begin() + csnum, _ccs.begin() + csnum + 1);

	// Indicate change
	change ();
}

//
//
//
void
CPageContents::getText (std::string& text, const string* encoding, const libs::Rectangle* rc) const
{
		kernelPrintDbg (debug::DBG_DBG, "");

	// Create text output device
	boost::scoped_ptr<TextOutputDev> textDev (new ::TextOutputDev (NULL, gFalse, gFalse, gFalse));
		if (!textDev->isOk())
			throw CObjInvalidOperation ();

	// Display page
	_page->display()->displayPage (*textDev);	

	// Set encoding
	if (encoding)
    	globalParams->setTextEncoding(const_cast<char*>(encoding->c_str()));

	// Get the text
	libs::Rectangle rec = (rc)? *rc : _page->display()->getPageRect();
	scoped_ptr<GString> gtxt (textDev->getText(rec.xleft, rec.yleft, rec.xright, rec.yright));
	text = gtxt->getCString();
}


//
// Text search/find
//

// 
// Find all occcurences of a text on a page
//
template<typename RectangleContainer>
size_t CPageContents::findText (std::string text, 
					  RectangleContainer& recs, 
					  const TextSearchParams&) const
{
	// Create text output device
	scoped_ptr<TextOutputDev> textDev (new ::TextOutputDev (NULL, gFalse, gFalse, gFalse));
		assert (textDev->isOk());
		if (!textDev->isOk())
			throw CObjInvalidOperation ();

	// Get the text
	_page->display()->displayPage (*textDev);	

	GBool startAtTop, stopAtBottom, startAtLast, stopAtLast, caseSensitive, backward;
	startAtTop = stopAtBottom = startAtLast = stopAtLast = gTrue;
	caseSensitive = backward = gFalse;
	
	double xMin = 0, yMin = 0, xMax = 0, yMax = 0;

	// Convert text to unicode (slightly modified from from PDFCore.cc)
	int length = static_cast<int>(text.length());
	::Unicode* utext = static_cast<Unicode*> (new Unicode [length]);
	for (int i = 0; i < length; ++i)
	    utext[i] = static_cast<Unicode> (text[i] & 0xff);
	
	if (textDev->findText(utext, length, startAtTop, stopAtBottom, 
				startAtLast,stopAtLast, caseSensitive, backward,
				&xMin, &yMin, &xMax, &yMax))
	{
		startAtTop = gFalse;
		
		recs.push_back (libs::Rectangle (xMin, yMin, xMax, yMax));
		// Get all text objects
		while (textDev->findText (utext, length,
								  startAtTop, stopAtBottom, 
								  startAtLast, stopAtLast, 
								  caseSensitive, backward,
								  &xMin, &yMin, &xMax, &yMax))
		{
			recs.push_back (libs::Rectangle (xMin, yMin, xMax, yMax));
		}
	}

	//
	// Find out the words...
	//
	delete[] utext;	
	return recs.size();
}

// Explicit instantiation
template size_t CPageContents::findText<std::vector<libs::Rectangle> >
	(std::string text, 
	 std::vector<libs::Rectangle>& recs, 
	 const TextSearchParams& params) const;


//==========================================================
namespace {
//==========================================================


	struct ToFront
	{
		static inline void add (CArray& arr, CRef& ref, IProperty& content)
		{
			arr.addProperty (ref);
			arr.addProperty (content);
		}
		static inline void add (CArray& arr, CRef& ref)
			{ arr.addProperty (0, ref); }
	};
	struct ToBack
	{
		static inline void add (CArray& arr, CRef& ref, IProperty& content)
		{
			arr.addProperty (content);
			arr.addProperty (ref);
		}
		static inline void add (CArray& arr, CRef& ref)
			{ arr.addProperty (arr.getPropertyCount(), ref); }
	};
	enum OPERWHERE {FRONT, BACK} ;
	template<int T> struct OpTrait; 
	template<> struct OpTrait<FRONT> {typedef struct ToFront Oper; };
	template<> struct OpTrait<BACK> {typedef struct ToBack Oper; };


	// addSomewhere
	template<OPERWHERE WHERE>
	void
	cc_add (shared_ptr<CDict> _dict, CRef& ref)
	{
		// contents not present
		if (!_dict->containsProperty (Specification::Page::CONTENTS))
		{
			CArray arr;
			arr.addProperty (ref);
			_dict->addProperty (Specification::Page::CONTENTS, arr);
			
		// contents present
		}else
		{
			shared_ptr<IProperty> content = _dict->getProperty (Specification::Page::CONTENTS);
			shared_ptr<IProperty> realcontent = getReferencedObject(content);
				assert (content);
			// Contents can be either stream or an array of streams
			if (isStream (realcontent))	
			{
				CArray arr;
				OpTrait<WHERE>::Oper::add (arr, ref, *content);
				_dict->setProperty (Specification::Page::CONTENTS, arr);
		
			}else if (isArray (realcontent))
			{
				// We can be sure that streams are indirect objects (pdf spec)
				shared_ptr<CArray> array = IProperty::getSmartCObjectPtr<CArray> (realcontent);
				OpTrait<WHERE>::Oper::add (*array, ref);
			
			}else // Neither stream nor array
			{
				kernelPrintDbg (debug::DBG_CRIT, "Content stream type: " << realcontent->getType());
				throw ElementBadTypeException ("Bad content stream type.");
			}
		}
	}

//==========================================================
} // namespace
//==========================================================

//
//
//
void 
CPageContents::toFront (CRef& ref)
{
	{
		ContentsObserverFreeSection reg_lock (this);
		cc_add<FRONT> (_dict, ref);
	}
	// Indicate change
	change ();
}

//
//
//
void 
CPageContents::toBack (CRef& ref)
{
	{
		ContentsObserverFreeSection reg_lock (this);
		cc_add<BACK> (_dict, ref);
	}
	// Indicate change
	change ();
}


//
//
//
/** 
 * Set Contents entry from a container of content streams. 
 * Indicats that the page changed.
 */
template<typename Cont>
void CPageContents::setContents (shared_ptr<CDict> dict, const Cont& cont)
{

	if (dict->containsProperty (Specification::Page::CONTENTS))
		dict->delProperty (Specification::Page::CONTENTS);
	
	//
	// Loop throug all content streams and add all cstreams from each
	// content streams to Contents entry of page dictionary
	//
	typedef vector<shared_ptr<CStream> > Css;
	Css css;
	getAllCStreams (cont, css);
	
	// Loop through all cstreams
	for (Css::iterator it = css.begin(); it != css.end(); ++it)
	{
			assert (hasValidPdf (*it));
			assert (hasValidRef (*it));
			if (!hasValidPdf (*it) || !hasValidRef (*it))
				throw XpdfInvalidObject ();
		CRef rf ((*it)->getIndiRef ());
		cc_add<BACK> (dict, rf);
	}
}
// Explicit instantiation
template void CPageContents::setContents<vector<shared_ptr<CContentStream> > >
	(shared_ptr<CDict> dict, const vector<shared_ptr<CContentStream> >& cont);

//
//
//
void 
CPageContents::remove (shared_ptr<const CContentStream> cs)
{
		if (!_dict->containsProperty (Specification::Page::CONTENTS))
			throw CObjInvalidOperation ();

	{
		ContentsObserverFreeSection reg_lock  (this);

		//
		// Loop throug all content streams and add all cstreams from each
		// content streams to Contents entry of page dictionary
		//
		typedef vector<shared_ptr<CStream> > Css;
		Css css;
		cs->getCStreams (css);
		
		// Loop through all cstreams
		for (Css::iterator it = css.begin(); it != css.end(); ++it)
		{
				assert (hasValidPdf (*it));
				assert (hasValidRef (*it));
				if (!hasValidPdf (*it) || !hasValidRef (*it))
					throw CObjInvalidObject ();

			// Remove the reference 
			remove ((*it)->getIndiRef ());
		}
	}
	// Indicate change
	change ();
}

//
//
//
void 
CPageContents::remove (const IndiRef& rf)
{
	shared_ptr<IProperty> content = _dict->getProperty (Specification::Page::CONTENTS);
	shared_ptr<IProperty> realcontent = getReferencedObject (content);
		assert (content);
	// Contents can be either stream or an array of streams
	if (isStream (realcontent))	
	{
		// Set empty contents
		CArray arr;
		_dict->setProperty (Specification::Page::CONTENTS, arr);

	}else if (isArray (realcontent))
	{
		// We can be sure that streams are indirect objects (pdf spec)
		shared_ptr<CArray> array = IProperty::getSmartCObjectPtr<CArray> (realcontent);
		for (size_t i = 0; i < array->getPropertyCount(); ++i)
		{
			IndiRef _rf = getRefFromArray (array,i);
			if (_rf == rf)
			{
				array->delProperty (i);
				return;
			}
		}
	
	}else // Neither stream nor array
	{
		kernelPrintDbg (debug::DBG_CRIT, "Content stream type: " << realcontent->getType());
		throw ElementBadTypeException ("Bad content stream type.");
	}
}

//
//
//
void 
CPageContents::reparse ( )
{
		if (!hasValidPdf(_dict) || !hasValidRef(_dict))
			throw CObjInvalidObject ();
	
	//
	// Create state and resources
	//
	boost::shared_ptr<GfxResources> res;
	boost::shared_ptr<GfxState> state;
	_xpdf_display_params (res, state);
	
	// Set only bboxes
	for (CCs::iterator it = _ccs.begin(); it != _ccs.end(); ++it)
		(*it)->reparse (true, state, res);

	change ();
}

bool 
CPageContents::parse ()
{
		if (!hasValidPdf(_dict) || !hasValidRef(_dict))
			throw CObjInvalidObject ();

	// Clear content streams
	_ccs.clear();

	//
	// Create state and resources
	//
	boost::shared_ptr<GfxResources> res;
	boost::shared_ptr<GfxState> state;
	_xpdf_display_params (res, state);
	
	//
	// Get the stream representing content stream (if any), make an xpdf object
	// and finally instantiate CContentStream
	//
		if (!_dict->containsProperty (Specification::Page::CONTENTS))
			return true;
	shared_ptr<IProperty> contents = getReferencedObject (_dict->getProperty (Specification::Page::CONTENTS));
		assert (contents);
	
	CContentStream::CStreams streams;
	
	//
	// Contents can be either stream or an array of streams
	//
	if (isStream (contents))	
	{
		shared_ptr<CStream> stream = IProperty::getSmartCObjectPtr<CStream> (contents); 
		streams.push_back (stream);
	
	}else if (isArray (contents))
	{
		// We can be sure that streams are indirect objects (pdf spec)
		shared_ptr<CArray> array = IProperty::getSmartCObjectPtr<CArray> (contents); 
		for (size_t i = 0; i < array->getPropertyCount(); ++i)
			streams.push_back (getCStreamFromArray(array,i));
		
	}else // Neither stream nor array
	{
		kernelPrintDbg (debug::DBG_CRIT, "Content stream type: " << contents->getType());
		throw ElementBadTypeException ("Bad content stream type.");
	}

	//
	// Create content streams, each cycle will take one/more content streams from streams variable
	//
		assert (_ccs.empty());
	// True if Contents is not [ ]
	while (!streams.empty())
	{
		shared_ptr<CContentStream> cc (new CContentStream(streams,state,res));
		// Save smart pointer of the content stream so pdfoperators can return it
		cc->setSmartPointer (cc);
		_ccs.push_back (cc);
	}


	// Indicate change
	change ();

	// Everything went ok
	return true;
}



void
CPageContents::reg_observer (boost::shared_ptr<IProperty> ip) const
{
	if (ip)
	{
		REGISTER_SHAREDPTR_OBSERVER(ip, _wd);
	}else
	{
		// Register dictionary and Contents observer
		REGISTER_SHAREDPTR_OBSERVER(_dict, _wd);
		// If it contains Contents register observer on it too
		if (_dict->containsProperty(Specification::Page::CONTENTS))
		{
			shared_ptr<IProperty> prop = _dict->getProperty(Specification::Page::CONTENTS);
			REGISTER_SHAREDPTR_OBSERVER(prop, _wd);
		}
	}
}

//
//
//
void
CPageContents::unreg_observer (boost::shared_ptr<IProperty> ip) const
{
	if (ip)
	{
		UNREGISTER_SHAREDPTR_OBSERVER(ip, _wd);
	}else
	{
		// Unregister dictionary observer
		UNREGISTER_SHAREDPTR_OBSERVER(_dict, _wd);
		// Unregister contents observer
		if (_dict->containsProperty(Specification::Page::CONTENTS))
		{
			shared_ptr<IProperty> prop = _dict->getProperty(Specification::Page::CONTENTS);
			UNREGISTER_SHAREDPTR_OBSERVER(prop, _wd);
		}
	}
}


void 
CPageContents::change (bool invalid)
{ 
	_page->_objectChanged (invalid); 
}

void 
CPageContents::_xpdf_display_params (boost::shared_ptr<GfxResources>& res, 
									boost::shared_ptr<GfxState>& state)
{
	_page->display()->createXpdfDisplayParams (res, state);
}

size_t
CPageContents::_page_pos () const
{
	return _page->getPagePosition();
}

//
//
//
void 
CPageContents::moveAbove (shared_ptr<const CContentStream> ct)
{
	// Get the next item
	CCs::iterator itNext = find (_ccs.begin(), _ccs.end(), ct);
		if (itNext == _ccs.end())
			throw CObjInvalidOperation ();
	++itNext;
		if (itNext == _ccs.end())
			throw OutOfRange ();

	// Delete next item but store it
	shared_ptr<CContentStream> tmp = *itNext;
	_ccs.erase (itNext, itNext + 1);
	// Insert stored item before supplied (simply swap ct with the next item)
	_ccs.insert (find (_ccs.begin(), _ccs.end(), ct), tmp);

	{
		ContentsObserverFreeSection reg_lock  (this);
		setContents (_dict, _ccs);
	}	// Also change Contents entry of page dictionary

	// Indicate change
	change ();
}

//
//
//
void 
CPageContents::moveBelow (shared_ptr<const CContentStream> ct)
{
	// Get the item index
	unsigned int pos = 0;
	for (pos = 0; pos < _ccs.size(); ++pos)
		if (_ccs[pos] == ct)
			break;
	
		// If first or not found throw exception
		if (pos == _ccs.size() || 0 == pos)
			throw CObjInvalidOperation ();
	
	// Swap
	shared_ptr<CContentStream> tmp = _ccs[pos];
	_ccs[pos] = _ccs[pos - 1];
	_ccs[pos - 1] = tmp;

	// Also change Contents entry of page dictionary
	{
		ContentsObserverFreeSection reg_lock  (this);
		setContents (_dict, _ccs);
	}	// Also change Contents entry of page dictionary

	// Indicate change
	change ();
}


//
//
//
void 
CPageContents::moveAbove (size_t pos)
{ 
	moveAbove (_page->contents()->getContentStream (pos)); 
}

//
//
//
void 
CPageContents::moveBelow (size_t pos)
{ 
	moveBelow (_page->contents()->getContentStream (pos)); 
}

void 
CPageContents::reset ()
{
		// we already made a reset
		if (!_page)
			return;
	unreg_observer ();
	_page = NULL;
	_dict.reset ();
	_wd.reset ();
		assert (!_wd.use_count());
}

//==========================================================
} // namespace pdfobjects
//==========================================================