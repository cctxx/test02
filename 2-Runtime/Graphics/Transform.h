#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "Runtime/Math/Quaternion.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Utilities/EnumFlags.h"
#include "Runtime/Modules/ExportModules.h"
using std::vector; //TODO:refactor

/** Transformcomponent stores rotation and position of objects
 *  A Transformcomponent can also have child Transformcomponent's, 
 *  which then rotate and translate relative to their father.

 *  Local space is the space which is relative to its father while world space is an absolute space
 *  Local and Worldspace is the same if the transformcomponent does not have a father.
 *  The localspace functions are faster than the worldspace functions
 */

class EXPORT_COREMODULE Transform : public Unity::Component
{
	public:
	
	typedef dynamic_array<ImmediatePtr<Transform> >	TransformComList;
	typedef TransformComList::iterator	iterator;
	
	typedef void TransformChangedCallback (Transform* t);
	typedef void HierarchyChangedCallback (Transform* t);
	typedef void HierarchyChangedCallbackSetParent (Transform* obj, Transform* oldParent, Transform* newParent);
	
private:
	Quaternionf                      m_LocalRotation;
	Vector3f                         m_LocalPosition;
	Vector3f                         m_LocalScale;

	mutable Matrix4x4f               m_CachedTransformMatrix;
	mutable UInt8                    m_CachedTransformType;
	mutable UInt8                    m_HasCachedTransformMatrix;
	mutable UInt8                    m_HasChanged;
	
public:	
	// This tracks kNoScaleTransform, kUniformScaleTransform and kNonUniformScaleTransform, kHasNegativeScale
	UInt8                            m_InternalTransformType;
	UInt8                            m_SupportsTransformChanged;

	TransformComList                 m_Children;
	ImmediatePtr<Transform>          m_Father;

	#if UNITY_EDITOR

	#if ENABLE_EDITOR_HIERARCHY_ORDERING
	typedef std::pair<std::string, int> VisibleRootSecondaryKey;
	typedef std::pair<SInt32, VisibleRootSecondaryKey> VisibleRootKey;	
	#else
	typedef std::pair<std::string, int> VisibleRootKey;	
	#endif
	
	struct CompareVisibleRoots
	{
		bool operator() (const VisibleRootKey& lhs, const VisibleRootKey& rhs) const;
	};
	typedef std::map<VisibleRootKey, Transform*, CompareVisibleRoots> VisibleRootMap;
	VisibleRootMap::iterator         m_VisibleRootIterator;
	bool                             m_VisibleRootValid;
	Vector3f                         m_LocalEulerAnglesHint;
	#endif

	#if ENABLE_EDITOR_HIERARCHY_ORDERING
	SInt32							 m_Order;
	#endif

public:

	REGISTER_DERIVED_CLASS (Transform, Component)
	DECLARE_OBJECT_SERIALIZE (Transform)
	
	Transform (MemLabelId label, ObjectCreationMode mode);
	// virtual ~Transform (); declared-by-macro

	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);
	virtual void CheckConsistency ();
	virtual void SupportedMessagesDidChange (int mask);

	// Tag class as sealed, this makes QueryComponent faster.
	static bool IsSealedClass ()				{ return true; }
	
	/// Returns a ptr to the father transformcomponent (NULL if no father)
	Transform *GetParent () const 	{ return m_Father; }
	/// Returns a reference to the root transform (top most transform with no parent)
	Transform& GetRoot ();	

	/// Finds given transform
	iterator Find( const Transform* child );
	
	/// access to the children
	int GetChildrenCount ()const 						{ return m_Children.size (); }
	Transform &GetChild (int i) const 			{ Assert (i < (int)m_Children.size()); return *m_Children[i]; }
	iterator begin ()									{ return m_Children.begin (); }
	iterator end ()										{ return m_Children.end (); }
	
	/// Sets the father to p(if p is invalid the Transformcomponent will have no father)
	/// Returns false if the father could not be set
	/// This happens only if you are trying to set the father to one of its direct/indirect children.
	enum SetParentOption { kWorldPositionStays = 1 << 0, kLocalPositionStays = 1 << 1, kAllowParentingFromPrefab = 1 << 2, kDisableTransformMessage = 1 << 3 };
	bool SetParent (Transform * parent, SetParentOption options = kWorldPositionStays);

	/// Sets the rotation in local space
	void SetLocalRotation (const Quaternionf& rotation);
	/// Sets the Rotation in world space
	void SetRotation (const Quaternionf& rotation);
	/// Sets the local euler angles
	void SetLocalEulerAngles (const Vector3f& eulerAngles);
	
	/// Sets the position in local space
	/// (If the object has no father, localspace is basically the same as world space)
	void SetLocalPosition (const Vector3f& inTranslation);
	/// Sets the position in world space
	void SetPosition (const Vector3f& position);

	/// Sets the position - a local space offset will be scaled, rotated and subtracted from the position.
	/// Used to set the position For CharacterController and NavMeshAgent that have baseOffset / center.
	/// For retreiving the position with a local offset use: TransformPoint (localOffset)
	void SetPositionWithLocalOffset (const Vector3f& p, const Vector3f& localOffset);

	/// Transforms local space position to world space - while applying an offset which is scaled and rotated accordingly.
	Vector3f TransformPointWithLocalOffset (const Vector3f& p, const Vector3f& localOffset) const;
	
	/// Same as above but normalizes the quaternion
	void SetLocalRotationSafe (const Quaternionf& rotation);
	void SetRotationSafe (const Quaternionf& rotation);
	
	/// Sets the scale in local space
	void SetLocalScale (const Vector3f& scale);
	/// Sets the scale from a rotation * scale
	/// The transform can not hold the full scale if it contains skew
	void SetWorldRotationAndScale (const Matrix3x3f& worldRotationAndScale);

	/// Sets the world position and rotation	
	void SetPositionAndRotation (const Vector3f& position, const Quaternionf& rotation);
	void SetLocalPositionAndRotation (const Vector3f& position, const Quaternionf& rotation);

	/// Return matrix that converts a point from World To Local space
	Matrix4x4f GetWorldToLocalMatrix () const;
	/// Return matrix that converts a point from Local To World space
	Matrix4x4f GetLocalToWorldMatrix () const;
	
	Matrix4x4f GetWorldToLocalMatrixNoScale () const;
	const Matrix4x4f& GetWorldToLocalMatrixNoScale (Matrix4x4f& m) const;
	Matrix4x4f GetLocalToWorldMatrixNoScale () const;
	const Matrix4x4f& GetLocalToWorldMatrixNoScale (Matrix4x4f& m) const;

	TransformType CalculateTransformMatrix (Matrix4x4f& matrix) const;

	TransformType CalculateTransformMatrixScaleDelta (Matrix4x4f& matrix) const;

	TransformType CalculateTransformMatrixDisableNonUniformScale (Matrix4x4f& matrix, Matrix4x4f& scaleOnly, float& scale) const;
	TransformType CalculateTransformMatrixDisableScale (Matrix4x4f& matrix) const;

	/// Whether the transform has changed since the last time this flag was set to 'false'.
	bool GetChangedFlag () { return m_HasChanged; }
	/// Sets the flag indicating whether the transform has changed. Most commonly used to simply set it to 'false'.
	void SetChangedFlag (bool val) { m_HasChanged = val; }

	/// Gets the rotation from local to world space
	Quaternionf GetRotation () const;
	/// Gets the local rotation
	Quaternionf GetLocalRotation () const { return m_LocalRotation; }
	/// Gets the local euler angles (in the editor it is first ensures that they are in sync with the local rotation quaternion)
	Vector3f GetLocalEulerAngles ();
	
	/// Gets the local position relative to the father
	Vector3f GetLocalPosition () const {return m_LocalPosition;}
	/// Gets the position in world space
	Vector3f GetPosition () const;
	
	/// Returns the local scale	
	Vector3f GetLocalScale () const { return m_LocalScale; }
	/// Returns the world rotation and scale.
	/// (It is impossible to return a Vector3 because the scale might be skewed)
	Matrix3x3f GetWorldRotationAndScale () const;
	
	Matrix3x3f GetWorldScale () const;
	
	Vector3f GetWorldScaleLossy () const;
	
	/// Rotates the transform around axis by rad
	void RotateAroundLocal (const Vector3f& localAxis, float rad);
	/// Same, but normalizes the axis for you
	void RotateAroundLocalSafe (const Vector3f& localAxis, float rad);
	/// Rotates the transform around axis by rad
	void RotateAround (const Vector3f& worldAxis, float rad);
	/// Same, but normalizes the axis for you
	void RotateAroundSafe (const Vector3f& worldAxis, float rad);
	
	
	/// transforms a point from localspace to worldspace
	Vector3f TransformPoint (const Vector3f& inPoint) const;
	/// Transforms a direction from localspace to worldspace
	/// (Ignores scale)
	Vector3f TransformDirection (const Vector3f& inDirection) const;

	/// Transforms a point from worldspace to localspace
	Vector3f InverseTransformPoint (const Vector3f& inDirection) const;
	/// Transforms a direction from worldspace to localspace
	/// (Ignores scale)
	Vector3f InverseTransformDirection (const Vector3f& inDirection) const;

	#if UNITY_EDITOR
	/// Register a function which is called whenever a transformcomponent is changed
	static void RegisterHierarchyChangedCallback (HierarchyChangedCallback* callback);
	static void RegisterHierarchyChangedSetParentCallback (HierarchyChangedCallbackSetParent* callback);
	
	VisibleRootMap::iterator* GetVisibleRootIterator () { return m_VisibleRootValid ? &m_VisibleRootIterator : NULL; }
	void SetVisibleRootIterator (const VisibleRootMap::iterator& it) { m_VisibleRootIterator = it; m_VisibleRootValid = true; }
	void ClearVisibleRootIterator () { m_VisibleRootValid = false; }
	#endif // End UNITY_EDITOR

	#if ENABLE_EDITOR_HIERARCHY_ORDERING
	static int CompareDepths(Transform* lhs, Transform* rhs);

	SInt32 GetOrder () const                  { return m_Order; }
	void SetOrder (SInt32 order);
	void OrderChildrenRecursively();
	void GetSortedChildList (TransformComList& sortedChildren) const;
	#endif

	TransformComList& GetChildrenInternal () { return m_Children; }
	const TransformComList& GetChildrenInternal () const { return m_Children; }
	ImmediatePtr<Transform>& GetParentPtrInternal () { return m_Father; }

	/// Reset position&rotation
	void Reset ();

	/// Sets the world position and rotation without sending out a TransformChanged message to the gameobject and without setting dirty
	/// (Not sending TransformChanged will result in the Renderer 
	/// not updating the AABB to reflect the transform change)
	void SetPositionAndRotationWithoutNotification (const Vector3f& position, const Quaternionf& q);
	void SetPositionWithoutNotification (const Vector3f& position);
	void SetRotationWithoutNotification (const Quaternionf& q);
	void SetPositionAndRotationSafeWithoutNotification (const Vector3f& p, const Quaternionf& q);

	void SetPositionAndRotationSafe (const Vector3f& p, const Quaternionf& q);

	void GetPositionAndRotation (Vector3f& pos, Quaternionf& q)const;
	TransformType GetPositionAndRotationWithTransformType (Vector3f& worldPos, Quaternionf& worldRot) const;
	
	/// You seldomly want to call this function yourself.
	/// Sends the transform changed message to itself and all children.
	/// A bitmask specifies which components have changed!
	/// kParentingChanged is also called when SetOrder(SInt32) is called. (Editor Only)
	enum { kPositionChanged = 1 << 0, kRotationChanged = 1 << 1, kScaleChanged = 1 << 3, kAnimatePhysics = 1 << 4, kParentingChanged = 1 << 5};
	void SendTransformChanged (int mask);

	/// private but used by datatemplates
	void RemoveFromParent ();
	void ClearChildrenParentPointer ();
	void ClearChild (Transform* child);
	
	/// Broadcasts a message to this and all child transforms
	void BroadcastMessageAny(const MessageIdentifier& message, MessageData& data);
	
	void SetLocalTRS (const Vector3f& pos, const Quaternionf& rot, const Vector3f& scale);

	inline void SetLocalRotationWithoutNotification (const Quaternionf& rotation)
	{
		m_LocalRotation = rotation;
		#if UNITY_EDITOR
		SyncLocalEulerAnglesHint ();
		#endif
	}
	
	inline void SetLocalPositionWithoutNotification (const Vector3f& inTranslation)
	{
		m_LocalPosition = inTranslation;
	}
	
	inline void SetLocalScaleWithoutNotification (const Vector3f& scale)
	{
		m_LocalScale = scale;
		RecalculateTransformType ();
	}

	// For use only by the animation system	
	void RecalculateTransformType ();

	UInt32 CalculateSupportedMessages ();

	void	MakeEditorValuesLookNice();

private:
	
	friend class AnimationBinder;
	friend void SampleAnimation (GameObject& go, class AnimationClip& clip, float inTime, int wrapMode, float deltaTime);

	TransformType CalculateLocalTransformMatrix(Matrix4x4f& matrix) const;
	TransformType CalculateTransformMatrixIterative (Matrix4x4f& matrix) const;
	
	void SetCacheDirty();
	
	template<bool Safe, bool Notify>
	void SetPositionAndRotationInternal( const Vector3f& position, const Quaternionf& rotation );
	
	#if UNITY_EDITOR
	/// Makes the local euler angles be in sync with the quaternion rotation
	void SyncLocalEulerAnglesHint ();
	#endif
};

ENUM_FLAGS(Transform::SetParentOption);

Transform* FindRelativeTransformWithPath (Transform& transform, const char* path);

Transform* FindActiveTransformWithPath (const char* path);

Transform* FindTransformWithName (Transform* root, const char* name);

std::string CalculateTransformPath (const Transform& transform, const Transform* to);
void AppendTransformPath (string& path, const char* appendName);
void AppendTransformPath (UnityStr& path, const char* appendName);

/// Is transform a child of parent? Or is the transform the same.
bool IsChildOrSameTransform(Transform& transform, Transform& parent);

EXPORT_COREMODULE int GetTransformDepth(Transform& transform);


#endif
