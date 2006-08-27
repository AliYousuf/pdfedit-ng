#ifndef __OPERATORHINT_H__
#define __OPERATORHINT_H__

#include <operatorhinter.h>

class QString;

namespace gui {

using namespace configuration;

/**
 OperatorHint - class providing wrapper around single OperatorHinter
 instance and managing its configuration
 \brief Mode controller wrapper
*/
class OperatorHint {
public:
 OperatorHinter* get();
 static OperatorHint* getInstance();
 QString hint(const QString &operatorName);
 ~OperatorHint();
private:
 OperatorHint();
};

} // namespace gui

#endif