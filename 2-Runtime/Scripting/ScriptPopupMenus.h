#ifndef __SCRIPTPOPUPMENUSH__
#define __SCRIPTPOPUPMENUSH__

#include <map>

class TypeTree;
class MonoBehaviour;

void BuildScriptPopupMenus (MonoBehaviour& behaviour, std::map<std::string, std::map<std::string, int> >& popups);
const std::string GetFieldIdentifierForEnum(const TypeTree* typeTree);
#endif