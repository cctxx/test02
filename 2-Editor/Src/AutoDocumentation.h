#ifndef AUTODOCUMENTATION_H
#define AUTODOCUMENTATION_H

#include <map>
#include <string>

std::string GetVariableDocumentation (const std::string& className, const std::string& variableName);
const std::map<std::string, int>* GetVariableParameter (const std::string& className, const std::string& variableName);
std::pair<float, float> GetRangeFromDocumentation (const std::string& className, const std::string& variableName);
const char* GetVariableDisplayName (const std::string& className, const std::string& variableName);
void InitializeAutoDocumentation ();
void CleanupAutoDocumentation();

#endif
