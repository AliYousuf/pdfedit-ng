/** @file
 SelfDestructiveWidget - widget class that will destruct
 when appropriate signal from killer window is sent<br>
 Killer window can be any window, but must emit signal
 "selfDestruct()" for this widget to work<br>
 Used for helper dialogs that should close when file they are editing is also closed<br>
 @author Martin Petricek
*/

#include "selfdestructivewidget.h"

namespace gui {

/**
 Default constructor of Self destructive widget
 @param killer Window that will destruct this widget
 @param parent parent widget (passed to QWidget)
 @param name name of this widget (passed to QWidget)
 @param f Widget flags (passed to QWidget)
 */
SelfDestructiveWidget::SelfDestructiveWidget(QWidget *killer,QWidget *parent/*=0*/, const char *name/*=0*/, WFlags f/*=0*/) 
 : QWidget(parent,name,f) {
QObject::connect(killer,SIGNAL(selfDestruct()),this,SLOT(close()));
}

/** default destructor */
SelfDestructiveWidget::~SelfDestructiveWidget() {
}

} // namespace gui