/*
 * =====================================================================================
 *        Filename:  ccontentstream.cc
 *         Created:  03/24/2006 10:33:34 PM CET
 *          Author:  jmisutka (), 
 * =====================================================================================
 *
 * \TODO: complex operators
 */

// static
#include "static.h"
#include "ccontentstream.h"
//
#include "cobject.h"

//==========================================================
namespace pdfobjects {
//==========================================================

using namespace std;
using namespace boost;
using namespace debug;
using namespace utils;


//==========================================================
// Specialized classes representing operators
//==========================================================

namespace
{
	//
	// Maximum operator arguments
	//
	const size_t MAX_OPERANDS = 6;
			
	//
	// Bit helper function
	//
	template<class T, class U>
	inline bool isBitSet(T value, U mask)
		{ return (value & ((unsigned short)0x1 << mask)) != 0;}

	inline unsigned short setNoneBitsShort()
		{ return 0x0;}

	template<class U>
	inline unsigned short setNthBitsShort(U mask)
		{ return ((unsigned short) 0x1 << mask);}

	template<class U>
	inline unsigned short setNthBitsShort(U mask, U mask1)
		{ return setNthBitsShort (mask) | setNthBitsShort (mask1); }
	template<class U>
	inline unsigned short setNthBitsShort(U mask, U mask1, U mask2)
		{ return setNthBitsShort (mask,mask1) | setNthBitsShort (mask2); }
	template<class U>
	inline unsigned short setNthBitsShort(U mask, U mask1, U mask2, U mask3)
		{ return setNthBitsShort (mask,mask1,mask2) | setNthBitsShort (mask3); }
	template<class U>
	inline unsigned short setNthBitsShort(U mask, U mask1, U mask2, U mask3, U mask4)
		{ return setNthBitsShort (mask,mask1,mask2,mask3) | setNthBitsShort (mask4); }


	//
	// Known operators, it is copied from pdf BECAUSE it is
	// a private member variable of a class and we do not have
	// access to it
	//
	typedef struct
	{
		const char 				name[4];	
		const size_t			argNum;
		const unsigned short	types[MAX_OPERANDS];	// Bits represent what it should be
		
	} CheckTypes;

	/**
	 * All known operators.
	 * 
	 */
	const CheckTypes KNOWN_OPERATORS[] = {
			{"",  3, 	setNthBitsShort (pInt, pReal),
					 	setNthBitsShort (pInt, pReal),    
					 	setNthBitsShort (pString) },
			{"'", 1, 	setNthBitsShort (pString) },
			{"B", 0, 	setNoneBitsShort () },
			{"B*", 0, 	setNoneBitsShort () },
			{"BDC", 2, 	setNthBitsShort (pName),   
						setNthBitsShort (pDict, pName) },
			{"BI", 0, 	setNoneBitsShort () },
			{"BMC", 1, 	setNthBitsShort (pName) },
			{"BT",  0, 	setNoneBitsShort () },
			{"BX",  0, 	setNoneBitsShort () },
			{"CS",  1, 	setNthBitsShort (pName) },
			{"DP",  2,	setNthBitsShort (pName),   
						setNthBitsShort (pDict, pName) },
			{"Do",  1, 	setNthBitsShort (pName) },
			{"EI",  0, 	setNoneBitsShort () },
			{"EMC", 0, 	setNoneBitsShort () },
			{"ET",  0, 	setNoneBitsShort () },
			{"EX",  0, 	setNoneBitsShort () },
			{"F",   0, 	setNoneBitsShort () },
			{"G",   1, 	setNthBitsShort (pInt, pReal) },
			{"ID",  0, 	setNoneBitsShort () },
			{"J",   1, 	setNthBitsShort (pInt) },
			{"K",   4, 	setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),
						setNthBitsShort (pInt, pReal),
						setNthBitsShort (pInt, pReal) },
			{"M",   1, 	setNthBitsShort (pInt, pReal) },
			{"MP",  1, 	setNthBitsShort (pName) },
			{"Q",   0, 	setNoneBitsShort () },
			{"RG",  3, 	setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal) },
			{"S",   0, 	setNoneBitsShort () },
			{"SC",  4, setNthBitsShort (pInt, pReal),   
						setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal) },
  			{"SCN", 5, setNthBitsShort (pInt, pReal, pName),   
						setNthBitsShort (pInt, pReal, pName),    
						setNthBitsShort (pInt, pReal, pName),    
						setNthBitsShort (pInt, pReal, pName),
						setNthBitsShort (pInt, pReal, pName)},
			{"T*",  0, 	setNoneBitsShort () },
			{"TD",  2, 	setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal) },
			{"TJ",  1, 	setNthBitsShort (pArray) },
			{"TL",  1, 	setNthBitsShort (pInt, pReal) },
			{"Tc",  1, 	setNthBitsShort (pInt, pReal) },
			{"Td",  2, 	setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal) },
			{"Tf",  2, 	setNthBitsShort (pName),   
						setNthBitsShort (pInt, pReal) },
			{"Tj",  1, 	setNthBitsShort (pString) },
			{"Tm",  6, 	setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),
						setNthBitsShort (pInt, pReal),
						setNthBitsShort (pInt, pReal)},
			{"Tr",  1, 	setNthBitsShort (pInt) },
			{"Ts",  1, 	setNthBitsShort (pInt, pReal) },
			{"Tw",  1, 	setNthBitsShort (pInt, pReal) },
			{"Tz",  1, 	setNthBitsShort (pInt, pReal) },
			{"W",   0, 	setNoneBitsShort () },
			{"W*",  0, 	setNoneBitsShort () },
			{"b",   0, 	setNoneBitsShort () },
			{"b*",  0, 	setNoneBitsShort () },
  			{"c",   6, 	setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),
						setNthBitsShort (pInt, pReal),
						setNthBitsShort (pInt, pReal)},
  			{"cm",  6, 	setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),
						setNthBitsShort (pInt, pReal),
						setNthBitsShort (pInt, pReal)},
			{"cs",  1, 	setNthBitsShort (pName) },
			{"d",   2, 	setNthBitsShort (pArray),  
						setNthBitsShort (pInt, pReal) },
			{"d0",  2, 	setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal) },
  			{"d1",  6, 	setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal)},
			{"f",   0, 	setNoneBitsShort () },
			{"f*",  0, 	setNoneBitsShort () },
			{"g",   1, 	setNthBitsShort (pInt, pReal) },
			{"gs",  1, 	setNthBitsShort (pName) },
			{"h",   0, 	setNoneBitsShort () },
			{"i",   1, 	setNthBitsShort (pInt, pReal) },
			{"j",   1, 	setNthBitsShort (pInt) },
			{"k",   4, 	setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal) },
			{"l",   2, 	setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal) },
			{"m",   2, 	setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal) },
			{"n",   0, 	setNoneBitsShort () },
			{"q",   0, 	setNoneBitsShort () },
			{"re",  4, 	setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal) },
			{"rg",  3, 	setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal) },
			{"ri",  1, 	setNthBitsShort (pName) },
			{"s",   0, 	setNoneBitsShort () },
			{"sc",  4, setNthBitsShort (pInt, pReal),   
						setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal) },
  			{"scn", 5, setNthBitsShort (pInt, pReal, pName),   
						setNthBitsShort (pInt, pReal, pName),    
						setNthBitsShort (pInt, pReal, pName),    
						setNthBitsShort (pInt, pReal, pName),    
						setNthBitsShort (pInt, pReal, pName)},
			{"sh",  1, 	setNthBitsShort (pName) },
			{"v",   4, 	setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal) },
			{"w",   1, 	setNthBitsShort (pInt, pReal) },
			{"y",   4, 	setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal),    
						setNthBitsShort (pInt, pReal) },

		};

	/**
	 * Set pdf to operands.
	 *
	 * This is vital when changing those operands.
	 *
	 * @param pdf Pdf where operand will belong.
	 * @param rf  Indiref of content's stream parent.
	 * @param operands Operand stack.
	 * @param count Count of objects to bind.
	 */
	void
	operandsSetPdf (CPdf& pdf, IndiRef rf, PdfOperator::Operands& operands, int count = std::numeric_limits<int>::max())
	{
		PdfOperator::Operands::reverse_iterator it = operands.rbegin ();
		for (; (it != operands.rend ()) && (0 <= count); ++it, --count)
		{
			(*it)->setPdf (&pdf);
			(*it)->setIndiRef (rf);
		}
	}
	
	/**
	 * Check if the operands match the specification.
	 *
	 * @param ops Operator specification
	 * @param operands Operand stack.
	 *
	 * @return True if OK, false otherwise.
	 */
	bool check (const CheckTypes& ops, PdfOperator::Operands& operands)
	{
		string str;
		for (PdfOperator::Operands::iterator it = operands.begin();it != operands.end(); ++it)
		{
			string tmp;
			(*it)->getStringRepresentation (tmp);
			str += " " + tmp;
		}
		printDbg (DBG_DBG, "Operands: " << str);
		
		PdfOperator::Operands::reverse_iterator it = operands.rbegin ();
		//
		// Check arguments in reverse order
		//
		int j = ops.argNum - 1;
		for (size_t i = 0; i < ops.argNum; ++i, --j)
		{			
			assert (j >= 0);
  			if (!isBitSet(ops.types[j], (*it)->getType()))
			{
				printDbg (DBG_ERR, "Bad " << i << "-th operand type [" << (*it)->getType() << "] " << hex << " 0x" << ops.types[i]);
				return false;
			}
			// Next element
			++it;
		}

		return true;
	}

		
	/**
	 * Find the operator and create it. 
	 *
	 * Here is decided which implementation of a pdf operator is used.
	 *
	 * @param op String representation of the operator. It is used to find the operator and
	 * 			 sometimes to initialize it.
	 * @param operands Operand stack.
	 * @param pdf Pdf where it will belong
	 * @param rf  Id of the parent object.
	 * @param
	 *
	 * @return Created pdf operator.
	 */
	shared_ptr<PdfOperator>
	createOp (const string& op, PdfOperator::Operands& operands, CPdf& pdf, IndiRef rf, PdfOperator*& )
	{
		printDbg (DBG_DBG, "Finding operator: " << op);

		int lo, hi, med, cmp;
		
		cmp = std::numeric_limits<int>::max ();
		lo = -1;
		hi = sizeof (KNOWN_OPERATORS) / sizeof (CheckTypes);
		
		printDbg (DBG_DBG, "size: " << hi );
		
		// 
		// dividing of interval
		// 
		while (hi - lo > 1) 
		{
			med = (lo + hi) / 2;
			cmp = op.compare (KNOWN_OPERATORS[med].name);
			if (cmp > 0)
				lo = med;
			else if (cmp < 0)
				hi = med;
			else
				lo = hi = med;
		}
		if (0 != cmp)
		{
			printDbg (DBG_DBG, "Operator not found.");

			// Set pdf to all operands
			operandsSetPdf (pdf, rf, operands);
			return shared_ptr<UnknownPdfOperator> (new UnknownPdfOperator (operands, op));
		}
				
		printDbg (DBG_DBG, "Operator found. " << KNOWN_OPERATORS[lo].name);

		if (!check (KNOWN_OPERATORS[lo], operands))
		{
			throw MalformedFormatExeption ("Content stream bad operator type. ");
		}
		else
		{
			// Set pdf to all operands
			operandsSetPdf (pdf, rf, operands, KNOWN_OPERATORS[lo].argNum);
			// Result in lo
			return shared_ptr<SimpleGenericOperator> (new SimpleGenericOperator 
				(KNOWN_OPERATORS[lo].name, KNOWN_OPERATORS[lo].argNum, operands));
		}
		//
		// \TODO complex types
		//
	}	

	/**
	 * Parse the stream into small objects.
	 *
	 * @param operators Operator stack.
	 * @param obj 	Xpdf content stream.
	 * @param pdf 	Pdf where this content stream belongs (parent object)
	 * @param rf 	Id of parent object.
	 *
	 *
	 * \TODO 
	 */
	void
	parseContentStream (CContentStream::Operators& operators, 
						Object& obj, 		
						CPdf& pdf, 
						IndiRef rf)
	{
		assert (obj.isStream() || obj.isArray());
		
		//
		// Create the parser and lexer and get objects from it
		//
		scoped_ptr<Parser> parser (new Parser (NULL, new Lexer(NULL, &obj)));

		PdfOperator::Operands operands;
			
		Object o;
		parser->getObj(&o);

		PdfOperator* cmplex = NULL;
		//
		// Loop through all object, if it is an operator create pdfoperator else assume it is an operand
		//
		while (!o.isEOF()) 
		{
			if (o.isCmd ())
			{
				// Create operator
				boost::shared_ptr<PdfOperator> op =  createOp (string (o.getCmd ()), operands, 
																pdf, rf,
																cmplex);
				//
				// Put it either to operators or if it is a complex type, put it there
				//
				if (NULL == cmplex || cmplex == op.get())
				{
					if (!operators.empty())
					{
						operators.back ()->setNext (op);
						op->setPrev (operators.back ());
					}
					// Make it the last one
					operators.push_back (op);
				
				}else
				{
					cmplex->putBehind (op);
				}
				
				assert (operands.empty());
				if (!operands.empty ())
					throw MalformedFormatExeption ("CContentStream::CContentStream() Operands left on stack in pdf content stream after operator.");
					
			}else
			{
				shared_ptr<IProperty> pIp (createObjFromXpdfObj (o));
				operands.push_back (pIp);
			}

			// free it else memory leak
			o.free ();
			// grab the next object
			parser->getObj(&o);
		}
	}
	


	
//==========================================================
} // namespace
//==========================================================


//
//
//
CContentStream::CContentStream (shared_ptr<IProperty> stream, Object* obj)
{
	// not implemented yet
	assert (obj != NULL);
	if (pStream != stream->getType() || NULL == obj)
		throw CObjInvalidObject (); 
	printDbg (DBG_DBG, "Creating content stream.");

	// Check if stream is in a pdf, if not is NOT IMPLEMENTED
	// the problem is with parsed pdfoperators
	assert (NULL != stream->getPdf ());
	
	// Save stream
	contentstreams.push_back (stream);

	// Parse it into small objects
	parseContentStream (operators, *obj, *(stream->getPdf ()), stream->getIndiRef());
}

//
// Parse the xpdf object, representing the content stream
//
CContentStream::CContentStream (ContentStreams& streams, Object* obj)
{
	// not implemented yet
	assert (obj != NULL);
	if (NULL == obj)
		throw CObjInvalidObject (); 
	for (ContentStreams::iterator it = streams.begin(); it != streams.end (); ++it)
	{
		if (pStream != (*it)->getType())
			throw CObjInvalidObject (); 
		// Check if stream is in a pdf, if not is NOT IMPLEMENTED
		// the problem is with parsed pdfoperators
		assert (NULL != (*it)->getPdf());
	}
	printDbg (DBG_DBG, "Creating content stream.");
	
	// Save content streams
	copy (streams.begin(), streams.end(), back_inserter (contentstreams));

	// Parse it into small objects
	parseContentStream (operators, *obj, *(streams.front()->getPdf ()), streams.front()->getIndiRef ());
}

//
// 
//
void
CContentStream::getStringRepresentation (string& str) const
{
	printDbg (DBG_DBG, " ()");
	string frst, lst, tmp;

	str.clear ();
	for (Operators::const_iterator it = operators.begin (); it != operators.end(); ++it)
	{
			
		(*it)->getOperatorName (frst, lst);
		printDbg (DBG_DBG, "Operator name: " << frst << " " << lst << " param count: " << (*it)->getParametersCount() );
		
		(*it)->getStringRepresentation (tmp);
		str += tmp + "\n";
		tmp.clear ();
	}
}

//==========================================================
} // namespace pdfobjects
//==========================================================
