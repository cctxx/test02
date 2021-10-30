#include "Editor/Src/InspectorState.h"

typedef std::pair<std::string,float> FloatMapElement;
typedef std::pair<std::string,int> IntMapElement;
typedef std::pair<std::string,std::string> StringMapElement;
typedef std::pair<std::string,Vector3f> Vector3MapElement;
typedef std::pair<std::string,std::vector<int> > IntVectorElement;


InspectorState::InspectorState ()
{
}

InspectorState::~InspectorState()
{
}

// SETTERS

void InspectorState::SetFloat (const std::string& key, float value)
{
	FloatMap::iterator it = m_FloatMap.find (key);
	if (it != m_FloatMap.end())
		it->second = value;
	else
		m_FloatMap.insert(FloatMapElement(key,value));
}

void InspectorState::SetInt (const std::string& key, int value)
{
	IntMap::iterator it = m_IntMap.find (key);
	if (it != m_IntMap.end())
		it->second = value;
	else
		m_IntMap.insert(IntMapElement(key,value));
}

void InspectorState::SetString (const std::string& key, const std::string& value)
{
	StringMap::iterator it = m_StringMap.find (key);
	if (it != m_StringMap.end())
		it->second = value;
	else
		m_StringMap.insert(StringMapElement(key,value));
}

void InspectorState::SetBool (const std::string& key, bool value)
{
	SetInt (key, value ? 1 : 0);
}

void InspectorState::SetVector3 (const std::string& key, Vector3f value)
{
	Vector3Map::iterator it = m_Vector3Map.find (key);
	if (it != m_Vector3Map.end())
		it->second = value;
	else
		m_Vector3Map.insert(Vector3MapElement(key,value));
}

void InspectorState::SetIntVector (const std::string& key, std::vector<int>& value)
{
	IntVectorMap::iterator it = m_IntVectorMap.find (key);
	if (it != m_IntVectorMap.end ())
		it->second.swap (value);
	else
		m_IntVectorMap.insert (IntVectorElement (key, value));
}


// GETTERS

float InspectorState::GetFloat (const std::string& key, float defaultValue)
{
	FloatMap::iterator it = m_FloatMap.find (key);
	if (it != m_FloatMap.end())
		return it->second;
	else
		return defaultValue;
}

int InspectorState::GetInt (const std::string& key, int defaultValue)
{
	IntMap::iterator it = m_IntMap.find (key);
	if (it != m_IntMap.end())
		return it->second;
	else
		return defaultValue;
}

std::string InspectorState::GetString (const std::string& key, const std::string& defaultValue)
{
	StringMap::iterator it = m_StringMap.find (key);
	if (it != m_StringMap.end())
		return it->second;
	else
		return defaultValue;
}

bool InspectorState::GetBool (const std::string& key, bool defaultValue)
{
	int v = GetInt (key, defaultValue ? 1 : 0);
	return v == 0 ? false : true;
}

Vector3f InspectorState::GetVector3 (const std::string& key, const Vector3f& defaultValue)
{
	Vector3Map::iterator it = m_Vector3Map.find (key);
	if (it != m_Vector3Map.end())
		return it->second;
	else
		return defaultValue;
}

std::vector<int> InspectorState::GetIntVector (const std::string& key, std::vector<int>& defaultValue)
{
	IntVectorMap::iterator it = m_IntVectorMap.find (key);
	if (it != m_IntVectorMap.end())
		return it->second;
	else
		return defaultValue;
}


// ERASERS

void InspectorState::EraseFloat (const std::string& key)
{
	m_FloatMap.erase (key);
}

void InspectorState::EraseInt (const std::string& key)
{
	m_IntMap.erase (key);
}

void InspectorState::EraseString (const std::string& key)
{
	m_StringMap.erase (key);
}

void InspectorState::EraseBool (const std::string& key)
{
	EraseInt (key);
}

void InspectorState::EraseVector3 (const std::string& key)
{
	m_Vector3Map.erase (key);
}

void InspectorState::EraseIntVector (const std::string& key)
{
	m_IntVectorMap.erase (key);
}
