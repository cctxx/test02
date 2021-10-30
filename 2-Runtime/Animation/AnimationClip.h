#ifndef ANIMATIONCLIP_H
#define ANIMATIONCLIP_H

#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/Serialize/SerializeTraits.h"
#include "Runtime/Utilities/LinkedList.h"
#include "Runtime/Math/Quaternion.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Geometry/AABB.h"
#include "Runtime/Math/AnimationCurve.h"
#include "AnimationEvent.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/mecanim/memory.h"
#include "AnimationClipBindings.h"
#include "PPtrKeyframes.h"

#include "Motion.h"

#if UNITY_EDITOR
#include "AnimationClipSettings.h"
#endif 

namespace mecanim
{ 
	namespace animation
	{ 
		struct ClipMuscleConstant; 
		struct ClipMuscleInput;
	}
}

struct AnimationClipStats;

namespace Unity { class GameObject; }
using namespace Unity;

class BaseAnimationTrack;
class NewAnimationTrack;
class MonoScript;
class Animation;
class AnimationState;
class CompressedAnimationCurve;

/*
	TODO:
	* We currently don't handle double cover operator automatically for rotation curves
	* We are not synchronizing animation state cached range correctly
	* GetCurve is not implemented yet
*/

class AnimationClip : public Motion
{
public:	
	struct QuaternionCurve
	{
		UnityStr path;
		AnimationCurveQuat curve;
		int            hash;
		
		QuaternionCurve () { hash = 0; }
		void CopyWithoutCurve(QuaternionCurve& other) const
		{
			other.path = path;
			other.hash = hash;
		}
		
		DECLARE_SERIALIZE (QuaternionCurve)
	};

	struct Vector3Curve
	{
		UnityStr path;
		AnimationCurveVec3 curve;
		int            hash;
		
		Vector3Curve () { hash = 0; }
		void CopyWithoutCurve(Vector3Curve& other) const
		{
			other.path = path;
			other.hash = hash;
		}
		
		DECLARE_SERIALIZE (Vector3Curve)
	};
	
public:

	
	struct PPtrCurve
	{
		UnityStr        path;
		UnityStr        attribute;
		int             classID;
		MonoScriptPPtr  script;
		PPtrKeyframes   curve;
		
		PPtrCurve () {  }
		
		DECLARE_SERIALIZE (PPtrCurve)
	};
	
	struct FloatCurve
	{
		UnityStr path;
		UnityStr attribute;
		int    classID;
		MonoScriptPPtr script;
		AnimationCurve curve;
		int            hash;
		
		FloatCurve () { hash = 0; }
		void CopyWithoutCurve(FloatCurve& other) const
		{
			other.path = path;
			other.attribute = attribute;
			other.classID = classID;
			other.script = script;
			other.hash = hash;
		}
		
		DECLARE_SERIALIZE (FloatCurve)
	};

	typedef UNITY_VECTOR(kMemAnimation, QuaternionCurve) QuaternionCurves;
	typedef UNITY_VECTOR(kMemAnimation, CompressedAnimationCurve) CompressedQuaternionCurves;
	typedef UNITY_VECTOR(kMemAnimation, Vector3Curve) Vector3Curves;
	typedef UNITY_VECTOR(kMemAnimation, FloatCurve) FloatCurves;
	typedef UNITY_VECTOR(kMemAnimation, PPtrCurve) PPtrCurves;
	typedef UNITY_VECTOR(kMemAnimation, AnimationEvent) Events;

	enum AnimationType
	{
		kLegacy   = 1,
		kGeneric  = 2,
		kHumanoid = 3
	};
	
	
	
	REGISTER_DERIVED_CLASS (AnimationClip, Motion)
	DECLARE_OBJECT_SERIALIZE (AnimationClip)
	
	static void InitializeClass ();
	static void CleanupClass () {  }
	
	AnimationClip (MemLabelId label, ObjectCreationMode mode);	
	// virtual ~AnimationClip(); declared-by-macro

	virtual void AwakeFromLoad(AwakeFromLoadMode mode);
	virtual void CheckConsistency();
	
	
	/// Assigns curve to the curve defined by path, classID and attribute
	/// If curve is null the exisiting curve will be removed.
	void SetCurve (const std::string& path, int classID, MonoScriptPPtr script, const std::string& attribute, AnimationCurve* curve, bool syncEditorCurves);
	bool GetCurve (const std::string& path, int classID, MonoScriptPPtr script, const std::string& attribute, AnimationCurve* outCurve);

	void ClearCurves ();
	void EnsureQuaternionContinuity ();

	bool HasAnimationEvents ();
	void FireAnimationEvents (float lastTime, float now, Unity::Component& source);
	
	#if UNITY_EDITOR
	void							SetEditorCurve (const std::string& path, int classID, MonoScriptPPtr script, const std::string& attribute, const AnimationCurve* curve, bool syncEditorCurves = true);
	bool							GetEditorCurve (const std::string& path, int classID, MonoScriptPPtr script, const std::string& attribute, AnimationCurve* outCurve);
	FloatCurves&					GetEditorCurvesSync();

	void							SetEditorPPtrCurve (const std::string& path, int classID, MonoScriptPPtr script, const std::string& attribute, PPtrKeyframes* keyframes);
	bool							GetEditorPPtrCurve (const std::string& path, int classID, MonoScriptPPtr script, const std::string& attribute, PPtrKeyframes* outKeyframes);
	PPtrCurves&						GetEditorPPtrCurves() { return m_PPtrCurves; }

	// Callback for telling animation window when an animclip got reloaded
	typedef void					OnAnimationClipAwake(AnimationClip* clip);
	static void						SetOnAnimationClipAwake (OnAnimationClipAwake *callback);

	void							SyncEditorCurves();	
	void							SyncMuscleCurvesBackwardCompatibility();
	
	void							SetEvents (const AnimationEvent* events, int size, bool sort = false);
	void							ClearEvents ();
	FloatCurves&					GetEulerEditorCurves()												{ return m_EulerEditorCurves; }
	virtual void					CloneAdditionalEditorProperties (Object& src);
	FloatCurves&					GetEditorCurvesNoConversion ()										{ return m_EditorCurves; } 
		
	
	const AnimationClipSettings&	GetAnimationClipSettings() const									{ return m_AnimationClipSettings; }
	void							SetAnimationClipSettingsNoDirty (AnimationClipSettings &clipInfo);
	void							SetAnimationClipSettings (const AnimationClipSettings&clipInfo)		{ m_AnimationClipSettings = clipInfo; SetDirty(); } 
	
	void							GenerateMuscleClip();		
		
	void							SetAnimationType (AnimationType type);
	
	
	// overloads from Motion	
	virtual float					GetAverageDuration();
	virtual float					GetAverageAngularSpeed();
	virtual Vector3f				GetAverageSpeed();
	virtual float					GetApparentSpeed();
	
	virtual bool					ValidateIfRetargetable(bool showWarning = true);	
	virtual bool					IsLooping();
		
	#endif

	virtual bool					IsAnimatorMotion()const;
	
	// Returns the smallest and largest keyframe time of any channel in the animation
	// if no keyframes are contained make_pair (infinity, -infinity) is returned
	std::pair<float, float> GetRange ();

	// Animation State support
	typedef List< ListNode<AnimationState> > AnimationStateList;
	typedef void DidModifyClipCallback(AnimationClip* clip, AnimationStateList& states);

	static void SetDidModifyClipCallback(DidModifyClipCallback* callback);
	void AddAnimationState(ListNode<AnimationState>& node) { m_AnimationStates.push_back(node); }

	void SetSampleRate (float s);
	float GetSampleRate () { return m_SampleRate; }

	void SetCompressionEnabled (bool s) { m_Compressed = s; }
	bool GetCompressionEnabled () { return m_Compressed; }
#if UNITY_EDITOR
	void SetUseHighQualityCurve (bool s) { m_UseHighQualityCurve = s; }
	bool GetUseHighQualityCurve ()const { return m_UseHighQualityCurve; }
#endif
	void SetWrapMode (int wrap) { m_WrapMode = wrap; SetDirty (); }
	int GetWrapMode () { return m_WrapMode; }

	Events& GetEvents () { return m_Events; }
	void AddRuntimeEvent (AnimationEvent& event);

	void AddRotationCurve (const AnimationCurveQuat& quat, const std::string& path);
	void AddPositionCurve (const AnimationCurveVec3& quat, const std::string& path);
	void AddScaleCurve (const AnimationCurveVec3& quat, const std::string& path);
	void AddFloatCurve (const AnimationCurve& curve, const std::string& path, int classID, const std::string& attribute);

	QuaternionCurves& GetRotationCurves() { return m_RotationCurves; } 
	Vector3Curves& GetPositionCurves() { return m_PositionCurves; } 
	Vector3Curves& GetScaleCurves() { return m_ScaleCurves; } 
	FloatCurves& GetFloatCurves() { return m_FloatCurves; } 

	const QuaternionCurves& GetRotationCurves() const { return m_RotationCurves; } 
	const Vector3Curves& GetPositionCurves() const { return m_PositionCurves; } 
	const Vector3Curves& GetScaleCurves() const { return m_ScaleCurves; } 
	const FloatCurves& GetFloatCurves() const { return m_FloatCurves; } 

	void SetBounds(const AABB& bounds) { m_Bounds = bounds; SetDirty(); }
	const AABB& GetBounds() const { return m_Bounds; }

	static void RevertAllPlaymodeAnimationEvents ();

	bool IsHumanMotion();

	AnimationType GetAnimationType () const { return m_AnimationType; }

	
	UnityEngine::Animation::AnimationClipBindingConstant* GetBindingConstant () { return &m_ClipBindingConstant; }
	mecanim::animation::ClipMuscleConstant*	GetRuntimeAsset();

	void GetStats(AnimationClipStats& stats);
	
	void ClipWasModifiedAndUpdateMuscleRange ();
	void UpdateMuscleClipRange ();

	
private:
	void CompressCurves (CompressedQuaternionCurves& compressedRotationCurves);
	void DecompressCurves (CompressedQuaternionCurves& compressedRotationCurves);

	void ClipWasModified (bool cleanupMecanimData = true);

	void ReloadEditorEulerCurves (const string& path);
	void ReloadEditorQuaternionCurves (const string& path);

	void ConvertToNewCurveFormat (NewAnimationTrack& curves, int classID, const std::string& path);
	void ConvertToNewCurveFormat ();

	void CleanupMecanimData();

	mecanim::memory::ChainedAllocator					m_ClipAllocator;

private:
	AnimationStateList m_AnimationStates;

	float           m_SampleRate;
	bool			m_Compressed;
	bool            m_UseHighQualityCurve;
	int             m_WrapMode;///< enum { Default = 0, Once = 1, Loop = 2, PingPong = 4, ClampForever = 8 }
	
	QuaternionCurves   m_RotationCurves;
	Vector3Curves      m_PositionCurves;
	Vector3Curves      m_ScaleCurves;
	FloatCurves        m_FloatCurves;
	PPtrCurves         m_PPtrCurves;
	Events             m_Events;
	AnimationType      m_AnimationType;

#if UNITY_EDITOR
	AnimationClipSettings m_AnimationClipSettings;
#endif//#if UNITY_EDITOR
	
	mecanim::animation::ClipMuscleConstant*		m_MuscleClip;
	mecanim::uint32_t							m_MuscleClipSize;
	UnityEngine::Animation::AnimationClipBindingConstant m_ClipBindingConstant;

	/// TODO: Serialiaze and do not compute it at all on startup
	std::pair<float, float> m_CachedRange;

	AABB m_Bounds;
	
	#if UNITY_EDITOR

	// Keep a copy of events setup from edit mode so we can safely revert events after playmode
	Events             m_EditModeEvents;
	FloatCurves        m_EditorCurves;
	FloatCurves        m_EulerEditorCurves;
		
	struct ChildTrack
	{
		UnityStr path;
		int classID;
		PPtr<BaseAnimationTrack> track;
		DECLARE_SERIALIZE (ChildTrack)
	};

	typedef vector_map<SInt32, PPtr<BaseAnimationTrack> > ClassIDToTrack;
	typedef ClassIDToTrack::iterator iterator;
	typedef std::vector<ChildTrack> ChildTracks;
	typedef ChildTracks::iterator child_iterator;

	ClassIDToTrack	m_ClassIDToTrack;
	ChildTracks     m_ChildTracks;
	
	friend class StripCurvesForMecanimClips;

	#endif
	
	friend class AnimationManager;
};

typedef std::vector<PPtr<AnimationClip> > AnimationClipVector;

#endif
