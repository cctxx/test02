#pragma once

#include "Runtime/Math/Vector3.h"
#include <map>
#include <vector>


class InspectorState 
{
public:
	InspectorState ();
	~InspectorState();

	void SetFloat (const std::string& key, float value);
	void SetInt (const std::string& key, int value);
	void SetString (const std::string& key, const std::string& value);
	void SetBool (const std::string& key, bool value);
	void SetVector3 (const std::string& key, Vector3f value);
	void SetIntVector (const std::string& key, std::vector<int>& value); 

	float GetFloat (const std::string& key, float defaultValue);
	int GetInt (const std::string& key, int defaultValue);
	std::string GetString (const std::string& identifier, const std::string& defaultValue);
	bool GetBool (const std::string& identifier, bool defaultValue);
	Vector3f GetVector3 (const std::string& identifier, const Vector3f& defaultValue);
	std::vector<int> GetIntVector (const std::string& key, std::vector<int>& defaultValue);

	void EraseFloat (const std::string& key);
	void EraseInt (const std::string& key);
	void EraseString (const std::string& key);
	void EraseBool (const std::string& key);
	void EraseVector3 (const std::string& key);
	void EraseIntVector (const std::string& key);

private:
	typedef std::map<std::string, float> FloatMap;
	typedef std::map<std::string, int> IntMap;
	typedef std::map<std::string, std::string> StringMap;
	typedef std::map<std::string, Vector3f> Vector3Map;
	typedef std::map<std::string, std::vector<int> > IntVectorMap;
	
	FloatMap m_FloatMap;
	IntMap m_IntMap;
	StringMap m_StringMap;
	Vector3Map m_Vector3Map;
	IntVectorMap m_IntVectorMap;
};
