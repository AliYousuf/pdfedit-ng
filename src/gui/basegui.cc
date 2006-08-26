/** @file
 GUI Base - class containing extra functionalyty presen only in GUI
 @author Martin Petricek
*/

#include "aboutwindow.h"
#include "basegui.h"
#include "colortool.h"
#include "commandwindow.h"
#include "consolewritergui.h"
#include "dialog.h"
#include "edittool.h"
#include "helpwindow.h"
#include "menu.h"
#include "mergeform.h"
#include "multitreewindow.h"
#include "numbertool.h"
#include "optionwindow.h"
#include "pagespace.h"
#include "pdfeditwindow.h"
#include "propertyeditor.h"
#include "qscobject.h"
#include "qsimporter.h"
#include "qsiproperty.h"
#include "qsmenu.h"
#include "qspage.h"
#include "qspdf.h"
#include "qstreeitem.h"
#include "selecttool.h"
#include "settings.h"
#include "statusbar.h"
#include "treeitemabstract.h"
#include "util.h"
#include "version.h"
#include <string.h>
#include <qdir.h>
#include <qfile.h>
#include <qmessagebox.h>
#include <qsplitter.h>
#include <utils/debug.h>

namespace gui {

using namespace util;

/**
 Create new Base class 
 @param parent Parent editor window containing this class
*/
BaseGUI::BaseGUI(PdfEditWindow *parent) : Base() {
 w=parent;
 import->addQSObj(w->pagespc,"PageSpace");
 import->addQSObj(w->cmdLine,"CommandWindow");

 //Console writer;
 consoleWriter=new ConsoleWriterGui(parent->cmdLine);
 setConWriter(consoleWriter);
}

/** destructor */
BaseGUI::~BaseGUI() {
 delete consoleWriter;
}

/**
 Run all initscripts.
 Gets name of initscripts from settings
*/
void BaseGUI::runInitScript() {
 QStringList initScripts=globalSettings->readPath("init","script/");
 int scriptsRun=runScriptList(initScripts);
 //Run list of initscripts from settings
 if (!scriptsRun) {
  //No init scripts found - print a warning
  warn(gui::Base::tr("No init script found - check your configuration")+"!\n"+gui::Base::tr("Looked for","scripts")+":\n"+initScripts.join("\n"));
 }
 // Run initscripts from paths listed in settings,
 // initscript with same name is executed only once,
 // initscripts in later paths take priority
 QStringList initScriptPaths=globalSettings->readPath("init_path","script/");
 runScriptsFromPath(initScriptPaths);
}

/**
 slot called when any of tools (color tool, edit tool, etc ...) change it's value
*/
void BaseGUI::toolChangeValue(const QString &toolName) {
 guiPrintDbg(debug::DBG_DBG,"tool change: " << toolName);
 //TODO: properly escape the name of tool
 call("onValueChange","'"+toolName+"'");
}

/**
 Add color selection tool to list of "known color selection tools"
 @param tool Tool to add
*/
void BaseGUI::addColorTool(ColorTool *tool) {
 colorPickers.insert(tool->getName(),tool);
 connect(tool,SIGNAL(clicked(const QString &)),this,SLOT(toolChangeValue(const QString &)));
}

/**
 Add edit tool to list of "known edit tools"
 @param tool Tool to add
*/
void BaseGUI::addEditTool(EditTool *tool) {
 editTools.insert(tool->getName(),tool);
 connect(tool,SIGNAL(clicked(const QString &)),this,SLOT(toolChangeValue(const QString &)));
}

/**
 Add number tool to list of "known number tools"
 @param tool Tool to add
*/
void BaseGUI::addNumberTool(NumberTool *tool) {
 numberTools.insert(tool->getName(),tool);
 connect(tool,SIGNAL(clicked(const QString &)),this,SLOT(toolChangeValue(const QString &)));
}

/**
 Add select tool to list of "known select tools"
 @param tool Tool to add
*/
void BaseGUI::addSelectTool(SelectTool *tool) {
 selectTools.insert(tool->getName(),tool);
 connect(tool,SIGNAL(clicked(const QString &)),this,SLOT(toolChangeValue(const QString &)));
}

/**
 For given name return widget represented by that name
 @param widgetName Widget name
 @return Pointer to widget, or NULL if no widget matches the name
*/
QWidget* BaseGUI::getWidgetByName(const QString &widgetName) {
 QString widget=widgetName.lower();
 if (widget=="commandline") return w->cmdLine;
 if (widget=="statusbar") return w->status;
 if (widget=="propertyeditor") return w->prop;
 if (widget=="rightside") return w->splProp;
 if (widget=="tree") return w->tree;
 //Widget not found ...
 return NULL;
}

/**
 Function to be run before the script is executed
 @param script Script code;
 @param callback is it callback from script?
*/
void BaseGUI::preRun(const QString &script,bool callback/*=false*/) {
 Base::preRun(script,callback);
 if (callback) return;
 /*
  Commit currently edited property in property editor
  This must be done this way, because by clicking on some button in toolbar to perform some action,
  for example reloading a page, does not cause the property to lose focus and update automatically
 */
 w->prop->commitProperty();
}

/**
 Function to be run after the script is executed
*/
void BaseGUI::postRun() {
 Base::postRun();
 if (treeReloadFlag) {
  //Reload the tree
  w->tree->reload();
  treeReloadFlag=false;
 }
}

/**
 Removes objects added with addScriptingObjects
 \see addScriptingObjects
 */
void BaseGUI::removeScriptingObjects() {
 Base::removeScriptingObjects();
}

/**
 Create objects that should be available to scripting from current CPdf and related objects
 \see removeScriptingObjects
*/
void BaseGUI::addScriptingObjects() {
 Base::addScriptingObjects();
}

/**
 Callback from main window when the treeitem got just deleted
 This will look for script wrappers containing the mentioned item and invalidate them, so they will not cause a crash
 @param theItem deleted tree item
*/
void BaseGUI::treeItemDeleted(TreeItemAbstract* theItem) {
 //Get dictionary with all wrappers for given treeitem
 QPtrDict<void>* pDict=treeWrap[theItem];
 if (!pDict) {
  //No wrapper exists. Done.
  //guiPrintDbg(debug::DBG_DBG,"Item deleted that is not in wrapper"); 
  return;
 }
 //Ok, now disable all wrappers pointing to this item

 // For each wrapper
 QPtrDictIterator<void> it(*pDict);
 for(;it.current();++it) {
  QSTreeItem* theWrap=reinterpret_cast<QSTreeItem*>(it.currentKey());
  guiPrintDbg(debug::DBG_DBG,"Disabling wrapper " << (intptr_t)theWrap << " w. item "<< (intptr_t)theItem); 
  assert(theWrap);
  guiPrintDbg(debug::DBG_DBG,"Check type: " << theWrap->type()); 
  //Disable the wrapper, so calling it will not result in crash, but some error/exception instead
  theWrap->disable();
  guiPrintDbg(debug::DBG_DBG,"Disabled wrapper"); 
 }
 //Remove reference to subdictionary from dictionary
 treeWrap.remove(theItem);
 //Autodelete is on, so the inner dictionary will be deleted ....
}

// === Scripting functions ===

/** Shows "About" window */
void BaseGUI::about() {
 AboutWindow *aboutWin= new AboutWindow(w);
 aboutWin->show();
}

/** 
 Show dialog for adding objects into given container.<br>
 Container must be Dict or Array, otherwise the dialog is not created.
 If Container is NULL, currently selected object ihn property editor will be attempted to use as container..<br>
 After creating, dialog is shown and usable and this function immediately returns.<br>
 @param container Dict or Array into which objects will be added
*/
void BaseGUI::addObjectDialog(QSIProperty *container/*=NULL*/) {
 if (container) {
  w->addObjectDialogI(container->get());
 } else {
  w->addObjectDialogI(w->selectedProperty);
 }
}

/** \copydoc addObjectDialog(QSIProperty *) */
void BaseGUI::addObjectDialog(QObject *container) {
 //This method is fix for QSA Bug: QSA sometimes degrade QObject descendants to bare QObjects
 QSIProperty *_container=dynamic_cast<QSIProperty*>(container);
 if (_container) {
  w->addObjectDialogI(_container->get());
 } else {
  guiPrintDbg(debug::DBG_ERR,"type Error: " << container->className());
  w->addObjectDialogI(w->selectedProperty);
 }
}

/** \copydoc Menu::checkByName */
void BaseGUI::checkItem(const QString &name,bool check) {
 w->menuSystem->checkByName(name,check);
}

/** \copydoc Menu::showByName */
void BaseGUI::showItem(const QString &name,bool show) {
 w->menuSystem->showByName(name,show);
}

/** \copydoc PdfEditWindow::exitApp */
void BaseGUI::closeAll() {
 w->exitApp();
}

/** \copydoc PdfEditWindow::closeFile */
bool BaseGUI::closeFile(bool askSave,bool onlyAsk/*=false*/) {
 return w->closeFile(askSave,onlyAsk);
}

/**
 \copydoc Menu::createItem(const QString &,const QString &,const QString &,const QString &,const QString &,const QString &,const QStringList &)
*/ 
void BaseGUI::createMenuItem(const QString &parentName,const QString &name,const QString &caption,const QString &action,const QString &accel/*=QString::null*/,const QString &icon/*=QString::null*/,const QStringList &classes/*=QStringList()*/) {
 try {
  w->menuSystem->createItem(parentName,name,caption,action,accel,icon,classes);
 } catch (...) {
  //Silently ignore any exception
 }
}

/** create new empty editor window and display it */
void BaseGUI::createNewWindow() {
 PdfEditWindow::create();
}

/** \copydoc Menu::enableByName */
void BaseGUI::enableItem(const QString &name,bool enable) {
 w->menuSystem->enableByName(name,enable);
}

/** \copydoc PdfEditWindow::filename() */
QString BaseGUI::filename() {
 return w->filename();
}

/**
 Show "open file" dialog and return file selected, or NULL if dialog was cancelled
 @return name of selected file.
 */
QString BaseGUI::fileOpenDialog() {
 guiPrintDbg(debug::DBG_DBG,"fileOpenDialog");
 return openFileDialogPdf(w);
}

/**
 Show "save file" dialog and return file selected, or NULL if dialog was cancelled
 @param oldName Old name of the file (if known) - will be preselected
 @return name of selected file.
 */
QString BaseGUI::fileSaveDialog(const QString &oldName/*=QString::null*/) {
 guiPrintDbg(debug::DBG_DBG,"fileSaveDialog");
 QString ret=saveFileDialogPdf(w,oldName);
 return ret;
}

/**
 Get color from color picker with given name
 @param colorName Name of color picker
 @return Color from the picker or empty Variant if color picker is not found
*/
QVariant BaseGUI::getColor(const QString &colorName) {
 //Check if we have the picker
 if (!colorPickers.contains(colorName)) return QVariant();
 ColorTool *pick=colorPickers[colorName];
 assert(pick);
 return QVariant(pick->getColor());
}

/**
 Get text from text edit box or select text box with given name
 @param textName Name of text edit box
 @return text in the edit box or QString::null if edit box is not found
*/
QString BaseGUI::getEditText(const QString &textName) {
 //Check if we have the tool as edit text
 if (editTools.contains(textName)) {
  EditTool *pick=editTools[textName];
  assert(pick);
  return pick->getText();
 }
 //Check if we have the tool as select text
 if (selectTools.contains(textName)) {
  SelectTool *pick=selectTools[textName];
  assert(pick);
  return pick->getText();
 }
 return QString::null;
}

/**
 Get number from number edit box with given name
 @param name Name of edit box
 @return number in the edit box or 0 if given edit box is not found
*/
double BaseGUI::getNumber(const QString &name) {
 //Check if we have the tool
 if (!numberTools.contains(name)) return 0;
 NumberTool *pick=numberTools[name];
 assert(pick);
 return pick->getNum();
}

/**
 Invokes program help. Optional parameter is topic - if invalid or not defined, help title page will be invoked
 @param topic Starting help topic
*/
void BaseGUI::help(const QString &topic/*=QString::null*/) {
 HelpWindow *hb=new HelpWindow(topic);
 hb->show();
}

/**
 Check if a widget is visible
 \see getWidgetByName
 @param widgetName name of widget
 @return True if widget is visible, false if not
*/
bool BaseGUI::isVisible(const QString &widgetName) {
 QWidget *w=getWidgetByName(widgetName);
 if (!w) return false;
 return w->isVisible();
}

/**
 Brings up informational messagebox with given message
 @param msg Message to display
 */
void BaseGUI::message(const QString &msg) {
 QMessageBox::information(w,APP_NAME,msg,QObject::tr("&Ok"),QString::null,QString::null,QMessageBox::Ok | QMessageBox::Default);
}

/**
 Brings up "merge documents" dialog and return the results

 Result is array of three elements:
 First element is array with page numbers
 Second element is array with page positions
 Third is filename of the document to be merged in
 @return Result of merge or empty variant if dialog was cancelled
*/
QVariant BaseGUI::mergeDialog() {
 QVariant retValue;
 CPdf *doc=qpdf->get();
 if (!doc) {
  //No document opened at all
  errorException("","mergeDialog",tr("No document opened"));
  return retValue;
 }
 // Create dialog instance
 MergeDialog *dialog=new MergeDialog();
 // Inits original document (one which is currently opened) with its page count.
 dialog->initOriginal(doc->getPageCount());
 // Starts dialog as modal and does something if OK was pressed
 if(dialog->exec()==QDialog::Accepted) {
  // gets result of merging operation
  MergeArray<int> * result=dialog->getResult();
  // if result length is 0 - there is nothing to merge
  int n=result->getLength();
  if(n>0) {
   QValueList<QVariant> rItems;
   QValueList<QVariant> rPos;
   // result->getItems() returns an array of pages to be merged
   // with current document
   int *res_items=result->getItems();
   // result->getPositions() returns an array of positions for
   // those pages
   size_t *res_pos=result->getPositions();
   for (int i=0;i<n;i++) {
    rItems.append(res_items[i]);
    rPos.append(res_pos[i]);
   }
   QValueList<QVariant> res;
   res.append(rItems);
   res.append(rPos);   
   res.append(dialog->fileName());   
   retValue=QVariant(res);
  }
  // result cleanup
  delete result;
 }
 // dialog cleanup
 dialog->destroyOpenFile();
 delete dialog;
 return retValue;
}

/** \copydoc PdfEditWindow::modified */
bool BaseGUI::modified() {
 return w->modified();
}

/** \copydoc PdfEditWindow::openFile */
bool BaseGUI::openFile(const QString &name) {
 return w->openFile(name);
}

/**
 Open file in a new editor window.
 @param name Name of file to open
*/
void BaseGUI::openFileNew(const QString &name) {
 PdfEditWindow::create(name);
}

/** Show options dialog. Does not wait for dialog to finish. */
void BaseGUI::options() {
 OptionWindow::optionsDialog(w->menuSystem);
}

/**
 Return number of currently shown page
 @return page number
*/
int BaseGUI::pageNumber() {
 return w->selectedPageNumber;
}

/**
 Return currently shown page
 @return page
*/
QSPage* BaseGUI::page() {
 if (w->selectedPage.get()) {
  //REturn selected page
  return new QSPage(w->selectedPage,this);
 } else {
  //No page selected currently
  return NULL;
 }
}

/**
 Invoke dialog to select color.
 Last selected color is remembered and offered as default next time.
 The 'initial default color' is red
 @return selected color, or invalid color if the dialog was cancelled
*/
QVariant BaseGUI::pickColor() {
 QColor ret=colorDialog(w);
 if (!ret.isValid()) {
  return QVariant();
  guiPrintDbg(debug::DBG_DBG,"Color is not valid");
 }
 return QVariant(ret);
}

/**
 Create and return new popup menu, build from menu list/item  identified by this name.
 If no item specified, menu is initially empty
 @param menuName Name of menu inconfiguration to use as template
 @return initialized QSMenu Object
*/
QSMenu* BaseGUI::popupMenu(const QString &menuName/*=QString::null*/) {
 return new QSMenu(w->menuSystem,this,menuName);
}

/**
 Asks question with Yes/No answer. "Yes" is default.
 Return true if user selected "yes", false if user selected "no"
 @param msg Question to display
 @return True if yes, false if no
*/
bool BaseGUI::question(const QString &msg) {
 return questionDialog(w,msg);
}

/**
 Asks question with Yes/No/Cancel answer. "Yes" is default. <br>
 Return one of these three according to user selection: <br>
  Yes : return 1     <br>
  No : return 0      <br>
  Cancel : return -1
 @param msg Question to display
 @return Selection of user.
 */
int BaseGUI::question_ync(const QString &msg) {
 int answer=QMessageBox::question(w,APP_NAME,msg,QObject::tr("&Yes"),QObject::tr("&No"),QObject::tr("&Cancel"),0,2);
 // QMessageBox::Yes | QMessageBox::Default,QMessageBox::No, QMessageBox::Cancel | QMessageBox::Escape
 switch (answer) {
  case 0: return 1;	//QMessageBox::Yes
  case 1: return 0;	//QMessageBox::No
  case 2: return -1;	//QMessageBox::Cancel
  default: assert(0);
 }
}

/** \copydoc PdfEditWindow::restoreWindowState */
void BaseGUI::restoreWindowState() {
 w->restoreWindowState();
}

/**
 Save currently edited document to disk
 @return true if saved succesfully, false if failed to save because of any reason
 */
bool BaseGUI::save() {
 return w->save();
}

/** \copydoc PdfEditWindow::saveCopy */
bool BaseGUI::saveCopy(const QString &name) {
 return w->saveCopy(name);
}

/**
 Save currently edited document to disk, while creating a new revisiion
 @return true if saved succesfully, false if failed to save because of any reason
 */
bool BaseGUI::saveRevision() {
 return w->save(true);
}

/** \copydoc PdfEditWindow::saveWindowState */
void BaseGUI::saveWindowState() {
 w->saveWindowState();
}

/**
 Set color of color picker with given name
 @param colorName Name of color picker
 @param newColor New color to set
*/
void BaseGUI::setColor(const QString &colorName,const QVariant &newColor) {
 //Check if we have the picker
 if (!colorPickers.contains(colorName)) return;
 ColorTool *pick=colorPickers[colorName];
 assert(pick);
 QColor col=newColor.toColor();
 if (!col.isValid()) return;
 pick->setColor(col);
}

/**
 Set text of text edit box with given name
 @param textName Name of text edit box
 @param newText New text to set
*/
void BaseGUI::setEditText(const QString &textName,const QString &newText) {
 //Check if we have the tool as edit text
 if (editTools.contains(textName)) {
  EditTool *pick=editTools[textName];
  assert(pick);
  pick->setText(newText);
  return;
 }
 //Check if we have the tool as select text
 if (selectTools.contains(textName)) {
  SelectTool *pick=selectTools[textName];
  assert(pick);
  pick->setText(newText);
  return;
 }
 //Nope. Nothing
 return;
}

/**
 Set number in number edit box with given name
 @param name Name of edit box
 @param number New number to set
*/
void BaseGUI::setNumber(const QString &name,double number) {
 //Check if we have the tool
 if (!numberTools.contains(name)) return;
 NumberTool *pick=numberTools[name];
 assert(pick);
 pick->setNum(number);
}

/**
 Set predefined values to choose from for number edit box or select text box with given name.
 For number edit box, the user is still able to type its own numbers.
 @param name Name of edit box
 @param predefs List of predefined numbers, separated with commas
*/
void BaseGUI::setPredefs(const QString &name,const QString &predefs) {
 //Check if we have the tool as number tool
 if (numberTools.contains(name)) {
  NumberTool *pick=numberTools[name];
  assert(pick);
  pick->setPredefs(predefs);
  return;
 }
 //Check if we have the tool as select text
 if (selectTools.contains(name)) {
  SelectTool *pick=selectTools[name];
  assert(pick);
  pick->setPredefs(predefs);
  return;
 }
 //Nope. Nothing
 return;
}

/**
 Set predefined values to choose from for number edit box or select text box with given name.
 For number edit box, the user is still able to type its own numbers.
 @param name Name of edit box
 @param predefs List of predefined numbers, separated with commas
*/
void BaseGUI::setPredefs(const QString &name,const QStringList &predefs) {
 //Check if we have the tool as number tool
 if (numberTools.contains(name)) {
  NumberTool *pick=numberTools[name];
  assert(pick);
  pick->setPredefs(predefs);
  return;
 }
 //Check if we have the tool as select text
 if (selectTools.contains(name)) {
  SelectTool *pick=selectTools[name];
  assert(pick);
  pick->setPredefs(predefs);
  return;
 }
 //Nope. Nothing
 return;
}

/**
 Change active revision in current PDF document
 @param revision number of revision to activate
 */
void BaseGUI::setRevision(int revision) {
 w->changeRevision(revision);
}

/**
 Set widget to be either visible or invisible
 \see getWidgetByName
 @param widgetName name of widget
 @param visible action to be performed (true = show, false = hide)
*/
void BaseGUI::setVisible(const QString &widgetName, bool visible) {
 QWidget *w=getWidgetByName(widgetName);
 if (!w) return;
 if (visible) w->show(); else w-> hide();
}

/**
 Return root item of currently selected tree
 @return Current tree root item
*/
QSTreeItem* BaseGUI::treeRoot() {
 return dynamic_cast<QSTreeItem*>(import->createQSObject(w->tree->root()));
}

/**
 Return root item of main tree
 @return Main tree root item
*/
QSTreeItem* BaseGUI::treeRootMain() {
 return dynamic_cast<QSTreeItem*>(import->createQSObject(w->tree->rootMain()));
}

/**
 Show this string as a warning in a messagebox and also print it to console, followed by newline
 @param str String to use as warning
 */
void BaseGUI::warn(const QString &str) {
 conPrintLine(str);
 QMessageBox::warning(w,gui::Base::tr("Warning"),str);
}

// Tree-selection related slots

/**
 Return first selected tree item from specified tree
 Return NULL, if no item is selected
 @param name Name of tree to get selection from
 @return Selected tree item
*/
QSTreeItem* BaseGUI::firstSelectedItem(const QString &name/*=QString::null*/) {
 return dynamic_cast<QSTreeItem*>(import->createQSObject(w->tree->getSelectedItem(name)));
}

/**
 Return next selected tree item from specified tree
 Return NULL, if no next item is selected
 @return Selected tree item
*/
QSTreeItem* BaseGUI::nextSelectedItem() {
 return dynamic_cast<QSTreeItem*>(import->createQSObject(w->tree->nextSelectedItem()));
}

/**
 Return object held in first selected tree item from specified tree
 Return NULL, if no item is selected
 @param name Name of tree to get selection from
 @return Selected tree item's object
*/
QSCObject* BaseGUI::firstSelected(const QString &name/*=QString::null*/) {
 return w->tree->getSelected(name);
}

/**
 Return object held in  next selected tree item from specified tree
 Return NULL, if no next item is selected
 @return Selected tree item's object
*/
QSCObject* BaseGUI::nextSelected() {
 return w->tree->nextSelected();
}

QProgressBar * BaseGUI::progressBar()
{
        return w->getProgressBar();
}

// === Non-scripting slots ===

#ifdef DRAGDROP

/**
 Invoked when dragging one item to another within the same tree window
 @param source Source (dragged) item
 @param target Target item
*/
void BaseGUI::_dragDrop(TreeItemAbstract *source,TreeItemAbstract *target) {
 //Import items
 QSCObject *src=import->createQSObject(source);
 QSCObject *tgt=import->createQSObject(target);
 import->addQSObj(src,"source");
 import->addQSObj(tgt,"target");
 call("onDragDrop");
 //delete page and item variables from script -> they may change while script is not executing
 deleteVariable("source");
 deleteVariable("target");
}

/**
 Invoked when dragging one item to another in different tree window (possibly in different document)
 @param source Source (dragged) item
 @param target Target item
*/
void BaseGUI::_dragDropOther(TreeItemAbstract *source,TreeItemAbstract *target) {
 /*
  More complicated, if the script keeps something from second tree
  and the second document is closed, the editor may crash.
  We need to make local copy of source's content
 */
 QSCObject *src=source->getQSObject(this);//Rebased object. May be NULL if rebase not possible
 QSCObject *tgt=import->createQSObject(target);
 import->addQSObj(src,"source");
 import->addQSObj(tgt,"target");
 call("onDragDropOther");
 deleteVariable("source");
 deleteVariable("target");
}

#endif

} // namespace gui
