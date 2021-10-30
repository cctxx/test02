#pragma once

/// Do we have a help file for this object?
/// If the opbject is a monobehaviour, we check to see if we have a help file named "script-CLASSNAME"
/// Otherwise, we check the objects class and see if we have a help file names "class-CLASSNAME"
bool HasHelpForObject (Object *obj, bool defaultToMonoBehaviour);
std::string GetNiceHelpNameForObject (Object *obj, bool defaultToMonoBehaviour);
void ShowHelpForObject (Object *obj);

void ShowNamedHelp (const char *topic);
std::string FindHelpNamed (const char* topic);

std::string GetDocumentationRelativeFolder ();