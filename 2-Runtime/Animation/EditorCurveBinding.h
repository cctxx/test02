#pragma once

class MonoScript;

struct EditorCurveBinding
{
	std::string  path;
	std::string  attribute; ///@TODO: Rename to propertyName, thats how it is called in C#
	int          classID;
	MonoScript*  script;
	bool         isPPtrCurve;
	
	EditorCurveBinding ()
	{
		script = NULL;
		isPPtrCurve = false;
		classID = 0;
	}
	
	
	EditorCurveBinding (const std::string& inPath, int inClassID, MonoScript* inScript, const std::string& inAttribute, bool inIsPPtrCurve)
	{
		path = inPath;
		classID = inClassID;
		script = inScript;
		attribute = inAttribute;
		isPPtrCurve = inIsPPtrCurve;
	}
	
	friend bool operator == (const EditorCurveBinding& lhs, const EditorCurveBinding& rhs)
	{
		return lhs.path == rhs.path && lhs.attribute == rhs.attribute && lhs.classID == rhs.classID && lhs.script == rhs.script && lhs.isPPtrCurve == rhs.isPPtrCurve;
	}
};
