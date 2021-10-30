#pragma once

#include "Runtime/Serialize/TypeTree.h"
#include "Runtime/Math/Color.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Utilities/dynamic_bitset.h"
#include "Runtime/Serialize/SerializeTraits.h"
#include "Runtime/Math/AnimationCurve.h"
#include "Editor/Src/Prefabs/PropertyModification.h"
#include "Editor/Src/Utility/SerializedPropertyPath.h"
#include "InspectorMode.h"

class GradientNEW;
class SerializedObject;

// This MUST be kept synchronized with the enum in SerializedPropertyBindings.txt
enum { 
	kOtherValue = -1, kIntValue = 0, kBoolValue = 1, kFloatValue = 2, kStringValue = 3, 
	kColorValue = 4, kPPtrValue = 5, kLayerMaskValue = 6, kEnumValue =  7,
	kVector2Value = 8, kVector3Value = 9, kRectValue = 10, kArraySize = 11, 
	kAnimationCurveValue = 13, kBoundsValue = 14, kGradientValue = 15, kQuaternionValue = 16,
};

typedef std::map<std::string, std::map<std::string, int> > ScriptEnumPopup;
class Vector3f;



class SerializedProperty
{
	typedef std::vector<SerializedPropertyStack> PropertyStack;
	
	unsigned				  m_SerializedObjectVersion;
	dynamic_array<UInt8>*     m_Data;
	PPtr<Object>			  m_Object;
	PropertyModifications*    m_PrefabModifications;
	std::string               m_PropertyPath;
	TypeTree*                 m_TypeTree;
	int                       m_ByteOffset;
	PropertyStack             m_Stack;
	SerializedObject*         m_SerializedObject;
	bool                      m_EditableObject;
	bool                      m_ComponentAddedToPrefabInstance;
	bool                      m_IsInstantiatedPrefab;
	
	void MarkPropertyModified (std::string path = "");
	
	typedef std::map<std::string, std::map<int, std::string> > ScriptEnumPopup;
	
	SInt32 RemapInstanceID (SInt32 instanceID);
	
	UInt8* GetDataPtr (int offset) const { return &(*m_Data)[offset]; }
	UInt8* GetDataPtrNoCheck (int offset) const { return m_Data->begin() + offset; }
	bool NextInternal (bool enterChildren, bool onlyVisibleOptimization);
	int GetSerializedPropertyType (TypeTree* typeTree) const;

	bool IsVisible (const TypeTree& typeTree) const;
	void Rewind ();
	
	public:

	inline const TypeTree& GetTypeTree () const;
	inline TypeTree& GetTypeTree ();
	inline const TypeTree& GetTypeTreeUnchecked () const;
	
	bool Next (bool enterChildren);
	bool NextVisible (bool enterChildren);
	
	int GetDepth () const;

	/// Does property have any children at all?
	bool HasChildren () const { return !m_TypeTree->m_Children.empty(); }

	/// Does the property have any children that are not marked kHideInEditorMask
	bool HasVisibleChildren () const;

	UInt32 HasMultipleDifferentValues () const;

	const char*  GetMangledName () const;
	
	int          GetSerializedPropertyType () const;

	/// Get/Set Int value
	int          GetIntValue () const;
	void         SetIntValue (int value);

	/// Get/Set Boolean value
	bool         GetBoolValue () const;
	void         SetBoolValue (bool value);

	/// Get/Set float value
	float        GetFloatValue () const;
	void        SetFloatValue (float value);

	/// Get/Set String value
	std::string  GetStringValue () const;
	void  SetStringValue (const std::string& val);

	/// Get/Set Color
	ColorRGBAf   GetColorValue () const;
	void   SetColorValue (const ColorRGBAf& c);	

	/// Get/Set AnimationCurve
	AnimationCurve* GetAnimationCurveValueCopy ();
	void SetAnimationCurveValue (const AnimationCurve* c);
	
	/// Get/Set GradientNEW
	GradientNEW* GetGradientValueCopy ();
	void SetGradientValue (const GradientNEW* g);

	/// Get / Set PPtr of Property (Setter performs dynamic typechecking to disallow illegal assignments)
	PPtr<Object> GetPPtrValue () const;
	void SetPPtrValue (PPtr<Object> pptr);

	/// Append a PPtr property to a foldout in the inspector
	void AppendFoldoutPPtrValue (PPtr<Object> value);

	/// Get / Set value of a Property
	template<typename T> void SetValue (const T &value);
	template<typename T> T GetValue () const;

	/// Get tooltips
	std::string GetTooltip () const;
	
	/// Checks if the value can be assigned to the property (Performs type checks)
	bool ValidatePPtrValue (PPtr<Object> value);
	
	std::string GetPPtrClassName ();

	// The displayed string value of the PPtr
	std::string  GetPPtrStringValue () const;

	/// Get/Set enum by string value
	std::string  GetEnumStringValue () const;
	void  SetEnumStringValue (std::string value);

	/// Get/Set the enum by index into GetEnumNames array
	void  SetEnumValueIndex (int index);
	int  GetEnumValueIndex () const;

	// Is the property currently driven by an animationclip in animation mode
	bool IsAnimated () const;
	
	/// The names of all enum values that the user can pick
	std::vector<std::string> GetEnumNames () const;
	
	
	bool IsArray () const;
	int GetArraySize () const;
	void InsertArrayElementAtIndex (int index);
	void DeleteArrayElementAtIndex (int index);
	bool GetArrayElementAtIndex (int index);
	
	bool MoveArrayElement (int src, int dst);
	
	bool FindRelativeProperty (const std::string& relativePropertyPath);
	
	std::string  GetLayerMaskStringValue () const;
	std::vector<std::string> GetLayerMaskNames () const;
	std::vector<int> GetLayerMaskSelectedIndex () const;

	void ToggleLayerMaskAtIndex (int index);
	void SetBitAtIndexForAllTargetsImmediate (int index, bool value);
	
	const std::string& GetPropertyPath () const;
	void GetArrayIndexLessPropertyPath (std::string& propertyPath) const;

	std::string GetParentArrayPropertyPath () const;

	bool GetIsExpanded ();
	void SetIsExpanded (bool value);
	int  CountRemaining ();
	int  CountInProperty ();
	
	// Find the property with propertyPath
	bool FindProperty (const std::string& propertyPath, TypeTree* typeTreeHint = NULL);
	
	bool GetPrefabOverride () const;
	void SetPrefabOverride (bool override);

	bool GetIsInstantiatedPrefab ();

	bool DuplicateCommand ();
	bool DeleteCommand ();

	bool GetEditable () const;
	
	bool IsPrefabInstance ();

	void ApplySerializedData (dynamic_array<UInt8> const& data);

	friend bool operator == (SerializedProperty& lhs, SerializedProperty& rhs);
	
	// Resizes the array to newArraySize (The property must either be an array or the array.size)
	void ResizeArray (int newArraySize);
	
	void SyncSerializedObjectVersion ();
	
	SerializedObject* GetSerializedObject () { return m_SerializedObject; }
	friend class SerializedObject;
};

class SerializedObject
{ 
	// m_SerializedObjectVersion is used to keep the SerializedProperty in sync, in case an array modifies a serialized object and you still have other SerializedProperties
	// that were created using FindProperty.
	unsigned                  m_SerializedObjectVersion;
	UInt32                    m_ObjectDirtyIndex;
	ScriptEnumPopup           m_ScriptPopupMenus;
	bool                      m_ScriptPopupDirty;
	bool					  m_IsDifferentCacheDirty;
	std::vector<PPtr<Object> > m_Objects;
	dynamic_array<UInt8>      m_Data;
	int                       m_InstanceIDOfTypeTree;
	bool                      m_DidModifyProperty;
	bool                      m_DidModifyPrefabModifications;
	InspectorMode             m_InspectorMode;
	bool                      m_ShowAllProperties;
	bool                      m_IsInstantiatedPrefab;
	int						  m_SerializeFlags;
	vector_set<UnityStr>      m_Expanded;
	std::map<std::string, UInt32> m_IsDifferent;
	std::map<std::string, int> m_MinArraySizes;
	vector_set<std::string>   m_Modified;

	/// Type tree for the first object in m_Objects.
	TypeTree m_TypeTree;

	/// When objects in m_Objects are not all of the same type, we need
	/// more than one type tree to compare objects.  We generate these
	/// trees as needed and cache them here.
	typedef std::map<int, TypeTree*> AlternateTypeTreeMap;
	AlternateTypeTreeMap m_AlternateTypeTrees;

	PPtr<Object>	GetFirstObject() { return m_Objects[0]; }
	void Init (std::vector<PPtr<Object> >& objects);
	void UpdateIsDifferentCache ();

	void GetIteratorInternal (SerializedProperty& property, TypeTree* typeTree, dynamic_array<UInt8>* objData, Object* object);
	TypeTree* GetTypeTreeForMultiEdit (Object& object);
	
	void PrepareUndoOfPropertyChanges();

	void InitIsDifferentCache ();
	bool ValidateObjectReferences ();
	
	PropertyModifications     m_PrefabModifications;
	
public:
	
	bool IsEditingMultipleObjects () { return m_Objects.size() > 1; }
	
	SerializedObject() { m_InspectorMode = kNormalInspector; m_IsInstantiatedPrefab = false; m_SerializedObjectVersion = 0; m_InstanceIDOfTypeTree = 0; }
	~SerializedObject ();
	
	void Init (Object& object);
	void Init (std::vector<Object*>& objs);
	
	void Update ();
	
	bool DidTypeTreeChange ();

	void UpdateIfDirtyOrScript ();
	void SetIsDifferentCacheDirty () { m_IsDifferentCacheDirty = true; }

	bool ApplyModifiedProperties ();
	bool ApplyModifiedPropertiesWithoutUndo ();

	void MarkPropertyModified (const std::string& propertyPath);
	
	bool HasModifiedProperties () const { return m_DidModifyProperty; }
	
	void SetInspectorMode (InspectorMode debug);
	InspectorMode GetInspectorMode () const { return m_InspectorMode; }
	
	void GetIterator (SerializedProperty& iterator);
	void PrepareMultiEditBufferAndIterator (Object& targetObject, SerializedProperty& property, dynamic_array<UInt8>& data);
	
	Object* GetTargetObject ();
	const std::vector<PPtr<Object> > GetTargetObjects () { return m_Objects; }
	
	/// Returns a enum string -> value representation mapping for the enum represented by typeTree.
	const std::map<std::string, int>* GetPopupMenuData (const TypeTree& typeTree);

	// Access to the expanded state
	void SetIsExpanded (const std::string& expanded, bool state);
	bool GetIsExpanded (const std::string& expanded);
	
	bool ExtractPropertyModification (PropertyModification& modification) const;
	bool SetPropertyModification (const PropertyModification& modification) ;
	
	void CopyFromSerializedProperty (const SerializedProperty& srcProperty);
	
	friend class SerializedProperty;
};

template<typename T>
T SerializedProperty::GetValue () const
{
	if (SerializeTraits<T>::GetTypeString(NULL) != GetTypeTree ().m_Type) 
		ErrorString ("Mismatched types in GetValue - return value is junk");
	
	void* data = &(*m_Data)[m_ByteOffset];
	return *reinterpret_cast<T*> (data);
}

template<typename T>
void SerializedProperty::SetValue (const T& c)
{
	void* data = &(*m_Data)[m_ByteOffset];
	
	if (SerializeTraits<T>::GetTypeString(NULL) != GetTypeTree ().m_Type)
	{
		ErrorString ("Mismatched types in SetValue");
		return;
	}
	
	if (GetValue<T> () == c)
		return;

	*reinterpret_cast<T*> (data) = c;
	MarkPropertyModified();
}

inline const TypeTree& SerializedProperty::GetTypeTreeUnchecked () const
{
	return *m_TypeTree;
}


inline const TypeTree& SerializedProperty::GetTypeTree () const
{
	Assert (m_SerializedObject->m_SerializedObjectVersion == m_SerializedObjectVersion);
	return *m_TypeTree;
}

inline TypeTree& SerializedProperty::GetTypeTree ()
{
	Assert (m_SerializedObject->m_SerializedObjectVersion == m_SerializedObjectVersion);
	return *m_TypeTree;
}
