#include "UnityPrefix.h"
#include "Transform.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/BaseClasses/SupportedMessageOptimization.h"
#include "Runtime/Serialize/SerializationMetaFlags.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Utilities/RecursionLimit.h"
#include "Runtime/Utilities/Prefetch.h"
#include "Runtime/Utilities/ValidateArgs.h"
#include "Runtime/Misc/BuildSettings.h"

#if UNITY_EDITOR
#include "Editor/Src/SceneInspector.h"
static Transform::HierarchyChangedCallback* gHierarchyChangedCallback = NULL;
static Transform::HierarchyChangedCallbackSetParent* gHierarchyChangedSetParentCallback = NULL;
#endif

static Transform* FindActiveTransformWithPathImpl (const char* path, GameObject& go, bool needsToBeRoot);

// parentTransform * (translation * rotation * scale)

#if DEBUGMODE
#define ASSERT_ROTATION if (!CompareApproximately (SqrMagnitude (m_LocalRotation), 1.0F)) { AssertStringObject(Format("transform.rotation of '%s' is no longer valid, due to a bad input value. Input rotation is %f, %f, %f, %f.", GetName(), q.x, q.y, q.z, q.w), this); }
#else
#define ASSERT_ROTATION
#endif

#pragma message ("Move this away")
inline float InverseSafe (float f)
{
	if (Abs (f) > Vector3f::epsilon)
		return 1.0F / f;
	else
		return 0.0F;
		
}

inline Vector3f InverseSafe (const Vector3f& v)
{
	return Vector3f (InverseSafe (v.x), InverseSafe (v.y), InverseSafe (v.z));
}

static float MakeNiceAbs(float f)
{
	union
	{
		float f;
		UInt32 n;
	} u;
	u.f = f;

	int exponent = ((u.n & 0x7F800000) >> 23) - 127;

	if(exponent >= 24)
		return f;
	
	if(exponent <= -24)
		return 0;

	UInt32 v = (UInt32)f;
	float fraction = f - v;
                   
	if(fraction < 0.00001f)
		return v;

	if(fraction > 0.99999f)
		return v + 1;

	return f;
}

static float MakeNice(float f)
{
	if(f >= 0)
		return MakeNiceAbs(f);
	else
		return -MakeNiceAbs(-f);
}

Vector3f MakeNice(const Vector3f& v)
{
	return Vector3f(MakeNice(v[0]), MakeNice(v[1]), MakeNice(v[2]));
}




Transform::Transform (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
//,   m_Children (Transform::TransformComList::allocator_type(*baseAllocator))
#if UNITY_EDITOR
,	m_VisibleRootValid(false)
#endif
,	m_CachedTransformType(kNoScaleTransform)
,	m_HasCachedTransformMatrix(false)
,	m_HasChanged(false)
{
	m_InternalTransformType = kNoScaleTransform;
	m_SupportsTransformChanged = 0;

#if UNITY_EDITOR
	// Create hint that will make euler angles at editor time match those in play mode
	// 
	m_LocalEulerAnglesHint.x = 179.999f;
	m_LocalEulerAnglesHint.y = 179.999f;
	m_LocalEulerAnglesHint.z = 179.999f;
	#endif
}

void Transform::Reset ()
{
	Super::Reset ();
	m_LocalRotation = Quaternionf::identity ();
	
	m_LocalPosition = Vector3f::zero;
	m_LocalScale = Vector3f::one;
	
#if UNITY_EDITOR
	m_LocalEulerAnglesHint.x = 0;
	m_LocalEulerAnglesHint.y = 0;
	m_LocalEulerAnglesHint.z = 0;
#endif
	
	RecalculateTransformType ();
	
	
	m_HasCachedTransformMatrix = false;
	m_HasChanged = true;

	if (GetGameObjectPtr())
		SendTransformChanged (kPositionChanged | kRotationChanged | kScaleChanged | kParentingChanged);

#if ENABLE_EDITOR_HIERARCHY_ORDERING
	m_Order = 0;
#endif
}

Transform::~Transform ()
{
#if UNITY_EDITOR
	if (m_VisibleRootValid)
		GetSceneTracker().GetVisibleRootTransforms().erase(m_VisibleRootIterator);
#endif
}

Transform::iterator Transform::Find( const Transform* child )
{
	iterator it, itEnd = end();
	for( it = begin(); it != itEnd; ++it )
	{
		if( *it == child )
			return it;
	}
	return itEnd;
}

void Transform::RemoveFromParent ()
{
//	Assert(!IsPersistent());
	
	// If it has an father, remove this from fathers children
	Transform* father = GetParent ();
	if (father != NULL)
	{
//		Assert(!father->IsPersistent());
		
		TransformComList& children = father->m_Children;
		// Fastpath try back of children array
		if (!children.empty() != 0 && &*children.back() == this)
		{
			children.pop_back();
		}
		// Find this in fathers children list
		else
		{
			iterator i = father->Find(this);
			if (i != children.end ())
				children.erase (i);
		}
		
		father->SetDirty ();
	}
}

void Transform::ClearChildrenParentPointer ()
{
	for (int i=0;i<m_Children.size();i++)
	{
		Transform* cur = m_Children[i];
		
		AssertIf(cur == NULL || cur->GetParent() != this);
		if (cur && cur->GetParent() == this)
			cur->m_Father = NULL;
	}
}

void Transform::ClearChild (Transform* child)
{
	iterator found = Find(child);
	if (found != m_Children.end())
		m_Children.erase(found);
}

Matrix3x3f Transform::GetWorldScale () const
{
	Matrix3x3f invRotation;
	QuaternionToMatrix (Inverse (GetRotation ()), invRotation);
	Matrix3x3f scaleAndRotation = GetWorldRotationAndScale ();
	return invRotation * scaleAndRotation;
}

Vector3f Transform::GetWorldScaleLossy () const
{
	Matrix3x3f rot = GetWorldScale ();
	return Vector3f (rot.Get (0, 0), rot.Get (1, 1), rot.Get (2, 2));
}


Matrix3x3f Transform::GetWorldRotationAndScale () const
{
	Matrix3x3f scale;
	scale.SetScale (m_LocalScale);

	Matrix3x3f rotation;
	QuaternionToMatrix (m_LocalRotation, rotation);
	
	Transform* parent = GetParent ();
	if (parent)
	{
		///@TODO optimize: Special case multiplication
		Matrix3x3f parentTransform = parent->GetWorldRotationAndScale ();
		return parentTransform * rotation * scale;
	}
	else
	{
		return rotation * scale;
	}
}

bool Transform::SetParent (Transform* newFather, SetParentOption options)
{
	if (GetGameObject().IsDestroying() || (newFather && newFather->GetGameObject().IsDestroying()))
		return false;

	if (IS_CONTENT_NEWER_OR_SAME (kUnityVersion4_0_a1))
	{
		if ( (GetParent () && GetParent ()->GetGameObject().IsActivating()) || (newFather && newFather->GetGameObject().IsActivating()))
		{
			ErrorStringObject ("Cannot change GameObject hierarchy while activating or deactivating the parent.", this);
			return false;
		}
	}
	
	// Make sure that the new father is not a child of this transform.
	Transform* cur;
	for (cur=newFather;cur != NULL;cur = cur->m_Father)
	{
		if (this == cur)
			return false;
	}
	
	if ((options & kAllowParentingFromPrefab) ==  0)
	{
		if (IsPrefabParent() || (newFather && newFather->IsPrefabParent()))
		{
			ErrorString("Setting the parent of a transform which resides in a prefab is disabled to prevent data corruption.");
			return false;
		}	
	}

//	if (IsPersistent() || (newFather && newFather->IsPersistent()))
//	{
//		ErrorString("Setting the parent of a transform on an asset is not allowed!");
//		return false;
//	}	

	// Save the old position in worldspace
	Vector3f worldPosition = GetPosition ();
	Quaternionf worldRotation = GetRotation ();
	Matrix3x3f worldScale = GetWorldRotationAndScale ();

	// If it already has an father, remove this from fathers children
	Transform* father = GetParent ();
	if (father != NULL)
	{
		// Find this in fathers children list
		iterator i = father->Find( this );
		AssertIf (i == father->end ());
		father->m_Children.erase (i);
		father->SetDirty ();
	}
	
	#if UNITY_EDITOR
	if (newFather)
	{
		///@TODO: This can use binary search because we should be able to assume that the children array is already sorted
		iterator i = newFather->begin();
		iterator end = newFather->end();
		
		for (;i!=end;i++)
		{
		#if ENABLE_EDITOR_HIERARCHY_ORDERING
			if (CompareDepths(this, (*i)))
		#else
			if (SemiNumericCompare(GetName(), (**i).GetName()) < 0)
		#endif
			{
				newFather->m_Children.insert (i, this);
				break;
			}
		}
		if (i == end)
			newFather->m_Children.push_back (this);
		
		newFather->SetDirty ();
	}
	#else
	if (newFather)
	{
		newFather->m_Children.push_back (this);
		newFather->SetDirty ();
	}
	#endif
	
	m_Father = newFather;

	if (!(options & kDisableTransformMessage))
	{
		if (options & kWorldPositionStays)
		{
			// Restore old position so they stay at the same position in worldspace
			SetRotationSafe (worldRotation);
			SetPosition (worldPosition);
			SetWorldRotationAndScale ( worldScale );
			SendTransformChanged (kParentingChanged);
		}
		else
			SendTransformChanged (kPositionChanged | kRotationChanged | kScaleChanged | kParentingChanged);
	}
		
	#if UNITY_EDITOR
	if (gHierarchyChangedSetParentCallback)
		gHierarchyChangedSetParentCallback (this, father, newFather);
	#endif
	SetDirty ();
	SetCacheDirty();

	return true;
}


#if UNITY_EDITOR

#if ENABLE_EDITOR_HIERARCHY_ORDERING
bool Transform::CompareVisibleRoots::operator() (const VisibleRootKey& lhs, const VisibleRootKey& rhs) const
{
	bool orderCompare = lhs.first != rhs.first;
	if (orderCompare)
		return lhs.first < rhs.first;
	else
	{
		VisibleRootSecondaryKey lhsSecondaryKey = lhs.second;
		VisibleRootSecondaryKey rhsSecondaryKey = rhs.second;
		int compare = SemiNumericCompare(lhsSecondaryKey.first, rhsSecondaryKey.first);
		if (compare != 0)
			return compare < 0;
		return lhsSecondaryKey.second < rhsSecondaryKey.second;
	}
}
#else
bool Transform::CompareVisibleRoots::operator() (const VisibleRootKey& lhs, const VisibleRootKey& rhs) const
{
	int compare = SemiNumericCompare(lhs.first, rhs.first);
	if (compare != 0)
		return compare < 0;
	return lhs.second < rhs.second;
}
#endif

// Cannot be called Repeat as there is already another function with that name (which does currently not work for negative numbers)
inline float RepeatWorking (float t, float length)
{
	return (t - (floor (t / length) * length));
}

void Transform::SyncLocalEulerAnglesHint ()
{
	if (IsWorldPlaying())
		return;
	
	Vector3f newEuler = QuaternionToEuler(m_LocalRotation) * Rad2Deg(1);
	
	newEuler.x = RepeatWorking(newEuler.x - m_LocalEulerAnglesHint.x + 180.0F, 360.0F) + m_LocalEulerAnglesHint.x - 180.0F;
	newEuler.y = RepeatWorking(newEuler.y - m_LocalEulerAnglesHint.y + 180.0F, 360.0F) + m_LocalEulerAnglesHint.y - 180.0F;
	newEuler.z = RepeatWorking(newEuler.z - m_LocalEulerAnglesHint.z + 180.0F, 360.0F) + m_LocalEulerAnglesHint.z - 180.0F;
	
	m_LocalEulerAnglesHint = MakeNice(newEuler);
}
#endif

void Transform::SetLocalEulerAngles (const Vector3f& eulerAngles)
{
	ABORT_INVALID_VECTOR3 (eulerAngles, localEulerAngles, transform)
	
	SetLocalRotationSafe (EulerToQuaternion (eulerAngles * Deg2Rad (1)));
	
	#if UNITY_EDITOR
	if (!IsWorldPlaying())
	{
		m_LocalEulerAnglesHint = MakeNice(eulerAngles);
	}
	#endif
}

Vector3f Transform::GetLocalEulerAngles ()
{
	#if UNITY_EDITOR
	if (!IsWorldPlaying())
	{
		if ( !CompareApproximately (EulerToQuaternion (m_LocalEulerAnglesHint * Deg2Rad(1)), m_LocalRotation) )
			SyncLocalEulerAnglesHint ();
		
		return m_LocalEulerAnglesHint;
	}
	else
	{
		Quaternionf rotation = NormalizeSafe (m_LocalRotation);
		return QuaternionToEuler (rotation) * Rad2Deg (1);
	}
	#else
	Quaternionf rotation = NormalizeSafe (m_LocalRotation);
	return QuaternionToEuler (rotation) * Rad2Deg (1);
	#endif
}


void Transform::SetLocalPosition (const Vector3f& inTranslation)
{
	ABORT_INVALID_VECTOR3 (inTranslation, localPosition, transform);
	m_LocalPosition = inTranslation;
	SetDirty ();
	SendTransformChanged (kPositionChanged);
}

void Transform::SetLocalRotation (const Quaternionf& q)
{
	ABORT_INVALID_QUATERNION (q, localRotation, transform);
	m_LocalRotation = q;

#if UNITY_EDITOR
	SyncLocalEulerAnglesHint ();
#endif
	ASSERT_ROTATION

	SetDirty ();
	SendTransformChanged (kRotationChanged);
}

void Transform::SetLocalRotationSafe (const Quaternionf& q)
{
	SetLocalRotation (NormalizeSafe(q));
}

void Transform::SetRotation (const Quaternionf& q)
{
	Transform* father = GetParent ();
	if (father != NULL)
		SetLocalRotation (Inverse (father->GetRotation ()) * q);
	else
		SetLocalRotation (q);
}

void Transform::SetRotationSafe(const Quaternionf& q) 
{
	ABORT_INVALID_QUATERNION (q, rotation, transform);
	Transform* father = GetParent ();
	if (father != NULL)
		SetLocalRotation (NormalizeSafe (Inverse (father->GetRotation ()) * q));
	else
		SetLocalRotation (NormalizeSafe (q));
}

void Transform::SetPosition (const Vector3f& p)
{
	ABORT_INVALID_VECTOR3 (p, position, transform);

	Vector3f newPosition = p;
	Transform* father = GetParent ();
	if (father != NULL)
		newPosition = father->InverseTransformPoint (newPosition);
		
	SetLocalPosition (newPosition);
}

void Transform::SetPositionWithLocalOffset (const Vector3f& p, const Vector3f& localOffset)
{
	ABORT_INVALID_VECTOR3 (p, positionWithLocalOffset, transform);
	ABORT_INVALID_VECTOR3 (localOffset, positionWithLocalOffset, transform);
	Vector3f newPosition = p - TransformPoint (localOffset) + GetPosition ();
	SetPosition (newPosition);
}	

Vector3f Transform::TransformPointWithLocalOffset (const Vector3f& p, const Vector3f& localOffset) const
{
	return p - TransformPoint (localOffset) + GetPosition ();
}

void Transform::SetWorldRotationAndScale (const Matrix3x3f& scale)
{
	m_LocalScale = Vector3f::one;

	Matrix3x3f inverseRS = GetWorldRotationAndScale ();
	inverseRS.Invert ();
	
	inverseRS = inverseRS * scale;
	
	m_LocalScale.x = inverseRS.Get (0, 0);
	m_LocalScale.y = inverseRS.Get (1, 1);
	m_LocalScale.z = inverseRS.Get (2, 2);

	RecalculateTransformType ();
	SetDirty ();
	SendTransformChanged (kScaleChanged | kRotationChanged | kPositionChanged);
}

void Transform::SetLocalScale (const Vector3f& scale)
{
	ABORT_INVALID_VECTOR3 (scale, localScale, transform);
	m_LocalScale = scale;
	RecalculateTransformType ();
	SetDirty ();
	SendTransformChanged (kScaleChanged | kRotationChanged | kPositionChanged);
}

template<bool Safe, bool Notify>
void Transform::SetPositionAndRotationInternal ( const Vector3f& p, const Quaternionf& q )
{
	ABORT_INVALID_VECTOR3 (p, position, transform);
	ABORT_INVALID_QUATERNION (q, rotation, transform);
	Transform* father = GetParent ();
	if (father != NULL)
	{
		m_LocalPosition = father->InverseTransformPoint (p);
		if ( Safe )
			m_LocalRotation = NormalizeSafe (Inverse (father->GetRotation ()) * q);
		else 
			m_LocalRotation = Inverse (father->GetRotation ()) * q;
	}
	else
	{
		m_LocalPosition = p;
		if ( Safe )
			m_LocalRotation = NormalizeSafe (q);
		else
			m_LocalRotation = q;
	}
#if UNITY_EDITOR
	SyncLocalEulerAnglesHint ();
#endif
	
	ASSERT_ROTATION
	if ( Notify )
	{
		SetDirty ();
		SendTransformChanged ( kPositionChanged | kRotationChanged );
	}
}

void Transform::SetPositionAndRotationWithoutNotification (const Vector3f& p, const Quaternionf& q)
{
	SetPositionAndRotationInternal<false, false>( p, q );
}

void Transform::SetPositionAndRotationSafeWithoutNotification (const Vector3f& p, const Quaternionf& q)
{
	SetPositionAndRotationInternal<true, false>( p, q );
}

void Transform::SetPositionAndRotation (const Vector3f& p, const Quaternionf& q)
{
	SetPositionAndRotationInternal<false, true>( p, q );
}

void Transform::SetPositionAndRotationSafe (const Vector3f& p, const Quaternionf& q)
{
	SetPositionAndRotationInternal<true, true>( p, q );
}

void Transform::SetPositionWithoutNotification (const Vector3f& p)
{
	ABORT_INVALID_VECTOR3 (p, position, transform);
	Transform* father = GetParent ();
	if (father != NULL)
		m_LocalPosition = father->InverseTransformPoint (p);
	else
		m_LocalPosition = p;
}

void Transform::SetRotationWithoutNotification (const Quaternionf& q)
{
	Transform* father = GetParent ();
	if (father != NULL)
		m_LocalRotation = Inverse (father->GetRotation ()) * q;
	else
		m_LocalRotation = q;
}

void Transform::SetLocalPositionAndRotation (const Vector3f& p, const Quaternionf& q)
{
	ABORT_INVALID_VECTOR3 (p, localPosition, transform);
	ABORT_INVALID_QUATERNION (q, localRotation, transform);
	m_LocalPosition = p;
	m_LocalRotation = q;
	
	#if UNITY_EDITOR
	SyncLocalEulerAnglesHint ();
	#endif
	
	ASSERT_ROTATION

	SetDirty ();
	SendTransformChanged (kPositionChanged | kRotationChanged);
}
#if UNITY_EDITOR
void Transform::RegisterHierarchyChangedCallback (HierarchyChangedCallback* callback)
{
	gHierarchyChangedCallback = callback;
}

void Transform::RegisterHierarchyChangedSetParentCallback (HierarchyChangedCallbackSetParent* callback)
{
	gHierarchyChangedSetParentCallback = callback;
}
#endif

UInt32 Transform::CalculateSupportedMessages ()
{
	if (GetGameObject().WillHandleMessage(kTransformChanged))
		return kSupportsTransformChanged;
	else
		return 0;
}

void Transform::MakeEditorValuesLookNice()
{
	m_LocalScale = MakeNice(m_LocalScale);
	m_LocalPosition = MakeNice(m_LocalPosition);
}

void Transform::SupportedMessagesDidChange (int mask)
{
	Super::SupportedMessagesDidChange(mask);
	m_SupportsTransformChanged = mask & kSupportsTransformChanged;
}

void Transform::SendTransformChanged (int changeMask)
{
	bool parentingChanged = changeMask & kParentingChanged;

	// Fastpath if we don't support any TransformChanged callbacks on this game object
	if (!m_SupportsTransformChanged && !parentingChanged)
	{
		m_HasCachedTransformMatrix = false;
		m_HasChanged = true;

		TransformComList::iterator i;
		TransformComList::iterator end = m_Children.end ();
		for (i = m_Children.begin ();i != end;i++)
		{
			Transform* child = *i;
			child->SendTransformChanged (changeMask | kPositionChanged);
		}
		
		return;
	}
	
	m_HasCachedTransformMatrix = false;
	m_HasChanged = true;

	// NOTE: SetCacheDirty() doesn't need to be called here since SendTransformChanged traverses the transform hierarchy anyway

	GameObject& go = GetGameObject ();
	if (m_SupportsTransformChanged)
	{
		MessageData data;
		data.SetData (changeMask, ClassID (int));
		go.SendMessageAny (kTransformChanged, data);
	}
	
	if (parentingChanged)
		go.TransformParentHasChanged ();
	
	TransformComList::iterator i;
	TransformComList::iterator end = m_Children.end ();
	for (i = m_Children.begin ();i != end;i++)
	{
		Transform* child = *i;
		child->SendTransformChanged (changeMask | kPositionChanged);
	}
}

void Transform::SetCacheDirty()
{
	m_HasCachedTransformMatrix = false;
	m_HasChanged = true;
	
	TransformComList::iterator end = m_Children.end ();
	for (TransformComList::iterator i = m_Children.begin (); i != end; ++i)
		(*i)->SetCacheDirty();
}

void Transform::BroadcastMessageAny(const MessageIdentifier& messageID, MessageData& data)
{
	GameObject* go = GetGameObjectPtr ();
	if (go)
		go->SendMessageAny (messageID, data);
	
	TransformComList::iterator i;
	for (i = m_Children.begin ();i != m_Children.end ();i++)
		(**i).BroadcastMessageAny (messageID, data);
}

void Transform::RotateAroundLocal (const Vector3f& localAxis, float rad)
{
	AssertIf (!CompareApproximately (Magnitude (localAxis), 1.0F));

	Quaternionf q = AxisAngleToQuaternion (localAxis, rad);
	m_LocalRotation = Normalize (q * m_LocalRotation);
	#if UNITY_EDITOR
	SyncLocalEulerAnglesHint ();
	#endif
	
	SetDirty ();
	SendTransformChanged (kRotationChanged);
}

void Transform::RotateAroundLocalSafe (const Vector3f& localAxis, float rad)
{
	if (SqrMagnitude (localAxis) > Vector3f::epsilon)
		RotateAroundLocal (Normalize (localAxis), rad);
}

void Transform::RotateAround (const Vector3f& worldAxis, float rad)
{
	AssertIf (!CompareApproximately (Magnitude (worldAxis), 1.0F));

	Vector3f localAxis = InverseTransformDirection(worldAxis);

	Quaternionf q = AxisAngleToQuaternion (localAxis, rad);
	m_LocalRotation = Normalize (m_LocalRotation * q);
	#if UNITY_EDITOR
	SyncLocalEulerAnglesHint ();
	#endif
	
	SetDirty ();
	SendTransformChanged (kRotationChanged);
}

void Transform::RotateAroundSafe (const Vector3f& worldAxis, float rad)
{
	Vector3f localAxis = InverseTransformDirection(worldAxis);
	if (SqrMagnitude (localAxis) > Vector3f::epsilon)
	{
		localAxis = Normalize (localAxis);
		Quaternionf q = AxisAngleToQuaternion (localAxis, rad);
		m_LocalRotation = Normalize (m_LocalRotation * q);
		#if UNITY_EDITOR
		SyncLocalEulerAnglesHint ();
		#endif
	
		SetDirty ();
		SendTransformChanged (kRotationChanged);
	}	
}

Vector3f Transform::GetPosition () const
{
	Vector3f worldPos = m_LocalPosition;
	Transform* cur = GetParent ();
	while (cur)
	{
		worldPos.Scale (cur->m_LocalScale);
		worldPos = RotateVectorByQuat (cur->m_LocalRotation, worldPos);
		worldPos += cur->m_LocalPosition;
		
		cur = cur->GetParent ();
	}
	
	return worldPos;
}

Quaternionf Transform::GetRotation ()const
{
	Quaternionf worldRot = m_LocalRotation;
	Transform* cur = GetParent ();
	while (cur)
	{
		worldRot = cur->m_LocalRotation * worldRot;
		cur = cur->GetParent ();
	}
	
	return worldRot;
}


void Transform::GetPositionAndRotation (Vector3f& pos, Quaternionf& q) const
{
	Vector3f worldPos = m_LocalPosition;
	Quaternionf worldRot = m_LocalRotation;
	Transform* cur = GetParent ();
	while (cur)
	{
		worldPos.Scale (cur->m_LocalScale);
		worldPos = RotateVectorByQuat (cur->m_LocalRotation, worldPos);
		worldPos += cur->m_LocalPosition;
		
		worldRot = cur->m_LocalRotation * worldRot;
		
		cur = cur->GetParent ();
	}
	
	pos = worldPos;
	q = worldRot;
}

static TransformType DetectActualNegativeScale (int type, const Transform* transform)
{
	type &= ~kOddNegativeScaleTransform;

	// Calculate if we need to flip the back facing when rendering
	// We need to use backface rendering if one or three scale axes are negative (odd count)
	// In this case we enable kOddNegativeScaleTransform flag

	Transform* cur = (Transform *)transform;
	while (cur)
	{
		TransformType parentType = (TransformType)cur->m_InternalTransformType;
		// kOddNegativeScaleTransform is XOR against parent (odd+odd=even, odd+even=odd, even+even=even), other bits are OR
		type = ((type | parentType) & ~kOddNegativeScaleTransform) | ((type ^ parentType) & kOddNegativeScaleTransform);
		cur = cur->GetParent ();
	}

	return (TransformType)type;
}

static TransformType UpdateTransformType (TransformType type, const Transform* transform)
{
	if ((type & kOddNegativeScaleTransform) != 0)
		type = DetectActualNegativeScale (type, transform);

	// kNonUniformScaleTransform 'overwrites' kUniformScaleTransform
	if ((type & kNonUniformScaleTransform) != 0)
		type &= ~kUniformScaleTransform;

	Assert ((type & (kNonUniformScaleTransform|kUniformScaleTransform)) != (kNonUniformScaleTransform|kUniformScaleTransform));
	return type;
}


TransformType Transform::GetPositionAndRotationWithTransformType (Vector3f& worldPos, Quaternionf& worldRot) const
{
	TransformType type = (TransformType)m_InternalTransformType;
	
	worldPos = m_LocalPosition;
	worldRot = m_LocalRotation;
	Transform* cur = GetParent ();
	while (cur)
	{
		TransformType parentType = (TransformType)cur->m_InternalTransformType;
		// kOddNegativeScaleTransform is XOR against parent (odd+odd=even, odd+even=odd, even+even=even), other bits are OR
		type = ((type | parentType) & ~kOddNegativeScaleTransform) | ((type ^ parentType) & kOddNegativeScaleTransform);
		
		worldPos.Scale (cur->m_LocalScale);
		worldPos = RotateVectorByQuat (cur->m_LocalRotation, worldPos);
		worldPos += cur->m_LocalPosition;
		
		worldRot = cur->m_LocalRotation * worldRot;
		
		cur = cur->GetParent ();
	}
	
	// kNonUniformScaleTransform 'overwrites' kUniformScaleTransform
	if ((type & kNonUniformScaleTransform) != 0)
		type &= ~kUniformScaleTransform;
	Assert ((type & (kNonUniformScaleTransform|kUniformScaleTransform)) != (kNonUniformScaleTransform|kUniformScaleTransform));
	
	return type;
}

TransformType Transform::CalculateTransformMatrix (Matrix4x4f& transform) const
{
	//@TODO: Does this give any performance gain??
	Prefetch(m_CachedTransformMatrix.GetPtr());
	if (m_HasCachedTransformMatrix)
	{
		CopyMatrix(m_CachedTransformMatrix.GetPtr(), transform.GetPtr());
		return (TransformType)m_CachedTransformType;
	}

	const Transform* transforms[32];
	int transformCount = 1;
	TransformType type = (TransformType)0;
	Matrix4x4f temp;

	{
		// collect all transform that need CachedTransformMatrix update
		transforms[0] = this;
		Transform* parent = NULL;
		for (parent = GetParent(); parent != NULL && !parent->m_HasCachedTransformMatrix; parent = parent->GetParent())
		{
			transforms[transformCount++] = parent;
			// reached maximum of transforms that we can calculate - fallback to old method
			if (transformCount == 31)
			{
				parent = parent->GetParent();
				if (parent)
				{
					type = parent->CalculateTransformMatrixIterative(temp);
					Assert(parent->m_HasCachedTransformMatrix);
				}
				break;
			}
		}

		// storing parent of last transform (can be null), the transform itself won't be updated
		transforms[transformCount] = parent;
		Assert(transformCount <= 31);
	}		

	// iterate transforms from lowest parent
	for (int i = transformCount - 1; i >= 0; --i)
	{
		const Transform* t = transforms[i];
		const Transform* parent = transforms[i + 1];
		if (parent)
		{
			Assert(parent->m_HasCachedTransformMatrix);
			// Build the local transform into temp
			type |= t->CalculateLocalTransformMatrix(temp);				
			type |= (TransformType)parent->m_CachedTransformType;
			MultiplyMatrices4x4(&parent->m_CachedTransformMatrix, &temp, &t->m_CachedTransformMatrix);
		}
		else
		{
			// Build the local transform into m_CachedTransformMatrix
			type |= t->CalculateLocalTransformMatrix(t->m_CachedTransformMatrix);
		}
		// store cached transform
		t->m_CachedTransformType = UpdateTransformType(type, t);
		t->m_HasCachedTransformMatrix = true;
	}

	Assert(m_HasCachedTransformMatrix);
	CopyMatrix(m_CachedTransformMatrix.GetPtr(), transform.GetPtr());
	return (TransformType)m_CachedTransformType;
}


// This method doesn't cache all transforms - just the last one, but can calculate
// more than 32 transforms. CalculateTransformMatrix caches all results
TransformType Transform::CalculateTransformMatrixIterative (Matrix4x4f& transform) const
{
	if (m_HasCachedTransformMatrix)
	{
		CopyMatrix(m_CachedTransformMatrix.GetPtr(), transform.GetPtr());
		return (TransformType)m_CachedTransformType;
	}

	// Build the local transform
	TransformType type = CalculateLocalTransformMatrix(transform);

	Transform* parent = GetParent ();
	Matrix4x4f temp;
	while (parent != NULL)
	{
		if (parent->m_HasCachedTransformMatrix)
		{
			type |= (TransformType)parent->m_CachedTransformType;
			MultiplyMatrices4x4 (&parent->m_CachedTransformMatrix, &transform, &temp);
			// no need to iterate further - we got world transform
			parent = NULL;
		}
		else
		{
			Matrix4x4f parentTransform;
			type |= parent->CalculateLocalTransformMatrix(parentTransform);
			MultiplyMatrices4x4 (&parentTransform, &transform, &temp);
			parent = parent->GetParent ();
		}
		CopyMatrix (temp.GetPtr(), transform.GetPtr());
	}

	CopyMatrix(transform.GetPtr(), m_CachedTransformMatrix.GetPtr()) ;
	m_CachedTransformType = UpdateTransformType(type, this);
	m_HasCachedTransformMatrix = true;
	return type;
}


TransformType Transform::CalculateLocalTransformMatrix(Matrix4x4f& matrix) const
{
	TransformType type = (TransformType)m_InternalTransformType;
	if (type == kNoScaleTransform)
		matrix.SetTR(m_LocalPosition, m_LocalRotation);
	else
		matrix.SetTRS(m_LocalPosition, m_LocalRotation, m_LocalScale);
	return type;
}


TransformType Transform::CalculateTransformMatrixDisableNonUniformScale (Matrix4x4f& transform, Matrix4x4f& scaleOnly, float& uniformScale) const
{
	// Use CalculateTransformMatrix in order to take advantage of the cached transform.
	// Only non-uniform scaled meshes need to take the slower code path.
/*	TransformType cachedTransformType = CalculateTransformMatrix (transform);
	if (!IsNonUniformScaleTransform(cachedTransformType))
	{
		uniformScale = Magnitude(transform.GetAxisX());
		scaleOnly.SetIdentity();
		return cachedTransformType;
	}
*/

	// Build the local transform!
	TransformType type = (TransformType)m_InternalTransformType;
	uniformScale = m_LocalScale.x;
	if (type == kNoScaleTransform)
		transform.SetTR (m_LocalPosition, m_LocalRotation);
	else
		transform.SetTRS (m_LocalPosition, m_LocalRotation, m_LocalScale);

	// @TBD: cache parent transform and reuse it across CalculateTransformXXX funcs
	Transform* parent = GetParent ();
	Matrix4x4f temp;
	while (parent != NULL)
	{
		Matrix4x4f parentTransform;
		
		TransformType parentType = (TransformType)parent->m_InternalTransformType;
		// kOddNegativeScaleTransform is XOR against parent (odd+odd=even, odd+even=odd, even+even=even), other bits are OR
		type = ((type | parentType) & ~kOddNegativeScaleTransform) | ((type ^ parentType) & kOddNegativeScaleTransform);

		if (parentType == kNoScaleTransform)
			parentTransform.SetTR (parent->m_LocalPosition, parent->m_LocalRotation);
		else
			parentTransform.SetTRS (parent->m_LocalPosition, parent->m_LocalRotation, parent->m_LocalScale);
		uniformScale *= parent->m_LocalScale.x;
		
		MultiplyMatrices4x4 (&parentTransform, &transform, &temp);
		CopyMatrix (temp.GetPtr(), transform.GetPtr());
		parent = parent->GetParent ();
	}
	
	// kNonUniformScaleTransform 'overwrites' kUniformScaleTransform
	if ((type & kNonUniformScaleTransform) != 0)
		type &= ~kUniformScaleTransform;
	DebugAssert ((type & (kNonUniformScaleTransform|kUniformScaleTransform)) != (kNonUniformScaleTransform|kUniformScaleTransform));

	// uniform or no scale
	if (!IsNonUniformScaleTransform(type))
	{
		scaleOnly.SetIdentity();
		return type;
	}
	// non-uniform scale
	else
	{
		// Calculate scaleOnlyMatrix (In order to scale the mesh with the non-uniform scale)
		Matrix4x4f worldToLocalMatrixNoScale;
		GetWorldToLocalMatrixNoScale (worldToLocalMatrixNoScale);
		MultiplyMatrices4x4(&worldToLocalMatrixNoScale, &transform, &scaleOnly);
		scaleOnly.Get (0,3) = 0.0F;
		scaleOnly.Get (1,3) = 0.0F;
		scaleOnly.Get (2,3) = 0.0F;

		// Calculate the matrix without any scale applied
		GetLocalToWorldMatrixNoScale (transform);
		uniformScale = 1.0F;
		
		return type;
	}
}


TransformType Transform::CalculateTransformMatrixDisableScale (Matrix4x4f& matrix) const
{
	Vector3f worldPos;
	Quaternionf worldRot;
	TransformType type = GetPositionAndRotationWithTransformType(worldPos, worldRot);
	
	matrix.SetTR (worldPos, worldRot);
		return type;
}

TransformType Transform::CalculateTransformMatrixScaleDelta (Matrix4x4f& m) const
{
	Matrix4x4f scaledMatrix;
	TransformType type = CalculateTransformMatrix (scaledMatrix);
	// Affine matrix?
	if ((type & (kUniformScaleTransform | kNonUniformScaleTransform)) == 0)
	{
		m.SetIdentity ();
		return type;
	}
	else
	{
		Matrix4x4f tmp;
		GetWorldToLocalMatrixNoScale (tmp);
		MultiplyMatrices4x4(&tmp, &scaledMatrix, &m);
		m.Get (0,3) = 0.0F;
		m.Get (1,3) = 0.0F;
		m.Get (2,3) = 0.0F;
		return type;
	}
}

Matrix4x4f Transform::GetWorldToLocalMatrixNoScale () const
{
	Matrix4x4f m;
	GetWorldToLocalMatrixNoScale(m);
	return m;
}

const Matrix4x4f& Transform::GetWorldToLocalMatrixNoScale (Matrix4x4f& m) const
{
	Vector3f pos;
	Quaternionf rot;
	GetPositionAndRotation(pos, rot);
	m.SetTRInverse (pos, rot);
	return m;
}

Matrix4x4f Transform::GetLocalToWorldMatrixNoScale () const
{
	Matrix4x4f m;
	GetLocalToWorldMatrixNoScale(m);
	return m;
}

const Matrix4x4f& Transform::GetLocalToWorldMatrixNoScale (Matrix4x4f& m) const
{
	Quaternionf rot; Vector3f pos;
	GetPositionAndRotation(pos, rot);
	m.SetTR (pos, rot);
	return m;
}

Matrix4x4f Transform::GetWorldToLocalMatrix () const
{
	Matrix4x4f m, temp;
	m.SetTRInverse (m_LocalPosition, m_LocalRotation);
	if (m_InternalTransformType != kNoScaleTransform)
	{
		Matrix4x4f scale;
		scale.SetScale (InverseSafe (m_LocalScale));
		MultiplyMatrices4x4 (&scale, &m, &temp);
		CopyMatrix (temp.GetPtr(), m.GetPtr());
	}
	
	Transform* father = GetParent ();
	if (father != NULL)
	{
		Matrix4x4f parentMat = father->GetWorldToLocalMatrix();
		MultiplyMatrices4x4 (&m, &parentMat, &temp);
		CopyMatrix (temp.GetPtr(), m.GetPtr());
	}
	
	return m;
}


Matrix4x4f Transform::GetLocalToWorldMatrix () const
{
	Matrix4x4f m;
	CalculateTransformMatrix (m);
	return m;
}

Vector3f Transform::TransformDirection (const Vector3f& inDirection) const
{
	return RotateVectorByQuat (GetRotation (), inDirection);
}

Vector3f Transform::InverseTransformDirection (const Vector3f& inDirection) const
{
	return RotateVectorByQuat (Inverse(GetRotation()), inDirection);
}

Vector3f Transform::InverseTransformPoint (const Vector3f& inPosition) const
{
	Vector3f newPosition, localPosition;
	Transform* father = GetParent ();
	if (father)
		localPosition = father->InverseTransformPoint (inPosition);
	else
		localPosition = inPosition;

	localPosition -= m_LocalPosition;
	newPosition = RotateVectorByQuat (Inverse(m_LocalRotation), localPosition);
	if (m_InternalTransformType != kNoScaleTransform)
		newPosition.Scale (InverseSafe (m_LocalScale));

	return newPosition;
}

Vector3f Transform::TransformPoint (const Vector3f& inPoint) const
{
	Vector3f worldPos = inPoint;

	const Transform* cur = this;
	while (cur)
	{
		worldPos.Scale (cur->m_LocalScale);
		worldPos = RotateVectorByQuat (cur->m_LocalRotation, worldPos);
		worldPos += cur->m_LocalPosition;
		
		cur = cur->GetParent ();
	}
	return worldPos;
}

template<class TransferFunction> inline
void Transform::Transfer (TransferFunction& transfer) 
{
	Super::Transfer (transfer);
	TRANSFER_SIMPLE (m_LocalRotation);
	TRANSFER_SIMPLE (m_LocalPosition);
	TRANSFER_SIMPLE (m_LocalScale);

	//TRANSFER_EDITOR_ONLY_HIDDEN (m_LocalEulerAnglesHint);

	// This needs to be here since eg. Mesh collider queries the recalculate transform type.
	// and awakefromload might not have been called already.
	if (transfer.IsReading())
		RecalculateTransformType ();
	
	// When cloning objects for prefabs and instantiate, we don't use serialization to duplicate the hierarchy,
	// we duplicate the hierarchy directly
	if (SerializePrefabIgnoreProperties(transfer))
	{
		transfer.Transfer (m_Children, "m_Children", kHideInEditorMask | kStrongPPtrMask | kIgnoreWithInspectorUndoMask);	
		transfer.Transfer (m_Father, "m_Father", kHideInEditorMask | kIgnoreWithInspectorUndoMask);
	}

#if ENABLE_EDITOR_HIERARCHY_ORDERING
	TRANSFER_EDITOR_ONLY_HIDDEN(m_Order);
#endif
}

void Transform::RecalculateTransformType ()
{
	// #pragma message ("Compare approximately is bad due to epsilon changing with the size of the value")
	if (CompareApproximately (m_LocalScale.x, m_LocalScale.y, 0.0001F) && CompareApproximately (m_LocalScale.y, m_LocalScale.z, 0.0001F))
	{
		if (CompareApproximately( m_LocalScale.x, 1.0F, 0.0001F ))
		{
			m_InternalTransformType = kNoScaleTransform;
		}
		else 
		{
			m_InternalTransformType = kUniformScaleTransform;
			if (m_LocalScale.x < 0.0F) 
			{
				m_InternalTransformType = kOddNegativeScaleTransform | kNonUniformScaleTransform;
			}
		}
	}
	else
	{
		m_InternalTransformType = kNonUniformScaleTransform;

		int hasOddNegativeScale = m_LocalScale.x * m_LocalScale.y * m_LocalScale.z < 0.0F ? 1 : 0;
		m_InternalTransformType |= (TransformType)(hasOddNegativeScale * kOddNegativeScaleTransform);
	}
}

void Transform::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);
	SetCacheDirty();
	
	// Only call SendTransformChanged if it was really changed eg. 
	// by a propepertyeditor or datatemplate propagation but not if it was loaded from disk
	
	if ((awakeMode & kDidLoadFromDisk) == 0)
	{
		// This is for all kinds of non-serialization Awakes.
		// eg. animation. Because Transfer already recalculates
		RecalculateTransformType ();

		SendTransformChanged (kPositionChanged | kRotationChanged | kScaleChanged);
	}
	
	#if UNITY_EDITOR
	if (gHierarchyChangedCallback)
		gHierarchyChangedCallback (this);
	#endif
}

inline void MakeValidFloat (float* f)
{
	if (!IsFinite (*f))
		*f = 0.0F;
}

void Transform::CheckConsistency ()
{
	Super::CheckConsistency ();
	MakeValidFloat (&m_LocalRotation.x);
	MakeValidFloat (&m_LocalRotation.y);
	MakeValidFloat (&m_LocalRotation.z);
	MakeValidFloat (&m_LocalRotation.w);
	MakeValidFloat (&m_LocalPosition.x);
	MakeValidFloat (&m_LocalPosition.y);
	MakeValidFloat (&m_LocalPosition.z);
	MakeValidFloat (&m_LocalScale.x);
	MakeValidFloat (&m_LocalScale.y);
	MakeValidFloat (&m_LocalScale.z);
	m_LocalRotation = NormalizeSafe (m_LocalRotation);
	#if UNITY_EDITOR
	SyncLocalEulerAnglesHint ();
	#endif
	
	// Check if father has this as child
	Transform* father = m_Father;
	if (father)
	{
		if ( father->Find(this) == father->m_Children.end ())
			father->m_Children.push_back (this);
	}

	// Check if all children are available and have this as father.  Also make
	// sure that any of our children is on the list exactly once.
	for (int i=0;i<m_Children.size ();i++)
	{
		Transform* child = m_Children[i];
		Assert(child != this);
		
		if (child == NULL)
		{
			ErrorStringObject ("CheckConsistency: Transform child can't be loaded", this);
			iterator it = m_Children.begin () + i;
			m_Children.erase (it);
			i--;
			continue;
		}

		Transform* parent = child->m_Father;
		
		#if UNITY_EDITOR
		// We only try to fix the parent pointer in the Editor as we don't want to risk breaking existing players.
		if (parent == NULL) 
		{
			child->m_Father = this;
			ErrorStringObject ("CheckConsistency: Restored Transform child parent pointer from NULL", child);
			continue;
		}
		#endif
		
		if (parent != this)
		{
			iterator it = m_Children.begin () + i;
			m_Children.erase (it);
			i--;
			ErrorStringObject ("CheckConsistency: Transform child has another parent", child);
			continue;
		}

		// Look for and remove multiple occurrences.
		bool occursMultipleTimes = false;
		for (int j = i + 1; j < m_Children.size ();)
		{
			Transform* otherChild = m_Children[j];
			if (otherChild == child)
			{
				occursMultipleTimes = true;
				m_Children.erase (m_Children.begin () + j);
			}
			else
				++j;
		}
		if (occursMultipleTimes)
		{
			ErrorStringObject ("CheckConsistency: Transform child is linked multiple times to parent; removed extraneous links from parent", child);
		}
	}
}
#if ENABLE_EDITOR_HIERARCHY_ORDERING
void Transform::SetOrder(SInt32 order)
{
	m_Order = order; 
	SetDirty(); 

	if (gHierarchyChangedCallback)
		gHierarchyChangedCallback (this);

	SendTransformChanged (kParentingChanged);
}

int Transform::CompareDepths(Transform* lhs, Transform* rhs)
 {
	bool orderCompare = lhs->GetOrder() != rhs->GetOrder();
	if (orderCompare)
		return lhs->GetOrder() < rhs->GetOrder();
	else
	{
		const char* lhsName = lhs ? lhs->GetName() : "";
		const char* rhsName = rhs ? rhs->GetName() : "";
		return SemiNumericCompare(lhsName, rhsName) < 0;
	}
 }

void Transform::OrderChildrenRecursively()
{
	std::sort(m_Children.begin(), m_Children.end(), Transform::CompareDepths);

	for (int i = 0; i < m_Children.size(); ++i)
	{
		Transform* childTrans = m_Children[i];
		childTrans->OrderChildrenRecursively();
	}
}
#endif

IMPLEMENT_OBJECT_SERIALIZE (Transform)
IMPLEMENT_CLASS (Transform)

static inline int FindSeperator (const char* in)
{
	const char* c = in;
	while (*c != '/' && *c != '\0')
		c++;
	return c - in;
}

Transform* FindRelativeTransformWithPath (Transform& transform, const char* path)
{
	LIMIT_RECURSION (2000, NULL);
	
	if (path[0] == '\0')
		return &transform;
	
	int seperator = FindSeperator (path);
	
	if (path[0] == '/')
		return FindActiveTransformWithPath (path);
	else if (path[0] == '.' && path[1] == '.')
	{
		Transform* parent = transform.GetParent();
		if (path[2] == '/')
		{
			if (parent)
				return FindRelativeTransformWithPath (*parent, path + 3);
			else
				return NULL;
		}			
		else if (path[2] == '\0')
			return parent;
	}

	for (Transform::iterator i=transform.begin ();i != transform.end ();i++)
	{
		Transform& child = **i;
		
		const char* name = child.GetName();
		
		// Early out if size is not the same
		if (strlen(name) != seperator)
			continue;
		
		// continue if the name isn't the same
		const char* n = name;
		int j;
		for (j=0;j<seperator;j++,n++)
			if (path[j] != *n)
				break;
		if (j != seperator)
			continue;
		
		// We found the transform we were searching for
		if (path[seperator] == '\0')
			return &child;
		// Recursively find in the children
		else
		{
			Transform* result = FindRelativeTransformWithPath (child, path + seperator + 1);
			if (result != NULL) 
				return result;
		}
	}
	return NULL;
}

string CalculateTransformPath (const Transform& transform, const Transform* to)
{
	string path;
	const Transform* cur = &transform;
	while (cur != to && cur != NULL)
	{
		if (!path.empty ())
			path = cur->GetName () + ('/' + path);
		else
			path = cur->GetName ();
		cur = cur->GetParent ();
	}
	return path;
}

void AppendTransformPath (string& path, const char* appendName)
{
	if (path.empty())
		path = appendName;
	else
	{
		path += '/';
		path += appendName;
	}
}

void AppendTransformPath (UnityStr& path, const char* appendName)
{
	if (path.empty())
		path = appendName;
	else
	{
		path += '/';
		path += appendName;
	}
}



Transform* FindActiveTransformWithPath (const char* path)
{
	bool needsToBeRoot = path[0] == '/';
	if (path[0] == '/')
		path++;
	
	if (path[0] == 0)
		return NULL;

	GameObjectList::iterator i;

	GameObjectList& tagged = GetGameObjectManager().m_TaggedNodes;
	for (i=tagged.begin();i != tagged.end();i++)
	{	
		Transform* transform = FindActiveTransformWithPathImpl(path, **i, needsToBeRoot);
		if (transform)
			return transform;
	}

	GameObjectList& active = GetGameObjectManager().m_ActiveNodes;
	for (i=active.begin();i != active.end();i++)
	{	
		Transform* transform = FindActiveTransformWithPathImpl(path, **i, needsToBeRoot);
		if (transform)
			return transform;
	}
	
	return NULL;
/*

	for (int i=0;i<gos.size ();i++)
	{
		if (!gos[i]->IsActive ())
			continue;
			
		const string& name = gos[i]->GetName ();
		if (name.find (path, 0, name.size ()) == 0)
		{
			path += name.size ();
			if (path[0] == '/')
				path ++;
				
			Transform* transform = gos[i]->QueryComponent (Transform);
			if (transform)
			{
				if (needsToBeRoot && transform->GetParent())
					continue;

				if (path[0] == 0)
					return transform;
				transform = FindRelativeTransformWithPath (*transform, path);
				if (transform)
					return transform;
			}
		}
	}
	
	return NULL;
*/	
}

static Transform* FindActiveTransformWithPathImpl (const char* path, GameObject& go, bool needsToBeRoot)
{
	AssertIf(!go.IsActive());

	const char* name = go.GetName ();
	size_t size = strlen(name);
	
	if (strncmp(name, path, size) == 0)
	{
		path += size;
		if (path[0] == '/')
			path ++;
			
		Transform* transform = go.QueryComponent (Transform);
		if (transform)
		{
			if (needsToBeRoot && transform->GetParent())
				return NULL;

			if (path[0] == 0 && transform->IsActive())
				return transform;
				
			transform = FindRelativeTransformWithPath (*transform, path);
			if (transform && transform->IsActive())
				return transform;
		}
	}
	return NULL;
}

Transform& Transform::GetRoot ()
{
	Transform* cur = this;
	Transform* curParent = NULL;
	while ((curParent = cur->GetParent ()) != NULL)
		cur = curParent;

	return *cur;
}

bool IsChildOrSameTransform(Transform& transform, Transform& inParent)
{
	Transform* child = &transform;
	while (child)
	{
		if (child == &inParent)
			return true;
		child = child->GetParent();
	}
	return false;
}

#if ENABLE_EDITOR_HIERARCHY_ORDERING
void Transform::GetSortedChildList (Transform::TransformComList& sortedChildren) const
{
	sortedChildren.assign(m_Children.begin(), m_Children.end());
	std::sort(sortedChildren.begin(), sortedChildren.end(), Transform::CompareDepths);
}
#endif

void Transform::SetLocalTRS (const Vector3f& pos, const Quaternionf& rot, const Vector3f& scale)
{
	ABORT_INVALID_VECTOR3 (pos, localPosition, transform);
	ABORT_INVALID_QUATERNION (rot, localRotation, transform);
	m_LocalRotation = NormalizeSafe(rot);
	m_LocalPosition = pos;
	m_LocalScale = scale;
	#if UNITY_EDITOR
	SyncLocalEulerAnglesHint ();
	#endif
	RecalculateTransformType();
	SendTransformChanged(kPositionChanged | kRotationChanged | kScaleChanged);
}

int GetTransformDepth(Transform& transform)
{
	Transform* parent = transform.GetParent();
	int depth = 0;
	while (parent)
	{
		depth++;
		parent = parent->GetParent();
	}
	return depth;	
}

Transform* FindTransformWithName(Transform* root, const char* name)
{
	if (strcmp(root->GetName(), name) == 0)
		return root;
	else
	{
		for (int i = 0; i < root->GetChildrenCount(); i++)
		{
			Transform* ret = FindTransformWithName(&(root->GetChild(i)), name);
			if (ret)
				return ret;
		}
		return NULL;
	}
}
