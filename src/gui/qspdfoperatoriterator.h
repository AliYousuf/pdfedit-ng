#ifndef __QSPDFOPERATORITERATOR_H__
#define __QSPDFOPERATORITERATOR_H__

#include <qstring.h>
#include <qobject.h>
#include <pdfoperators.h>
#include <ccontentstream.h>
#include "qscobject.h"

namespace gui {

class Base;
class QSContentStream;
class QSPdfOperator;

using namespace pdfobjects;

/*= This type of object represents pdf operator in content stream */
class QSPdfOperatorIterator : public QSCObject {
 Q_OBJECT
public:
 QSPdfOperatorIterator(PdfOperator::Iterator *op,boost::shared_ptr<CContentStream> cs,Base *_base);
 QSPdfOperatorIterator(boost::shared_ptr<PdfOperator> op,Base *_base);
 QSPdfOperatorIterator(boost::shared_ptr<PdfOperator> op,boost::shared_ptr<CContentStream> cs,Base *_base);
 virtual ~QSPdfOperatorIterator();
 PdfOperator::Iterator* get();
 boost::shared_ptr<PdfOperator> getCurrent();
public slots:
 /*- Returns current operator from this iterator */
 QSPdfOperator* current();
 /*- Create and return copy of this iterator, initially pointing to the same item */
 QSPdfOperatorIterator* copy();
 /*- Move the iterator to next operator */
 void next();
 /*- Move the iterator to previous operator */
 void prev();
 /*-
  Return content stream in which the initial operator used to construct the iterator was contained.
  May return NULL, if operator is not contained in any content stream or if content stream is not known at time of creation
 */
 QSContentStream* stream();
 /*- Return true, if we are at the end of the operator list (either beginning or end) */
 bool isEnd();
protected:
 PdfOperator::Iterator* copyIterator(PdfOperator::Iterator *src);
private:
 /** Object held in class*/
 PdfOperator::Iterator *obj;
 /** Reference to content stream that is holding the original operator used to construct the iterator. It may be NULL (empty shared_ptr) if unknown or empty */
 boost::shared_ptr<CContentStream> csRef;
};

} // namespace gui

#endif