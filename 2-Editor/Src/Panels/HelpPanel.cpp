#include "UnityPrefix.h"
#include "HelpPanel.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/File.h"
#include "Editor/Src/EditorHelper.h"
#include <string>
#include <map>

const char* kComponentDocumentation = "Documentation/Components";
const char* kMonoScriptReference = "file:///unity/ScriptReference/MonoBehaviour.html";
const char* kBaseDocumentation = "Documentation";

string FindHelpNamed (const char* topic);
static bool HasNamedHelp (const char *topic);


static bool HasNamedHelp (const char *topic)
{
	if (!topic)
		return false;
	return !FindHelpNamed (topic).empty();
}


const std::string HelpFileNameForObject (Object *obj)
{
	std::string name = obj->GetClassName ();
	if (name == "MonoBehaviour") {
		MonoBehaviour *beh = reinterpret_cast<MonoBehaviour *> (obj);
		MonoScript *script = beh->GetScript ();
		if (script)
			return "script-"+(std::string)script->GetScriptClassName ();
		else 
			return string ();
	}
	return "class-" + name;
}

std::string GetNiceHelpNameForObject (Object *obj, bool defaultToMonoBehaviour)
{
	std::string name = HelpFileNameForObject (obj);
	if (!defaultToMonoBehaviour || HasNamedHelp (name.c_str()))
	{
		int pos = name.find ('-');
		if (pos >= 0)
			return name.substr (pos+1);
	}
	else if (defaultToMonoBehaviour)
	{
		string className = obj->GetClassName ();
		if (className == "MonoBehaviour" || className == "MonoScript")
			return "MonoBehaviour";
	}
	return string ();
}

bool HasHelpForObject (Object *obj, bool defaultToMonoBehaviour)
{
	if (obj == NULL)
		return false;

	// Lookup with script name
	if (HasNamedHelp (HelpFileNameForObject (obj).c_str()))
		return true;
	
	// Fall back to monobehaviour script reference
	string className = obj->GetClassName ();
	if (defaultToMonoBehaviour && (className == "MonoBehaviour" || className == "MonoScript"))
		return HasNamedHelp (kMonoScriptReference);
	else
		return false;
}

void ShowHelpForObject (Object *obj)
{
	if (obj == NULL)
		return;
		
	// Lookup with script name
	if (HasNamedHelp (HelpFileNameForObject (obj).c_str()))
		ShowNamedHelp (HelpFileNameForObject (obj).c_str());
	else
	{
		string className = obj->GetClassName ();
		if (className == "MonoBehaviour" || className == "MonoScript")
			ShowNamedHelp (kMonoScriptReference);
	}
}

static inline unsigned char Unhex(char c)
{
	if( c >= '0' && c <= '9')
		return c-'0';
	if( c >= 'a' && c <= 'f')
		return c-'a'+10;
	if( c >= 'A' && c <= 'F')
		return c-'A'+10;
	return 0;
}

static string UnescapeHTMLPath (const std::string& path)
{
	string result;
	size_t n = path.size();
	result.reserve(n);
	for( size_t i = 0; i < n; /**/ ) {
		if( path[i] == '%' && i+2 < n ) {
			char decoded = ToLower(char((Unhex(path[i+1])<<4) | Unhex(path[i+2])));
			result += decoded;
			i += 3;
		} else {
			result += path[i];
			++i;
		}
	}
	return result;
}

string FindHelpNamed (const char* topic)
{
	static std::map<string, string> s_Cache;

	std::map<string, string>::iterator i = s_Cache.find (topic);
	if (i != s_Cache.end())
		return i->second;
	
	string path;
	string topicName;

	// resolve file:///unity/ absoulte URIs	
	if (!strncmp ("file:///unity/", topic, 14))
	{
		std::string folder = GetApplicationContentsPath();
		folder = AppendPathName(folder, GetDocumentationRelativeFolder ());
		path = AppendPathName(folder, kBaseDocumentation);
		topicName = &topic[14];
	}
	// if topic starts with a file:// assume absolute fileref. 
	else if (!strncmp ("file://", topic, 7))
	{
		topicName = &topic[7];
	}
	// Relative Component reference
	else
	{
		std::string folder = GetApplicationContentsPath();
		folder = AppendPathName(folder, GetDocumentationRelativeFolder ());
		path = AppendPathName(folder, kComponentDocumentation);
		topicName = topic;
	}

	#if UNITY_WIN
	topicName = UnescapeHTMLPath(topicName);
	#endif

	std::string fullPath = AppendPathName(path, topicName);

	if (GetPathNameExtension(fullPath) != "html") 
		fullPath = AppendPathNameExtension(fullPath, "html");
	
	bool result = IsFileCreated(fullPath);
	if (!result)
		fullPath.clear();
	
	s_Cache[topic] = fullPath;
	
	return fullPath;
}

string GetDocumentationRelativeFolder ()
{
	if( !IsDeveloperBuild() )
	{
		return "Documentation";
	}
	else
	{
		#if UNITY_WIN
		return "../../UserDocumentation/ActualDocumentation";
		#else
		return "../../../UserDocumentation/ActualDocumentation";
		#endif
	}
}

void ShowNamedHelp (const char* topic)
{
	string path = FindHelpNamed (topic);
	if (!path.empty())
		OpenPathInWebbrowser( path );
}
