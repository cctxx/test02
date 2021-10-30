#pragma once

class IAnimationBinding
{
public:
#if UNITY_EDITOR
	virtual void		GetAllAnimatableProperties (Object& component, std::vector<EditorCurveBinding>& outProperties) const = 0;
#endif
	
	virtual float		GetFloatValue (const UnityEngine::Animation::BoundCurve& bound) const = 0;
	virtual void		SetFloatValue (const UnityEngine::Animation::BoundCurve& bound, float value) const = 0;
	
	virtual void		SetPPtrValue (const UnityEngine::Animation::BoundCurve& bound, SInt32 value) const= 0;
	virtual SInt32		GetPPtrValue (const UnityEngine::Animation::BoundCurve& bound) const = 0;
	
	virtual bool		GenerateBinding (const UnityStr& attribute, bool pptrCurve, UnityEngine::Animation::GenericBinding& outputBinding) const = 0;
	virtual ClassIDType	BindValue (Object& target, const UnityEngine::Animation::GenericBinding& binding, UnityEngine::Animation::BoundCurve& bound) const = 0;
	
	virtual std::string SerializedPropertyPathToCurveAttribute (Object& target, const char* propertyPath) const { return std::string(); }
	virtual std::string CurveAttributeToSerializedPath (const UnityEngine::Animation::BoundCurve& bound) const { return std::string(); }
};

inline const char* ParsePrefixedName (const char* attribute, const char* prefix)
{
	if (BeginsWith(attribute, prefix))
		return attribute + strlen(prefix);
	else
		return NULL;
}

inline void AddBinding (std::vector<EditorCurveBinding>& attributes, int classID, const std::string& attribute)
{
	attributes.push_back(EditorCurveBinding ("", classID, NULL, attribute, false));
}

inline void AddBindingCheckUnique (std::vector<EditorCurveBinding>& attributes, int startIndex, int classID, const std::string& attribute)
{
	for (int i=startIndex;i<attributes.size();i++)
	{
		if (attributes[i].classID == classID && attributes[i].attribute == attribute)
			return;
	}
	
	attributes.push_back(EditorCurveBinding ("", classID, NULL, attribute, false));
}


inline void AddPPtrBinding (std::vector<EditorCurveBinding>& attributes, int classID, const std::string& attribute)
{
	attributes.push_back(EditorCurveBinding ("", classID, NULL, attribute, true));
}

inline int ParseIndexAttributeIndex (const UnityStr& attribute, const char* preString)
{
	const std::string::size_type pos0 = attribute.find_first_of('[');
	const std::string::size_type pos1 = attribute.find_first_of(']');
	
	if (pos0 == std::string::npos || pos1 == std::string::npos ||
		!BeginsWith(attribute, preString))
	{
		return -1;
	}
	
	Assert(pos0 < pos1);
	Assert(pos1 - pos0 > 1);
	
	return StringToInt(attribute.c_str() + pos0);
}

