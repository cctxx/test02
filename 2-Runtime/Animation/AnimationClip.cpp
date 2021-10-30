#include "UnityPrefix.h"
#include "Runtime/Math/AnimationCurve.h"
#include "AnimationClip.h"
#include "AnimationBinder.h"
#include "NewAnimationTrack.h"
#include "AnimationCurveUtility.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Utilities/algorithm_utility.h"
#include <limits>
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Filters/Mesh/CompressedMesh.h"
#include "Runtime/mecanim/generic/stringtable.h"
#include "Runtime/mecanim/generic/crc32.h"
#include "Runtime/Serialize/Blobification/BlobWrite.h"
#include "Runtime/Serialize/SerializeUtility.h"
#include "GenericAnimationBindingCache.h"
#include "AnimationClipStats.h"
#include "MecanimClipBuilder.h"
#include "MecanimUtility.h"



#if UNITY_EDITOR
#include "KeyframeReducer.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#endif

using namespace UnityEngine::Animation;

static AnimationClip::DidModifyClipCallback* gDidModifyClipCallback = NULL;

#if UNITY_EDITOR
static AnimationClip::OnAnimationClipAwake* gOnAnimationClipAwake = NULL;
void AnimationClip::SetOnAnimationClipAwake (OnAnimationClipAwake* callback)
{
	gOnAnimationClipAwake = callback;
}

#endif

using namespace std;

AnimationClip::AnimationClip(MemLabelId label, ObjectCreationMode mode)
: Super(label, mode), 
m_Bounds(Vector3f::zero, Vector3f::zero), 
m_AnimationType(kLegacy),
m_MuscleClipSize(0),
m_MuscleClip(0),
m_UseHighQualityCurve(true),
m_ClipAllocator(4*1024)
{
	m_SampleRate = 60.0F;
	m_Compressed = false;
	m_WrapMode = 0;
}

void AnimationClip::AwakeFromLoad(AwakeFromLoadMode mode)
{
	Super::AwakeFromLoad(mode);
	#if UNITY_EDITOR
	
	ConvertToNewCurveFormat();
	if (gOnAnimationClipAwake)
		gOnAnimationClipAwake (this);
	
	
	// Generate muscle clip data from editor curves (Assetbundles already have a muscle clip baked in, we dont want to regenerate that)
 	if ((mode & kDidLoadFromDisk) && GetRuntimeAsset() != NULL)
	{
		// case 476382, need to populate m_AnimationClipSettings manually because it is not transfer for game release asset
		CstToAnimationClipSettings(m_MuscleClip, m_AnimationClipSettings);
	}
	// When loading scenes etc it's a good idea not to have the hiccups all on first access
	else if ((mode & kDidLoadFromDisk) && GetRuntimeAsset() == NULL)
		GenerateMuscleClip();
	else
		CleanupMecanimData();
	
	#endif

	ClipWasModified (false);
}

void AnimationClip::ClipWasModifiedAndUpdateMuscleRange ()
{
	ClipWasModified();
	UpdateMuscleClipRange ();
}

void AnimationClip::UpdateMuscleClipRange ()
{
#if UNITY_EDITOR
	m_CachedRange = make_pair (std::numeric_limits<float>::infinity (), -std::numeric_limits<float>::infinity ());
	pair<float,float> range = GetRange();

	AnimationClipSettings settings = GetAnimationClipSettings ();
	settings.m_StartTime = 0.0F;
	settings.m_StopTime = range.second;

	SetAnimationClipSettingsNoDirty(settings);
#endif	
}

void AnimationClip::SetSampleRate (float s)
{
	m_SampleRate = s;
	// Stop time depends on the sample rate
	// because we add an extra frame at the end to cover pptr curves
	UpdateMuscleClipRange ();
	SetDirty();
}

void AnimationClip::CheckConsistency ()
{
	Super::CheckConsistency();

	if(kLegacy > m_AnimationType || m_AnimationType > kHumanoid)
	{
		CleanupMecanimData();
		m_AnimationType = kLegacy;
	}
}

void AnimationClip::ClipWasModified (bool cleanupMecanimData)
{
	if (cleanupMecanimData)
		CleanupMecanimData();
	
	NotifyObjectUsers(kDidModifyMotion);
	
	m_CachedRange = make_pair (std::numeric_limits<float>::infinity (), -std::numeric_limits<float>::infinity ());
	gDidModifyClipCallback (this, m_AnimationStates);
}

void AnimationClip::ClearCurves ()
{
	m_RotationCurves.clear();
	m_PositionCurves.clear();
	m_ScaleCurves.clear();
	m_FloatCurves.clear();
	m_PPtrCurves.clear();
	#if UNITY_EDITOR
	m_EditorCurves.clear();
	m_EulerEditorCurves.clear();
	#endif

	ClipWasModifiedAndUpdateMuscleRange ();
	SetDirty();
}

void AnimationClip::EnsureQuaternionContinuity ()
{
	for (QuaternionCurves::iterator i=m_RotationCurves.begin();i != m_RotationCurves.end();i++)
		::EnsureQuaternionContinuityAndRecalculateSlope(i->curve);
		
	gDidModifyClipCallback (this, m_AnimationStates);
	SetDirty();
}

bool AnimationClip::HasAnimationEvents ()
{
	return m_Events.size() > 0;
}

void  AnimationClip::FireAnimationEvents (float lastTime, float now, Unity::Component& source)
{
	AnimationClip::Events& events = GetEvents();
	Assert(!events.empty());
	
	if (lastTime == now)
		return;
	
	///@TODO: 
	// * AnimationEvents with blendWeight = 0 will not be fired (In a blendtree they should always be fired, irregardless of blendweight)

	
	int eventCount = events.size();

	// Simple forward playback.
	if (lastTime < now)
	{
		// Special case for first frame in the clip, when just playing a simple clip
		// We want to make sure the event on the first frame will be fired.
		// (We can't have this in the general codepath otherwise we end up firing events twice, 
		// if the event matches the time exactly)
		if (lastTime == 0.0F && events[0].time == 0.0F)
			FireEvent(events[0], 0, source);
		
		// Play all events that 
		for (int eventIter = 0; eventIter < eventCount; eventIter++)
		{
			if (lastTime < events[eventIter].time && now >= events[eventIter].time)
				FireEvent(events[eventIter], 0, source);
		}	
	}
	// Looping
	// - Play all events to the end of the clip
	// - then from first event up to now
	else
	{
		for (int eventIter = 0; eventIter < eventCount; eventIter++)
		{
			if (lastTime < events[eventIter].time)
				FireEvent(events[eventIter], 0, source);
		}	

		for (int eventIter = 0; eventIter < eventCount; eventIter++)
		{
			if (now > events[eventIter].time)
				FireEvent(events[eventIter], 0, source);
		}	
	}
}

#if UNITY_EDITOR
void AnimationClip::ReloadEditorEulerCurves (const string& path)	
{
	SET_ALLOC_OWNER(this);
	// Find the euler curves we will use to build the quaternion curves
	AnimationCurve* curves[4] = { NULL, NULL, NULL, NULL };
	for (FloatCurves::iterator i=m_EditorCurves.begin();i != m_EditorCurves.end();i++)
	{
		if (BeginsWith(i->attribute, "m_LocalRotation") && i->path == path)
		{
			char last = i->attribute[i->attribute.size()-1];
			if (last == 'x')
				curves[0] = &i->curve;
			else if (last == 'y')
				curves[1] = &i->curve;
			else if (last == 'z')
				curves[2] = &i->curve;
			else if (last == 'w')
				curves[3] = &i->curve;
			else
			{
				ErrorString("Can't set curve because " + i->attribute + " is not a valid Transform property.");
				continue;
			}
		}
	}
	
	// Remove existing euler curves at path
	for (int i=0;i != m_EulerEditorCurves.size();i++)
	{
		if (m_EulerEditorCurves[i].classID == ClassID(Transform) && m_EulerEditorCurves[i].path == path)
		{
			m_EulerEditorCurves[i] = m_EulerEditorCurves.back();
			m_EulerEditorCurves.resize(m_EulerEditorCurves.size() - 1);
			i--;
		}
	}
	
	if (curves[0] && curves[1] && curves[2] && curves[3])
	{
		// Create combined quaternion curve
		AnimationCurveQuat quatCurve;
		CombineCurve (*curves[0], 0, quatCurve);
		CombineCurve (*curves[1], 1, quatCurve);
		CombineCurve (*curves[2], 2, quatCurve);
		CombineCurve (*curves[3], 3, quatCurve);

		// Create euler curves
		FloatCurve curve;
		curve.path = path;
		curve.classID = ClassID(Transform);
		int first = m_EulerEditorCurves.size();
		curve.attribute = "localEulerAngles.x";
		m_EulerEditorCurves.push_back(curve);
		curve.attribute = "localEulerAngles.y";
		m_EulerEditorCurves.push_back(curve);	
		curve.attribute = "localEulerAngles.z";
		m_EulerEditorCurves.push_back(curve);
		AnimationCurve* eulerCurves[3] = { &m_EulerEditorCurves[first].curve, &m_EulerEditorCurves[first+1].curve, &m_EulerEditorCurves[first+2].curve };	
		
		QuaternionCurveToEulerCurve(quatCurve, eulerCurves);
	}
}

void AnimationClip::ReloadEditorQuaternionCurves (const string& path)	
{
	SET_ALLOC_OWNER(this);
	int bakedEulerCurves = -1;
	
	// Find the euler curves we will use to build the quaternion curves
	AnimationCurve* curves[3] = { NULL, NULL, NULL };
	for (FloatCurves::iterator i=m_EulerEditorCurves.begin();i != m_EulerEditorCurves.end();i++)
	{
		char last = i->attribute[i->attribute.size()-1];

		if (i->path == path)
		{
			DebugAssertIf(!BeginsWith(i->attribute, "localEulerAngles") && !BeginsWith(i->attribute, "localEulerAnglesBaked"));
			int curBaked = BeginsWith(i->attribute, "localEulerAnglesBaked");
			if (bakedEulerCurves != -1 && bakedEulerCurves != curBaked)
			{
				ErrorString("localEulerAnglesBaked and localEulerAngles exist at the same time. Please ensure that there is always only one curve type in use on a single transform.");
				continue;
			}
				
			bakedEulerCurves = curBaked;
			
			if (last == 'x')
				curves[0] = &i->curve;
			else if (last == 'y')
				curves[1] = &i->curve;
			else if (last == 'z')
				curves[2] = &i->curve;
			else
			{
				ErrorString("Can't set curve because " + i->attribute + " is not a valid Transform property.");
				continue;
			}
		}
	}

	// Remove existing quaternion curves at path
	for (int i=0;i != m_EditorCurves.size();i++)
	{
		if (m_EditorCurves[i].classID == ClassID(Transform) && BeginsWith(m_EditorCurves[i].attribute, "m_LocalRotation") && m_EditorCurves[i].path == path)
		{
			m_EditorCurves[i] = m_EditorCurves.back();
			m_EditorCurves.resize(m_EditorCurves.size() - 1);
			i--;
		}
	}

	// If all 3 euler curves exist, add the quaternion editor curve
	if (curves[0] && curves[1] && curves[2])
	{
		// Create temporary quaternion curve
		AnimationCurveQuat quatCurve;
		
		// TODO : these both should use tangents or fitting
		if (bakedEulerCurves)
			EulerToQuaternionCurveBake(*curves[0], *curves[1], *curves[2], quatCurve, GetSampleRate());
		else
			EulerToQuaternionCurve(*curves[0], *curves[1], *curves[2], quatCurve);
		
		// Create quaternion editor curves from single quaternion curve
		FloatCurve curve;
		curve.path = path;
		curve.classID = ClassID(Transform);

		int first = m_EditorCurves.size();
		curve.attribute = "m_LocalRotation.x";
		m_EditorCurves.push_back(curve);
		curve.attribute = "m_LocalRotation.y";
		m_EditorCurves.push_back(curve);	
		curve.attribute = "m_LocalRotation.z";
		m_EditorCurves.push_back(curve);	
		curve.attribute = "m_LocalRotation.w";
		m_EditorCurves.push_back(curve);
		
		AnimationCurve* quaternionCurves[4] = { &m_EditorCurves[first].curve, &m_EditorCurves[first+1].curve, &m_EditorCurves[first+2].curve, &m_EditorCurves[first+3].curve };
		ExpandQuaternionCurve(quatCurve, quaternionCurves);
	}
}

void AnimationClip::SetEditorCurve (const string& path, int classID, MonoScriptPPtr script, const std::string& attribute, const AnimationCurve* curve, bool syncEditorCurves)
{
	SET_ALLOC_OWNER(this);
	GetEditorCurvesSync();
	
	FloatCurves* curveArray = &m_EditorCurves;
	if (classID == ClassID (Transform) && BeginsWith (attribute, "localEulerAngles"))
		curveArray = &m_EulerEditorCurves;

	// Find existing curve
	FloatCurves::iterator i;
	for (i=curveArray->begin();i != curveArray->end();i++)
	{
		if (i->classID == classID && i->path == path && i->attribute == attribute && i->script == script)
			break;
	}
		
	// Shall we remove a curve?
	if (curve == NULL)
	{
		if (i != curveArray->end ())
		{
			curveArray->erase(i);
			
			// Modified euler angle curve, reload editor quaternion curves
			if (classID == ClassID (Transform) && (BeginsWith (attribute, "localEulerAngles")) )
				ReloadEditorQuaternionCurves(path);
			// Modified quaternion curve, reload editor euler angle curves
			else if (classID == ClassID (Transform) && BeginsWith (attribute, "m_LocalRotation"))
				ReloadEditorEulerCurves(path);
			
			if (syncEditorCurves)
				SyncEditorCurves();
		}
		return;
	}

	// Add or replace the curve
	AnimationCurve* comboCurve = i != curveArray->end() ? &i->curve : NULL;
	if (comboCurve == NULL)
	{
		curveArray->push_back(FloatCurve());
		curveArray->back().path = path;
		curveArray->back().attribute = attribute;
		curveArray->back().classID = classID;
		curveArray->back().script = script;
		curveArray->back().curve = *curve;
	}	
	else
		*comboCurve = *curve;
	
	// Modified euler angle curve, reload editor quaternion curves
	if (classID == ClassID (Transform) && (BeginsWith (attribute, "localEulerAngles")))
		ReloadEditorQuaternionCurves(path);
	// Modified quaternion curve, reload editor euler angle curves
	else if (classID == ClassID (Transform) && BeginsWith (attribute, "m_LocalRotation"))
		ReloadEditorEulerCurves(path);

 	SyncEditorCurves();
}

bool AnimationClip::GetEditorCurve (const string& path, int classID, MonoScriptPPtr script, const std::string& attribute, AnimationCurve* curve)
{
	GetEditorCurvesSync();

	// Find existing curve
	FloatCurves::iterator i;
	for (i=m_EditorCurves.begin();i != m_EditorCurves.end();i++)
	{
		if (i->classID == classID && i->path == path && i->attribute == attribute && i->script == script)
		{
			if (curve != NULL)
			*curve = i->curve;
			return true;
		}
	}
	
	// Find existing curve in euler editor curves array
	for (i=m_EulerEditorCurves.begin();i != m_EulerEditorCurves.end();i++)
	{
		if (i->classID == classID && i->path == path && i->attribute == attribute && i->script == script)
		{
			if (curve != NULL)
			*curve = i->curve;
			return true;
		}
	}
	
	return false;
}

AnimationClip::FloatCurves& AnimationClip::GetEditorCurvesSync ()
{
	SET_ALLOC_OWNER(this);
	if (m_EditorCurves.empty() && m_EulerEditorCurves.empty())
	{
		m_EulerEditorCurves.clear();
		m_EditorCurves = m_FloatCurves;
		
		for (QuaternionCurves::iterator i=m_RotationCurves.begin ();i != m_RotationCurves.end ();i++)
		{
			FloatCurve curve;
			curve.path = i->path;
			curve.classID = ClassID(Transform);
			AssertMsg(i->curve.GetKeyCount() >= 2, "Key count: %d on curve '%s'", i->curve.GetKeyCount(), i->path.c_str());
			
			// Create quaternion curves
			int first = m_EditorCurves.size();
			curve.attribute = "m_LocalRotation.x";
			m_EditorCurves.push_back(curve);
			curve.attribute = "m_LocalRotation.y";
			m_EditorCurves.push_back(curve);	
			curve.attribute = "m_LocalRotation.z";
			m_EditorCurves.push_back(curve);	
			curve.attribute = "m_LocalRotation.w";
			m_EditorCurves.push_back(curve);
			
			AnimationCurve* curves[4] = { &m_EditorCurves[first].curve, &m_EditorCurves[first+1].curve, &m_EditorCurves[first+2].curve, &m_EditorCurves[first+3].curve };
			ExpandQuaternionCurve(i->curve, curves);
			
			// Create euler curves
			first = m_EulerEditorCurves.size();
			curve.attribute = "localEulerAngles.x";
			m_EulerEditorCurves.push_back(curve);
			curve.attribute = "localEulerAngles.y";
			m_EulerEditorCurves.push_back(curve);	
			curve.attribute = "localEulerAngles.z";
			m_EulerEditorCurves.push_back(curve);
			
			AnimationCurve* eulerCurves[3] = { &m_EulerEditorCurves[first].curve, &m_EulerEditorCurves[first+1].curve, &m_EulerEditorCurves[first+2].curve };
			QuaternionCurveToEulerCurve(i->curve, eulerCurves);
		}
		
		for (Vector3Curves::iterator i=m_PositionCurves.begin ();i != m_PositionCurves.end ();i++)
		{
			FloatCurve curve;
			curve.path = i->path;
			curve.classID = ClassID(Transform);
			
			int first = m_EditorCurves.size();
			curve.attribute = "m_LocalPosition.x";
			m_EditorCurves.push_back(curve);
			curve.attribute = "m_LocalPosition.y";
			m_EditorCurves.push_back(curve);	
			curve.attribute = "m_LocalPosition.z";
			m_EditorCurves.push_back(curve);	
			
			AnimationCurve* curves[3] = { &m_EditorCurves[first].curve, &m_EditorCurves[first+1].curve, &m_EditorCurves[first+2].curve };
			ExpandVector3Curve(i->curve, curves);
		}
		
		// Create scale curves
		for (Vector3Curves::iterator i=m_ScaleCurves.begin ();i != m_ScaleCurves.end ();i++)
		{
			FloatCurve curve;
			curve.path = i->path;
			curve.classID = ClassID(Transform);
			
			int first = m_EditorCurves.size();
			curve.attribute = "m_LocalScale.x";
			m_EditorCurves.push_back(curve);
			curve.attribute = "m_LocalScale.y";
			m_EditorCurves.push_back(curve);	
			curve.attribute = "m_LocalScale.z";
			m_EditorCurves.push_back(curve);	
			
			AnimationCurve* curves[3] = { &m_EditorCurves[first].curve, &m_EditorCurves[first+1].curve, &m_EditorCurves[first+2].curve };
			ExpandVector3Curve(i->curve, curves);
		}
	}
	return m_EditorCurves;
}



void AnimationClip::SyncEditorCurves ()
{
	SET_ALLOC_OWNER(this);
	m_RotationCurves.clear();
	m_PositionCurves.clear();
	m_ScaleCurves.clear();
	m_FloatCurves.clear();
	
	for (FloatCurves::iterator i=m_EditorCurves.begin ();i != m_EditorCurves.end ();i++)
	{
		SetCurve(i->path, i->classID, i->script, i->attribute, &i->curve, false);
	}

	ClipWasModifiedAndUpdateMuscleRange ();
	SetDirty();
	
}

struct CurveHasAnimatorAttributePredicate
{
	CurveHasAnimatorAttributePredicate(UnityStr &attr)  : m_Attribute(attr) {}

	bool operator()(const AnimationClip::FloatCurve &curve)
	{	
		return curve.attribute == m_Attribute && curve.classID == ClassID(Animator);
	}

	UnityStr& m_Attribute;
};

void AnimationClip::SyncMuscleCurvesBackwardCompatibility()
{
	for (FloatCurves::iterator i=m_EditorCurves.begin ();i != m_EditorCurves.end ();i++)
	{		
		if(i->classID == ClassID(Animator))	
		{			
			// check if attribute is already in m_FloatCurves, if not add it.			
			if(std::find_if(m_FloatCurves.begin(),m_FloatCurves.end(), CurveHasAnimatorAttributePredicate( i->attribute )) == m_FloatCurves.end())
			{
				m_FloatCurves.push_back(FloatCurve());
				m_FloatCurves.back().path = i->path;
				m_FloatCurves.back().attribute = i->attribute;
				m_FloatCurves.back().classID = i->classID;
				m_FloatCurves.back().script = i->script;
				m_FloatCurves.back().curve = i->curve;								
			}
		}
	}
}

struct EventSorter
{
	bool operator()( const AnimationEvent& ra, const AnimationEvent& rb ) const
	{
		return ra.time < rb.time;
	}
};

static void SortEvents (AnimationClip::Events& events)
{
	std::sort (events.begin(), events.end(), EventSorter());
}

void AnimationClip::SetEvents (const AnimationEvent* events, int size, bool sort)
{
	m_Events.assign(events, events + size);
	if (sort)
		SortEvents(m_Events);
	m_EditModeEvents = m_Events;

	ClipWasModifiedAndUpdateMuscleRange ();
	
	SetDirty();
}

void AnimationClip::ClearEvents ()
{
	m_Events.clear();
	m_EditModeEvents.clear();

	ClipWasModifiedAndUpdateMuscleRange ();
	
	SetDirty();
}

void AnimationClip::CloneAdditionalEditorProperties (Object& src)
{
	Super::CloneAdditionalEditorProperties(src);
	m_Events = static_cast<AnimationClip&> (src).m_Events;
}

void AnimationClip::RevertAllPlaymodeAnimationEvents ()
{
	vector<AnimationClip*> clips;
	Object::FindObjectsOfType(&clips);
	for (int i=0;i<clips.size();i++)
	{
		AnimationClip& clip = *clips[i];
		if (clip.m_Events.size() != clip.m_EditModeEvents.size())
		{
			clip.m_Events = clip.m_EditModeEvents;
			clip.ClipWasModifiedAndUpdateMuscleRange ();
		}
	}
}

#endif


void AnimationClip::AddRuntimeEvent(AnimationEvent& event)
{
	Events::iterator i = lower_bound (m_Events.begin(), m_Events.end(), event);
	m_Events.insert(i, event);
	ClipWasModifiedAndUpdateMuscleRange ();
	
	#if UNITY_EDITOR
	if (!IsWorldPlaying())
	{
		ErrorString("Please use Editor.AnimationUtility to add persistent animation events to an animation clip");
	}
	#endif
}

#if UNITY_EDITOR
bool AnimationClip::GetEditorPPtrCurve (const std::string& path, int classID, MonoScriptPPtr script, const std::string& attribute, PPtrKeyframes* outKeyframes)
{
	PPtrCurves::iterator i;
	for (i = m_PPtrCurves.begin(); i != m_PPtrCurves.end(); ++i)
	{
		if (i->path == path && i->classID == classID && i->script == script && i->attribute == attribute)
		{
			*outKeyframes = i->curve;
			return true;
		}
	}

	return false;
}

void AnimationClip::SetEditorPPtrCurve (const std::string& path, int classID, MonoScriptPPtr script, const std::string& attribute, PPtrKeyframes* keyframes)
{
	SET_ALLOC_OWNER(this);

	if (classID == -1)
	{
		ErrorString("Can't assign curve because the type does not inherit from Component.");
		return;
	}

	// Find existing curve
	PPtrCurves::iterator i;
	for (i = m_PPtrCurves.begin(); i != m_PPtrCurves.end(); ++i)
	{
		if (i->path == path && i->classID == classID && i->script == script && i->attribute == attribute)
			break;
	}
		
	// Shall we remove a curve?
	if (keyframes == NULL)
	{
		if (i != m_PPtrCurves.end())
		{
			m_PPtrCurves.erase(i);
			ClipWasModifiedAndUpdateMuscleRange ();
			SetDirty();
		}
		return;
	}
		
	// Add or replace the curve
	PPtrCurve* comboCurve = (i != m_PPtrCurves.end()) ? &(*i) : NULL;
	if (comboCurve == NULL)
	{
		m_PPtrCurves.push_back(PPtrCurve());
		comboCurve = &m_PPtrCurves.back();
	}

	comboCurve->path = path;
	comboCurve->attribute = attribute;
	comboCurve->classID = classID;
	comboCurve->script = script;
	comboCurve->curve = *keyframes;
	
	ClipWasModifiedAndUpdateMuscleRange ();
	SetDirty();
}
#endif

bool AnimationClip::GetCurve (const string& path, int classID, MonoScriptPPtr script, const std::string& attribute, AnimationCurve* curve)
{
	AnimationCurve dummy0, dummy1, dummy2, dummy3;

	// Get rotation curve
	if (classID == ClassID(Transform) && BeginsWith (attribute, "m_LocalRotation"))
	{
		// Find existing curve
		QuaternionCurves::iterator i;
		for (i=m_RotationCurves.begin();i != m_RotationCurves.end();i++)
		{
			if (i->path == path)
				break;
		}

		if (i == m_RotationCurves.end())
			return false;
		
		AnimationCurve* curves[4] = { &dummy0, &dummy1, &dummy2, &dummy3 };
		
		char last = attribute[attribute.size()-1];
		if (last == 'x')
			curves[0] = curve;
		else if (last == 'y')
			curves[1] = curve;
		else if (last == 'z')
			curves[2] = curve;
		else if (last == 'w')
			curves[3] = curve;
		else
		{
			ErrorString("Can't get curve because " + attribute + " is not a valid Transform property.");
			return false;
		}

		ExpandQuaternionCurve(i->curve, curves);

		return true;
	}
	// Get Local position / local scale curve
	else if (classID == ClassID(Transform) && (BeginsWith (attribute, "m_LocalPosition") || BeginsWith (attribute, "m_LocalScale")))
	{
		Vector3Curves::iterator i;
		
		if (BeginsWith (attribute, "m_LocalPosition"))
		{
			// Find existing curve
			for (i=m_PositionCurves.begin();i != m_PositionCurves.end();i++)
			{
				if (i->path == path)
					break;
			}
			if (i == m_PositionCurves.end())
				return false;
		}
		else
		{
			// Find existing curve
			for (i=m_ScaleCurves.begin();i != m_ScaleCurves.end();i++)
			{
				if (i->path == path)
					break;
			}
			if (i == m_ScaleCurves.end())
				return false;
		}

		
		AnimationCurve* curves[3] = { &dummy0, &dummy1, &dummy2 };
		
		char last = attribute[attribute.size()-1];
		if (last == 'x')
			curves[0] = curve;
		else if (last == 'y')
			curves[1] = curve;
		else if (last == 'z')
			curves[2] = curve;
		else
		{
			ErrorString("Can't get curve because " + attribute + " is not a valid Transform property.");
			return false;
		}

		ExpandVector3Curve(i->curve, curves);

		return true;
	}
	// Get any other type of curve
	else
	{
		// Find existing curve
		FloatCurves::iterator i;
		for (i=m_FloatCurves.begin();i != m_FloatCurves.end();i++)
		{
			if (classID == i->classID && i->path == path && i->attribute == attribute && i->script == script)
				break;
		}
		
		if (i == m_FloatCurves.end())
			return false;
		
		*curve = i->curve;
		return true;
	}
}

void AnimationClip::SetCurve (const string& path, int classID, MonoScriptPPtr script, const std::string& attribute, AnimationCurve* curve, bool syncEditorCurves)
{
	SET_ALLOC_OWNER(this);

	#if UNITY_EDITOR
	if (syncEditorCurves)
	{
		m_EditorCurves.clear();
		m_EulerEditorCurves.clear();
	}
	#endif
	
	if (classID == -1)
	{
		ErrorString("Can't assign curve because the type does not inherit from Component.");
		return;
	}
	
	if (classID == ClassID(Transform) && (BeginsWith (attribute, "m_LocalRotation") || BeginsWith (attribute, "localRotation")))
	{
		// Find existing curve
		QuaternionCurves::iterator i;
		for (i=m_RotationCurves.begin();i != m_RotationCurves.end();i++)
		{
			if (i->path == path)
				break;
		}

		// Shall we remove a curve?
		if (curve == NULL)
		{
			if (attribute == "m_LocalRotation" || attribute == "localRotation")
			{
				if (i != m_RotationCurves.end())
				{
					m_RotationCurves.erase(i);
					ClipWasModified ();
					
					SetDirty();
				}
				return;
			}
			else
			{
				ErrorString("Can't remove individual animation rotation curve " + attribute + " you must remove the entire animation curve with m_LocalRotation.");
				return;
			}
		}

		// Add the curve if it doesnt exist already
		AnimationCurveQuat* comboCurve = i != m_RotationCurves.end() ? &i->curve : NULL;

		if (comboCurve == NULL)
		{
			m_RotationCurves.push_back(QuaternionCurve());
			m_RotationCurves.back().path = path;
			comboCurve = &m_RotationCurves.back().curve;
		}
	
		// Combine the curve into a rotation curve
		char last = attribute[attribute.size()-1];
		if (last == 'x')
			CombineCurve(*curve, 0, *comboCurve);
		else if (last == 'y')
			CombineCurve(*curve, 1, *comboCurve);
		else if (last == 'z')
			CombineCurve(*curve, 2, *comboCurve);
		else if (last == 'w')
			CombineCurve(*curve, 3, *comboCurve);
		else
		{
			ErrorString("Can't assign curve because " + attribute + " is not a valid Transform property.");
		}
	}
	else if (classID == ClassID(Transform) && (BeginsWith (attribute, "m_LocalPosition") || BeginsWith (attribute, "localPosition")))
	{
		// Find existing curve
		Vector3Curves::iterator i;
		for (i=m_PositionCurves.begin();i != m_PositionCurves.end();i++)
		{
			if (i->path == path)
				break;
		}
		
		// Shall we remove a curve?
		if (curve == NULL)
		{
			if (attribute == "m_LocalPosition" || attribute == "localPosition")
			{
				if (i != m_PositionCurves.end())
				{
					m_PositionCurves.erase(i);
					ClipWasModified ();
					SetDirty();
				}
				return;
			}
			else
			{
				ErrorString("Can't remove individual position animation curve " + attribute + " you must remove the entire animation curve with m_LocalPosition.");
				return;
			}
		}

		// Add the curve if it doesnt exist already
		AnimationCurveVec3* comboCurve = i != m_PositionCurves.end() ? &i->curve : NULL;
		if (comboCurve == NULL)
		{
			m_PositionCurves.push_back(Vector3Curve());
			m_PositionCurves.back().path = path;
			comboCurve = &m_PositionCurves.back().curve;
		}

		// Combine the curve into a rotation curve
		char last = attribute[attribute.size()-1];
		if (last == 'x')
			CombineCurve(*curve, 0, *comboCurve);
		else if (last == 'y')
			CombineCurve(*curve, 1, *comboCurve);
		else if (last == 'z')
			CombineCurve(*curve, 2, *comboCurve);
		else
		{
			ErrorString("Can't assign curve because " + attribute + " is not a valid Transform property.");
		}
	}
	else if (classID == ClassID(Transform) && (BeginsWith (attribute, "m_LocalScale") || BeginsWith (attribute, "localScale")))
	{
		// Find existing curve
		Vector3Curves::iterator i;
		for (i=m_ScaleCurves.begin();i != m_ScaleCurves.end();i++)
		{
			if (i->path == path)
				break;
		}
		
		// Shall we remove a curve?
		if (curve == NULL)
		{
			if (attribute == "m_LocalScale" || attribute == "localScale")
			{
				if (i != m_ScaleCurves.end())
				{
					m_ScaleCurves.erase(i);
					ClipWasModified ();
					SetDirty();
				}
				return;
			}
			else
			{
				ErrorString("Can't remove individual scale animation curve " + attribute + " you must remove the entire animation curve with m_LocalScale.");
				return;
			}
		}

		// Add the curve if it doesnt exist already
		AnimationCurveVec3* comboCurve = i != m_ScaleCurves.end() ? &i->curve : NULL;
		if (comboCurve == NULL)
		{
			m_ScaleCurves.push_back(Vector3Curve());
			m_ScaleCurves.back().path = path;
			comboCurve = &m_ScaleCurves.back().curve;
		}

		// Combine the curve into a rotation curve
		char last = attribute[attribute.size()-1];
		if (last == 'x')
			CombineCurve(*curve, 0, *comboCurve);
		else if (last == 'y')
			CombineCurve(*curve, 1, *comboCurve);
		else if (last == 'z')
			CombineCurve(*curve, 2, *comboCurve);
		else
		{
			ErrorString("Can't assign curve because " + attribute + " is not a valid Transform property.");
		}
	}
	else
	{
		// Find existing curve
		FloatCurves::iterator i;
		for (i=m_FloatCurves.begin();i != m_FloatCurves.end();i++)
		{
			if (i->classID == classID && i->path == path && i->attribute == attribute && i->script == script)
				break;
		}
		
		// Shall we remove a curve?
		if (curve == NULL)
		{
			if (i != m_FloatCurves.end ())
			{
				m_FloatCurves.erase(i);
				ClipWasModified ();
				SetDirty();
			}
			
			return;
		}
		
		// Add or replace the curve
		AnimationCurve* comboCurve = i != m_FloatCurves.end() ? &i->curve : NULL;
		if (comboCurve == NULL)
		{
			m_FloatCurves.push_back(FloatCurve());
			m_FloatCurves.back().path = path;
			m_FloatCurves.back().attribute = attribute;
			m_FloatCurves.back().classID = classID;
			m_FloatCurves.back().script = script;
			m_FloatCurves.back().curve = *curve;
		}	
		else
			*comboCurve = *curve;
	}
	
	ClipWasModified ();
	SetDirty();
}

#if UNITY_EDITOR
void AnimationClip::ConvertToNewCurveFormat (NewAnimationTrack& track, int classID, const string& path)
{
	NewAnimationTrack::Curves& curves = track.m_Curves;
	for (NewAnimationTrack::Curves::iterator i=curves.begin();i!= curves.end();i++)
	{
		AnimationCurve& curve = i->curve;
		SetCurve(path, classID, NULL, i->attributeName, &curve, true);
	}
}

void AnimationClip::ConvertToNewCurveFormat ()
{
	for (ClassIDToTrack::iterator i=m_ClassIDToTrack.begin ();i != m_ClassIDToTrack.end ();i++)
	{
		NewAnimationTrack* track = dynamic_pptr_cast<NewAnimationTrack*> (i->second);
		if (track)
			ConvertToNewCurveFormat(*track, i->first, "");
		
		SetDirty();
	}

	for (ChildTracks::iterator i=m_ChildTracks.begin ();i != m_ChildTracks.end ();i++)
	{
		NewAnimationTrack* track = dynamic_pptr_cast<NewAnimationTrack*> (i->track);
		if (track)
			ConvertToNewCurveFormat(*track, i->classID, i->path);

		SetDirty();
	}
	
	// Just leak the actual animation track objects!
	m_ClassIDToTrack.clear();
	m_ChildTracks.clear();
}
#endif


mecanim::animation::ClipMuscleConstant*	AnimationClip::GetRuntimeAsset()
{
	#if UNITY_EDITOR
	if (m_MuscleClip == NULL)
		GenerateMuscleClip();
	#endif
	
	if (m_MuscleClip != 0 && m_MuscleClipSize != 0)
		return m_MuscleClip;

	return NULL;
}

void AnimationClip::GetStats(AnimationClipStats& stats)
{
	memset(&stats, 0, sizeof(stats));
	stats.size = m_MuscleClipSize;
	
	if (GetRuntimeAsset())
	{
		stats.totalCurves = 0;
		for (int i=0;i<m_ClipBindingConstant.genericBindings.size();i++)
		{
			if (m_ClipBindingConstant.genericBindings[i].classID == ClassID(Transform))
			{
				switch (m_ClipBindingConstant.genericBindings[i].attribute)
				{
					case UnityEngine::Animation::kBindTransformPosition:
						stats.positionCurves++;
						break;
					case UnityEngine::Animation::kBindTransformRotation:
						stats.rotationCurves++;
						break;
					case UnityEngine::Animation::kBindTransformScale:
						stats.scaleCurves++;
						break;
				}
			}
			else if (m_ClipBindingConstant.genericBindings[i].isPPtrCurve)
				stats.pptrCurves++;	
			else if (IsMuscleBinding(m_ClipBindingConstant.genericBindings[i]))
				stats.muscleCurves++;
			else
				stats.genericCurves++;
			
			stats.totalCurves++;
		}
	}
}

bool  AnimationClip::IsHumanMotion()
{
	return m_AnimationType == kHumanoid;
}

void AnimationClip::CleanupMecanimData()
{
	// Since the m_MuscleClip and all its data is placed on the m_Allocator, the destructor is not needed
	m_MuscleClip = 0;
	m_MuscleClipSize = 0;
	m_ClipAllocator.Reset();
	
	///@TODO: Make destory function.
	m_ClipBindingConstant.genericBindings.clear();
	m_ClipBindingConstant.pptrCurveMapping.clear();
}

bool  AnimationClip::IsAnimatorMotion()const
{
	return m_AnimationType == kHumanoid || m_AnimationType == kGeneric;
}


#if UNITY_EDITOR

void  AnimationClip::SetAnimationType(AnimationType type)
{
	m_AnimationType = type;
}


void AnimationClip::SetAnimationClipSettingsNoDirty(AnimationClipSettings &clipInfo) 
{ 
	m_AnimationClipSettings = clipInfo;  

	if (GetRuntimeAsset())
		PatchMuscleClipWithInfo (m_AnimationClipSettings, IsHumanMotion(), m_MuscleClip);
}

void AnimationClip::GenerateMuscleClip()
{
	CleanupMecanimData();
	
	if (m_AnimationType == kLegacy)
		return;
	
	MecanimClipBuilder clipBuilder;
	GenericAnimationBindingCache& binder = GetGenericAnimationBindingCache();
	
	//// collect all curves, and count keys
	
	// Position curves
	for (Vector3Curves::iterator positionCurveIter = m_PositionCurves.begin (); positionCurveIter != m_PositionCurves.end (); positionCurveIter++)
		AddPositionCurveToClipBuilder (positionCurveIter->curve, positionCurveIter->path, clipBuilder, m_UseHighQualityCurve);

	// Rotation curves
	for (QuaternionCurves::iterator rotationCurveIter = m_RotationCurves.begin (); rotationCurveIter != m_RotationCurves.end (); rotationCurveIter++)
		AddRotationCurveToClipBuilder (rotationCurveIter->curve, rotationCurveIter->path, clipBuilder, m_UseHighQualityCurve);

	// Scale curves
	for (Vector3Curves::iterator scaleCurveIter = m_ScaleCurves.begin (); scaleCurveIter != m_ScaleCurves.end (); scaleCurveIter++)
		AddScaleCurveToClipBuilder (scaleCurveIter->curve, scaleCurveIter->path, clipBuilder, m_UseHighQualityCurve);

	// Dynamic binded curves	
	for (FloatCurves::iterator dynamicCurveIter = m_FloatCurves.begin(); dynamicCurveIter != m_FloatCurves.end (); dynamicCurveIter++)
	{
		GenericBinding binding;
		binder.CreateGenericBinding (dynamicCurveIter->path, dynamicCurveIter->classID, dynamicCurveIter->script, dynamicCurveIter->attribute, false, binding);
		AddGenericCurveToClipBuilder (dynamicCurveIter->curve, binding, clipBuilder, m_UseHighQualityCurve);
	}

	// PPtr curves
	for (PPtrCurves::iterator pptrCurveIter = m_PPtrCurves.begin(); pptrCurveIter != m_PPtrCurves.end(); ++pptrCurveIter)
	{
		GenericBinding binding;
		binder.CreateGenericBinding (pptrCurveIter->path, pptrCurveIter->classID, pptrCurveIter->script, pptrCurveIter->attribute, true, binding);
		AddPPtrCurveToClipBuilder(pptrCurveIter->curve, binding, clipBuilder);
	}	

	clipBuilder.hasAnimationEvents = HasAnimationEvents ();
	clipBuilder.sampleRate = GetSampleRate();
	
	if (!PrepareClipBuilder (clipBuilder))
		return;
	
    m_MuscleClip = BuildMuscleClip (clipBuilder, m_AnimationClipSettings, IsHumanMotion (), m_ClipBindingConstant, m_ClipAllocator);
	if (m_MuscleClip)
	{
		BlobWrite::container_type blob;
		BlobWrite blobWrite (blob, kNoTransferInstructionFlags, kBuildNoTargetPlatform);
		blobWrite.Transfer( *m_MuscleClip, "Base");
		
		m_MuscleClipSize = blob.size();
	}
}




bool AnimationClip::ValidateIfRetargetable(bool showWarning)
{
	if(!IsAnimatorMotion())
	{											
		if(showWarning)
			WarningString (Format("Animation clip '%s' is not retargetable. Animation clips used within the Animator Controller need to have Muscle Definition set up in the Asset Importer Inspector", GetName()));
		return false;
	}
	return true;
}

bool AnimationClip::IsLooping()
{
	return m_AnimationClipSettings.m_LoopTime;	
}


float AnimationClip::GetAverageDuration()
{
	return m_AnimationClipSettings.m_StopTime - m_AnimationClipSettings.m_StartTime;
}

float AnimationClip::GetAverageAngularSpeed()
{
	return m_MuscleClip != 0 ? m_MuscleClip->m_AverageAngularSpeed : 0;
}

Vector3f AnimationClip::GetAverageSpeed()
{
	return m_MuscleClip != 0 ? float4ToVector3f(m_MuscleClip->m_AverageSpeed) : Vector3f::zero;
}

float AnimationClip::GetApparentSpeed()
{
	// approximation of equivalent rectilinear motion speed that conserves kinectic energy ... i'll work on something better
	return Magnitude(GetAverageSpeed()) * (1 + pow(GetAverageAngularSpeed()/2,2));
}

#endif

AnimationClip::~AnimationClip ()
{
	gDidModifyClipCallback (NULL, m_AnimationStates);
	m_MuscleClip = 0;
	NotifyObjectUsers(kDidModifyMotion);
}

pair<float, float> AnimationClip::GetRange ()
{
	pair<float, float> range = make_pair (std::numeric_limits<float>::infinity (), -std::numeric_limits<float>::infinity ());
	if (range != m_CachedRange)
		return m_CachedRange;
	for (QuaternionCurves::iterator i=m_RotationCurves.begin ();i != m_RotationCurves.end ();i++)
	{
		pair<float, float> curRange = i->curve.GetRange ();
		range.first = min (curRange.first, range.first);
		range.second = max (curRange.second, range.second);
	}

	for (Vector3Curves::iterator i=m_PositionCurves.begin ();i != m_PositionCurves.end ();i++)
	{
		pair<float, float> curRange = i->curve.GetRange ();
		range.first = min (curRange.first, range.first);
		range.second = max (curRange.second, range.second);
	}

	for (Vector3Curves::iterator i=m_ScaleCurves.begin ();i != m_ScaleCurves.end ();i++)
	{
		pair<float, float> curRange = i->curve.GetRange ();
		range.first = min (curRange.first, range.first);
		range.second = max (curRange.second, range.second);
	}

	for (FloatCurves::iterator i=m_FloatCurves.begin ();i != m_FloatCurves.end ();i++)
	{
		pair<float, float> curRange = i->curve.GetRange ();
		range.first = min (curRange.first, range.first);
		range.second = max (curRange.second, range.second);
	}

	for (PPtrCurves::iterator i=m_PPtrCurves.begin ();i != m_PPtrCurves.end ();i++)
	{
		if (i->curve.empty())
			continue;
		
		range.first = min (i->curve.front().time, range.first);
		range.second = max (i->curve.back().time + 1.0F / m_SampleRate, range.second);
	}
	
	#if UNITY_EDITOR
	// get a valid range for muscle clip when importing
	for (FloatCurves::iterator i=m_EditorCurves.begin ();i != m_EditorCurves.end ();i++)
	{
		pair<float, float> curRange = i->curve.GetRange ();
		range.first = min (curRange.first, range.first);
		range.second = max (curRange.second, range.second);
	}
	#endif

	if (!m_Events.empty())
	{
		range.first = min (m_Events.front().time, range.first);
		range.second = max (m_Events.back().time, range.second);
	}

	if (range.first == std::numeric_limits<float>::infinity() && range.second == -std::numeric_limits<float>::infinity())
	{
		// arbitrary range - the length shouldn't matter because it doesn't have any keys or events anyway
		// TODO : do not allow to create clips without any keys or events or least it show a warning (LocomotionSystem creates such clip now)
		range.first = 0;
		range.second = 1;
	}

	m_CachedRange = range;
	AssertIf(!IsFinite(m_CachedRange.first) || !IsFinite(m_CachedRange.second));

	return m_CachedRange;
}

IMPLEMENT_CLASS_HAS_INIT (AnimationClip)
IMPLEMENT_OBJECT_SERIALIZE (AnimationClip)

void AnimationClip::InitializeClass ()
{
	// 2.6 beta -> 2.6 final compatibility
	RegisterAllowNameConversion("AnimationClip", "m_UseCompression", "m_Compressed");
	// 4.3
	RegisterAllowNameConversion("AnimationClip", "m_MuscleClipInfo", "m_AnimationClipSettings");
}

#if UNITY_EDITOR
class StripCurvesForMecanimClips
{
public:
	
	AnimationClip*                    clip;
	AnimationClip::QuaternionCurves   rotationCurves;
	AnimationClip::Vector3Curves      positionCurves;
	AnimationClip::Vector3Curves      scaleCurves;
	AnimationClip::FloatCurves        floatCurves;
	AnimationClip::PPtrCurves         pptrCurves;
	bool                              stripCurves;
	
	StripCurvesForMecanimClips (AnimationClip& inputClip, bool inStripCurves)
	{
		if (inStripCurves)
		{	
			clip = &inputClip;
			rotationCurves.swap (clip->m_RotationCurves);
			positionCurves.swap (clip->m_PositionCurves);
			scaleCurves.swap (clip->m_ScaleCurves);
			floatCurves.swap (clip->m_FloatCurves);
			pptrCurves.swap (clip->m_PPtrCurves);
		}
		else
		{
			clip = NULL;
		}
	}

	~StripCurvesForMecanimClips ()
	{
		if (clip)
		{
			rotationCurves.swap (clip->m_RotationCurves);
			positionCurves.swap (clip->m_PositionCurves);
			scaleCurves.swap (clip->m_ScaleCurves);
			floatCurves.swap (clip->m_FloatCurves);
			pptrCurves.swap (clip->m_PPtrCurves);
		}
	}
};
#endif

static void ConvertDeprecatedValueArrayConstantBindingToGenericBinding (const mecanim::animation::ClipMuscleConstant* muscleClip, UnityEngine::Animation::AnimationClipBindingConstant& bindings)
{
	if (muscleClip == NULL || muscleClip->m_Clip.IsNull() || muscleClip->m_Clip->m_DeprecatedBinding.IsNull())
		return;
	
	const mecanim::ValueArrayConstant& values = *muscleClip->m_Clip->m_DeprecatedBinding;
	
	for (int i=0;i<values.m_Count;)
	{
		mecanim::uint32_t curveID = values.m_ValueArray[i].m_ID;
		mecanim::uint32_t curveTypeID = values.m_ValueArray[i].m_TypeID;
			
		bindings.genericBindings.push_back(GenericBinding());
		GenericBinding& binding = bindings.genericBindings.back();

		if(curveTypeID == mecanim::CRCKey(mecanim::ePositionX))
		{
			binding.path = curveID;
			binding.attribute = UnityEngine::Animation::kBindTransformPosition;
			binding.classID = ClassID(Transform);
			i+=3;
		}
		else if(curveTypeID == mecanim::CRCKey(mecanim::eQuaternionX))
		{
			binding.path = curveID;
			binding.attribute = UnityEngine::Animation::kBindTransformRotation;
			binding.classID = ClassID(Transform);
			i+=4;
		}
		else if(curveTypeID == mecanim::CRCKey(mecanim::eScaleX))
		{
			binding.path = curveID;
			binding.attribute = UnityEngine::Animation::kBindTransformScale;
			binding.classID = ClassID(Transform);
			i+=3;
		}
		else
		{
			if(curveTypeID == mecanim::CRCKey(mecanim::ePositionY) || curveTypeID == mecanim::CRCKey(mecanim::ePositionZ) || curveTypeID == mecanim::CRCKey(mecanim::eQuaternionY) || curveTypeID == mecanim::CRCKey(mecanim::eQuaternionZ) || curveTypeID == mecanim::CRCKey(mecanim::eQuaternionW) || curveTypeID == mecanim::CRCKey(mecanim::eScaleY) || curveTypeID == mecanim::CRCKey(mecanim::eScaleZ))
			{
				AssertString("Invalid value array data");
			}
			// 
			binding.classID = ClassID(Animator);
			binding.path = 0;
			binding.attribute = curveID;
			
			i++;
		}
	}
}

template<class TransferFunction>
void AnimationClip::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	transfer.SetVersion (4);

	TRANSFER_ENUM(m_AnimationType);
	
	// Strip mecanim curve data for non-legacy clips
	#if UNITY_EDITOR
	StripCurvesForMecanimClips revert (*this, transfer.IsWritingGameReleaseData() && m_AnimationType != kLegacy);
	#endif

	// Backwards compatibility with ancient tracks
	#if UNITY_EDITOR
	if (transfer.IsOldVersion(2) || transfer.IsOldVersion(1))
	{
		transfer.Transfer (m_ClassIDToTrack, "m_ClassIDToTrack", kHideInEditorMask);
		transfer.Transfer (m_ChildTracks, "m_ChildTracks", kHideInEditorMask);
	}
	#endif	

	// Rotation curves / potentially compressed
	transfer.Transfer (m_Compressed, "m_Compressed", kNotEditableMask);
	transfer.Transfer (m_UseHighQualityCurve, "m_UseHighQualityCurve", kNotEditableMask);
	transfer.Align();
	
	if(!m_Compressed)
	{
		transfer.Transfer (m_RotationCurves, "m_RotationCurves", kHideInEditorMask);
		CompressedQuaternionCurves empty;
		transfer.Transfer (empty, "m_CompressedRotationCurves", kHideInEditorMask);
	}
	else
	{
		QuaternionCurves empty;
		transfer.Transfer (empty, "m_RotationCurves", kHideInEditorMask);
		
		TRANSFER_WITH_CUSTOM_GET_SET (CompressedQuaternionCurves, "m_CompressedRotationCurves", 
			CompressCurves(value),
			DecompressCurves(value),
			kHideInEditorMask);	
	}
	
	transfer.Transfer (m_PositionCurves, "m_PositionCurves", kHideInEditorMask);
	transfer.Transfer (m_ScaleCurves, "m_ScaleCurves", kHideInEditorMask);
	transfer.Transfer (m_FloatCurves, "m_FloatCurves", kHideInEditorMask);
	transfer.Transfer (m_PPtrCurves, "m_PPtrCurves", kHideInEditorMask);
	transfer.Transfer (m_SampleRate, "m_SampleRate");
	transfer.Transfer (m_WrapMode, "m_WrapMode");
	transfer.Transfer (m_Bounds, "m_Bounds");
	
	if (transfer.IsSerializingForGameRelease())
	{
		TRANSFER(m_MuscleClipSize);

		if(m_MuscleClip == 0)
			m_ClipAllocator.Reserve(m_MuscleClipSize);

		// Enforce that there is always a valid muscle clip when building for the player.
		if (transfer.IsWritingGameReleaseData ())
			GetRuntimeAsset ();
		
		transfer.SetUserData(&m_ClipAllocator);
		TRANSFER_NULLABLE(m_MuscleClip, mecanim::animation::ClipMuscleConstant);
		TRANSFER(m_ClipBindingConstant);
		
		if (transfer.IsReadingBackwardsCompatible())
			ConvertDeprecatedValueArrayConstantBindingToGenericBinding (m_MuscleClip, m_ClipBindingConstant);
	}
	

	// Editor curves
	#if UNITY_EDITOR
	if (!transfer.IsSerializingForGameRelease())
	{
		transfer.Transfer (m_AnimationClipSettings, "m_AnimationClipSettings");
		transfer.Transfer (m_EditorCurves, "m_EditorCurves", kHideInEditorMask);
		transfer.Transfer (m_EulerEditorCurves, "m_EulerEditorCurves", kHideInEditorMask);
	}
	#endif
	
	// Events
	#if UNITY_EDITOR
	transfer.Transfer (m_EditModeEvents, "m_Events", kHideInEditorMask);
	if (transfer.IsReading())
		m_Events = m_EditModeEvents;
	#else
	transfer.Transfer (m_Events, "m_Events", kHideInEditorMask);
	#endif

	#if UNITY_EDITOR
	if (transfer.IsVersionSmallerOrEqual(3))
	{
		SyncMuscleCurvesBackwardCompatibility();	
	}
	#endif
}

void AnimationClip::CompressCurves (CompressedQuaternionCurves& compressedRotationCurves)
{
	bool didShowError = false;
	compressedRotationCurves.resize(m_RotationCurves.size());
	
	for(int i=0;i<compressedRotationCurves.size();i++)
	{
		compressedRotationCurves[i].CompressQuatCurve(m_RotationCurves[i]);
		if( m_RotationCurves[i].curve.GetKeyCount() > 0 && !didShowError)
		{
			if( m_RotationCurves[i].curve.GetKey(0).time < -kCurveTimeEpsilon)
			{
				LogStringObject(Format("Animation Clip %s contains negative time keys. This may cause your animation to look wrong, as negative time keys are not supported in compressed animation clips!",this->GetName()),this);
				didShowError = true;
			}
		}
	}
}

void AnimationClip::DecompressCurves (CompressedQuaternionCurves& compressedRotationCurves)
{
	SET_ALLOC_OWNER(this);
	m_RotationCurves.resize(compressedRotationCurves.size());
	
	for(int i=0;i<compressedRotationCurves.size();i++)
		compressedRotationCurves[i].DecompressQuatCurve(m_RotationCurves[i]);
}


#if UNITY_EDITOR
template<class TransferFunction>
void AnimationClip::ChildTrack::Transfer (TransferFunction& transfer)
{
	TRANSFER (path);
	TRANSFER (classID);
	TRANSFER (track);
}
#endif

template<class TransferFunction>
void AnimationClip::QuaternionCurve::Transfer (TransferFunction& transfer)
{
	TRANSFER (curve);
	TRANSFER (path);
}

template<class TransferFunction>
void AnimationClip::Vector3Curve::Transfer (TransferFunction& transfer)
{
	TRANSFER (curve);
	TRANSFER (path);
}

template<class TransferFunction>
void AnimationClip::FloatCurve::Transfer (TransferFunction& transfer)
{
	TRANSFER (curve);
	TRANSFER (attribute);
	TRANSFER (path);
	TRANSFER (classID);
	TRANSFER (script);
}

template<class TransferFunction>
void AnimationClip::PPtrCurve::Transfer (TransferFunction& transfer)
{
	TRANSFER (curve);
	TRANSFER (attribute);
	TRANSFER (path);
	TRANSFER (classID);
	TRANSFER (script);
}

template<class TransferFunction>
void PPtrKeyframe::Transfer (TransferFunction& transfer)
{
	TRANSFER (time);
	TRANSFER (value);
}

void AnimationClip::SetDidModifyClipCallback(DidModifyClipCallback* callback)
{
	gDidModifyClipCallback = callback;
}

void AnimationClip::AddRotationCurve (const AnimationCurveQuat& quat, const std::string& path)
{
	SET_ALLOC_OWNER(this);
	m_RotationCurves.push_back(QuaternionCurve());
	m_RotationCurves.back().curve = quat;
	m_RotationCurves.back().path = path;
}

void AnimationClip::AddPositionCurve (const AnimationCurveVec3& quat, const std::string& path)
{
	SET_ALLOC_OWNER(this);
	m_PositionCurves.push_back(Vector3Curve());
	m_PositionCurves.back().curve = quat;
	m_PositionCurves.back().path = path;
}

void AnimationClip::AddScaleCurve (const AnimationCurveVec3& quat, const std::string& path)
{
	SET_ALLOC_OWNER(this);
	m_ScaleCurves.push_back(Vector3Curve());
	m_ScaleCurves.back().curve = quat;
	m_ScaleCurves.back().path = path;
}

void AnimationClip::AddFloatCurve (const AnimationCurve& curve, const std::string& path, int classID, const std::string& attribute)
{
	m_FloatCurves.push_back(FloatCurve());
	FloatCurve& fc = m_FloatCurves.back();
	fc.curve = curve;
	fc.path = path;
	fc.classID = classID;
	fc.attribute = attribute;	
}
