/** @file
 TreeItemPdf - class holding CPDF in tree, descendant of QListViewItem
*/

#include "treeitempdf.h"
#include "treeitem.h"
#include "treedata.h"
#include "treewindow.h"
#include "treeitempage.h"
#include <cobject.h>
#include <cpdf.h>
#include <cpage.h>
#include <qobject.h>

namespace gui {

using namespace std;
using namespace pdfobjects;

/** constructor of TreeItemPdf - create root item from given object
 @param _data TreeData containing necessary information about tree in which this item will be inserted
 @param parent QListView in which to put item
 @param _pdf CPdf Object contained in this item
 @param name Name of this item - will be shown in treeview
 @param after Item after which this one will be inserted
 */
TreeItemPdf::TreeItemPdf(TreeData *_data,CPdf *_pdf,QListView *parent,const QString name/*=QString::null*/,QListViewItem *after/*=NULL*/):TreeItemAbstract(parent,after) {
 data=_data;
 init(_pdf,name);
}

/** constructor of TreeItemPdf - create child item from given object
 @param _data TreeData containing necessary information about tree in which this item will be inserted
 @param parent QListViewItem which is parent of this object
 @param _pdf CPdf Object contained in this item
 @param name Name of file - will be shown in treeview
 @param after Item after which this one will be inserted
 */
TreeItemPdf::TreeItemPdf(TreeData *_data,CPdf *_pdf,QListViewItem *parent,const QString name/*=QString::null*/,QListViewItem *after/*=NULL*/):TreeItemAbstract(parent,after) {
 data=_data;
 init(_pdf,name);
}

/** constructor of TreeItemPdf - create special child item of TreeItemPdf
 @param _data TreeData containing necessary information about tree in which this item will be inserted
 @param parent QListViewItem which is parent of this object
 @param name Name (type) of this item - will be shown in treeview
 @param after Item after which this one will be inserted
 */
TreeItemPdf::TreeItemPdf(TreeData *_data,TreeItemPdf *parent,const QString name,QListViewItem *after/*=NULL*/):TreeItemAbstract(parent,after) {
 data=_data;
 initSpec(parent->getObject(),name);
}

/** Initialize item from given CPdf object
 @param pdf CPdf used to initialize this item
 @param name Name of this item - will be shown in treeview (usually name of PDF file)
 */
void TreeItemPdf::init(CPdf *pdf,const QString &name) {
 obj=pdf;
 // object name
 if (name.isNull()) {
  setText(0,QObject::tr("<no name>"));
 } else {
  setText(0,name);
 }
 reload(false); //Add all subchilds, etc ...
}

/** reload itself */
void TreeItemPdf::reloadSelf() {
 if (nType.isNull()) { ///Not special type
  // object type
  setText(1,QObject::tr("PDF"));
  // Page count
  setText(2,QString::number(obj->getPageCount())+QObject::tr(" page(s)"));
 }
}

/** Initialize special PDF subitem from given CPdf object and its name (which defines also type of this item)
 @param pdf CPdf used to initialize this item
 @param name Name of this item - will be shown in treeview
 */
void TreeItemPdf::initSpec(CPdf *pdf,const QString &name) {
 obj=pdf;
 // object name
 if (name.isNull()) {
  setText(0,QObject::tr("<no name>"));
 } else {
  setText(0,QObject::tr(name));
 }
 // object type
 setText(1,QObject::tr("List"));
 nType=name;//Set node type
 reload(false);//Add all childs
}

/** return CPdf stored inside this item
 @return stored object (CPdf) */
CPdf* TreeItemPdf::getObject() {
 return obj;
}

/** default destructor */
TreeItemPdf::~TreeItemPdf() {
}

/** Create child */
TreeItemAbstract* TreeItemPdf::createChild(const QString &name,QListViewItem *after/*=NULL*/) {
 if (nType.isNull()) { //Childs under PDF document
  if (name=="Dict") { //get Dictionary
   return new TreeItem(data,this,obj->getDictionary().get(),QObject::tr("Dictionary"));
  }
  if (name=="Pages") { //get Page list
   return new TreeItemPdf(data,this,QT_TRANSLATE_NOOP("gui::TreeItemPdf","Pages"),after); 
  }
  if (name=="Outlines") { //get Outline List
   return new TreeItemPdf(data,this,QT_TRANSLATE_NOOP("gui::TreeItemPdf","Outlines"),after); 
  }
  assert(0);//Unknown
  return NULL;
 }
 if (nType=="Pages") { //Pages - get page given its number
  //name = Page number
  unsigned int i=name.toUInt();
  printDbg(debug::DBG_DBG,"Adding page by reload() - " << i);
  CPage *page=obj->getPage(i).get();
  return new TreeItemPage(data,page,this,name,after);
 }
 if (nType=="Outlines") { //Outlines - get specific outline
  //TODO: implement
  return NULL;
 }
 assert(0);//Unknown
 return NULL;
}
/** Return list of child names */
QStringList TreeItemPdf::getChildNames() {
 if (nType.isNull()) {//PDF document
  QStringList items;
  items += "Dict";
  items += "Pages";
  items += "Outlines";
  return items;
 }
 if (nType=="Pages") {
  unsigned int count=obj->getPageCount();
  QStringList items;
  for(unsigned int i=1;i<=count;i++) { //Add all pages
   items += QString::number(i);
  }
  return items;
 }
 if (nType=="Outlines") {
  //TODO : implement outlines
  return QStringList();
 }
 assert(0); //Should not happen
 return QStringList();
}

} // namespace gui
