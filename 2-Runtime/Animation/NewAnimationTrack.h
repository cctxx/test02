
/// BACKWARDS COMPATIBILITY ONLY
#if UNITY_EDITOR
#ifndef NEWANIMATIONTRACK_H
#define NEWANIMATIONTRACK_H

#include "BaseAnimationTrack.h"
#include "Runtime/Math/AnimationCurve.h"

/// The animationTrack2 class is currently specialized to handling only 
/// transformcomponents. It will be expanded to animate any data that is serialized.

class NewAnimationTrack : public BaseAnimationTrack
{
	public:
	
	REGISTER_DERIVED_CLASS (NewAnimationTrack, BaseAnimationTrack)
	DECLARE_OBJECT_SERIALIZE (NewAnimationTrack)
	
	NewAnimationTrack (MemLabelId label, ObjectCreationMode mode);
	// ~NewAnimationTrack (); declared-by-macro
	/// The attributename is generated from the serialization system
	/// and is the path to the property. "." is used as the path seperator.
	/// It is supposed to be similar how you access the variable in C++
	/// A transformcomponent that transfers a Vector3 using TRANSFER (m_LocalPosition)
	/// and a Vector3 that transfers using TRANSFER (x)
	/// would have a attribute name "m_LocalPosition.x"
/*	
	/// Returns the curve for an attribute.
	Channel* GetChannel (const std::string& attributeName);
	int GetChannelCount () { m_Curves.size (); }
	Channel& GetChannelAtIndex (int index) { return m_Curves[index]; }
*/
	
	/// Inserts a curve for an attribute always returns true
//	bool InsertCurve (const std::string& attributeName, const AnimationCurve& curve);
//	bool RemoveCurve (const std::string& attributeName);
	AnimationCurve* GetCurve (const std::string& attributeName);
	std::vector<std::string> GetCurves ();
	
//	virtual void SampleAnimation (Object& o, float time, int /*wrapmode*/);
//	virtual std::pair<float, float> GetRange () const;
//	virtual std::pair<float, float> GetPlayableRange () const;
//	int GetAnimationClassID () const { return m_ClassID; }
//	void SetAnimationClassID (int classID) { m_ClassID = classID; }

	struct Channel
	{
		int byteOffset;
//		int type;
		AnimationCurve curve;
		UnityStr attributeName;
		
		bool operator == (const string& name) { return attributeName == name; }
		
		DECLARE_SERIALIZE (Channel)
	};
	
	/// Returns whether the animation system supports the value!
	/// if src and f are non-null extracts the value into f.
//	static bool ExtractFloatValue (Object* src, const TypeTree* value, float* f);
	typedef std::vector<Channel> Curves;
		
	Curves m_Curves;
	int m_ClassID;
//	int m_Optimizations;
	
//	void RebuildByteOffsets ();
	
	friend class Animation;
};

#endif
#endif
