#include "UnityPrefix.h"
#include "SerializedProperty.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Serialize/SerializationMetaFlags.h"
#include "Runtime/Serialize/IterateTypeTree.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Editor/Src/AutoDocumentation.h"
#include "ObjectNames.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/BaseClasses/Tags.h"
#include "Editor/Src/Undo/Undo.h"
#include "Editor/Src/EditorExtensionImpl.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Src/SceneInspector.h"
#include "Editor/Src/InspectorExpandedState.h"
#include <iterator>
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Gradient.h"
#include "Editor/Src/Prefabs/PropertyModification.h"
#include "Runtime/Scripting/ScriptPopupMenus.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Src/Animation/AnimationModeSnapshot.h"

#include "Runtime/Serialize/CacheWrap.h"// for Align4 -> Move it somewhere else. This is weird

using namespace std;

SerializedObject::~SerializedObject ()
{
	// Delete all our cached type trees.
	for (AlternateTypeTreeMap::iterator iter = m_AlternateTypeTrees.begin ();
		 iter != m_AlternateTypeTrees.end (); ++iter)
	{
		UNITY_DELETE (iter->second, kMemTypeTree);
	}
}

int ExtractScriptInstanceID(Object* object)
{
	MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*> (object);
	if (behaviour)
		return behaviour->GetScript().GetInstanceID();
	else
		return 0;
}


void SerializedObject::Init (std::vector<PPtr<Object> >& objects)
{
	m_ScriptPopupDirty = true;
	m_IsDifferentCacheDirty = true;
	m_DidModifyProperty = false;
	m_DidModifyPrefabModifications = false;
	m_SerializedObjectVersion++;
	m_Objects = objects;
	m_ObjectDirtyIndex = 0xFFFFFFFF;
	
	m_SerializeFlags = kSerializeForInspector;
	if (m_InspectorMode >= kDebugInspector)
		m_SerializeFlags |= kSerializeDebugProperties | kIgnoreDebugPropertiesForIndex;
	
	GenerateTypeTree (*GetFirstObject(), &m_TypeTree, m_SerializeFlags);
	m_InstanceIDOfTypeTree = ExtractScriptInstanceID(GetFirstObject());
	
	InspectorExpandedState::ExpandedData* data = GetInspectorExpandedState().GetExpandedData(GetFirstObject());
	if (data)
		m_Expanded.assign(data->m_ExpandedProperties.begin(), data->m_ExpandedProperties.end());
	else
		m_Expanded.clear();
	
	Update ();
}

void SerializedObject::Init (std::vector<Object*>& objs)
{
	std::vector<PPtr<Object> > objects;
	for (int i=0; i<objs.size(); i++)
		objects.push_back(objs[i]);
	Init (objects);
}

void SerializedObject::Init (Object& object)
{
	std::vector<PPtr<Object> > objects;
	objects.push_back(&object);
	Init (objects);
}

void SerializedObject::SetInspectorMode (InspectorMode mode)
{
	if (m_InspectorMode != mode)
	{
		m_InspectorMode = mode;
		Init (m_Objects);
	}
}

void SerializedObject::UpdateIfDirtyOrScript ()
{
	Object* target = m_Objects.empty () ? NULL : m_Objects[0];
	UInt32 dirtyIndex = target ? target->GetPersistentDirtyIndex() : 0;
	if (dirtyIndex != m_ObjectDirtyIndex || target == NULL || target->GetNeedsPerObjectTypeTree ())
		Update();
}

bool SerializedObject::ValidateObjectReferences ()
{
	if (m_Objects.empty())
	{
		ErrorString("SerializedObject target has been destroyed.");
		return false;
	}
	
	for (int i=0;i<m_Objects.size();i++)
	{
		if (!m_Objects[i].IsValid())
		{
			ErrorString("SerializedObject target has been destroyed.");
			return false;
		}
	}
	
	return true;
}

bool SerializedObject::DidTypeTreeChange ()
{
	return m_InstanceIDOfTypeTree != ExtractScriptInstanceID(GetFirstObject());
}

void SerializedObject::Update ()
{
	if (!ValidateObjectReferences ())
		return;
	
	if (DidTypeTreeChange())
	{
		Init(m_Objects);
		return;
	}

	m_Modified.clear();
	m_DidModifyProperty = false;

	WriteObjectToVector (*m_Objects[0], &m_Data, m_SerializeFlags);		
	m_ObjectDirtyIndex = m_Objects[0]->GetPersistentDirtyIndex();
	
	Prefab* prefab = GetPrefabFromAnyObjectInPrefab(m_Objects[0]);
	if (prefab)
		m_PrefabModifications = prefab->GetPropertyModifications();
	else
		m_PrefabModifications.clear();
	
	m_SerializedObjectVersion++;
		
	if (m_IsDifferentCacheDirty)
		UpdateIsDifferentCache();
}

TypeTree* SerializedObject::GetTypeTreeForMultiEdit (Object& object)
{
	Object* firstObject = GetFirstObject ();
	TypeTree* typeTree = &m_TypeTree;

	// We generally assume we can use the type tree of the first object for any
	// other object we have.  However, for objects that turn out to not be of the
	// same type as the first object, we need to generate a type tree specific to
	// them.
	int classIdOfObject = object.GetClassID ();
	if (classIdOfObject != firstObject->GetClassID())
	{
		typeTree = m_AlternateTypeTrees[classIdOfObject];
		if (!typeTree)
		{
			typeTree = UNITY_NEW (TypeTree, kMemTypeTree);
			GenerateTypeTree (object, typeTree, m_SerializeFlags);
			m_AlternateTypeTrees[classIdOfObject] = typeTree;
		}
	}
	return typeTree;
}

void SerializedObject::GetIterator (SerializedProperty& property)
{
	GetIteratorInternal(property, &m_TypeTree, &m_Data, GetFirstObject());
}

void SerializedObject::GetIteratorInternal (SerializedProperty& property, TypeTree* typeTree, dynamic_array<UInt8>* objData, Object* object)
{
	property.m_Data = objData;
	property.m_Object = object;
	property.m_PrefabModifications = &m_PrefabModifications;
	property.m_TypeTree = typeTree;
	property.m_ByteOffset = 0;
	property.m_SerializedObject = this;
	property.m_Stack.clear();
	property.m_PropertyPath.clear();
	property.m_PropertyPath.reserve(512);
	property.m_EditableObject = object != NULL ? !object->TestHideFlag(Object::kNotEditable) : false;
	property.m_ComponentAddedToPrefabInstance = IsComponentAddedToPrefabInstance(object);
	property.m_IsInstantiatedPrefab = IsPrefabInstanceWithValidParent(object) | property.m_ComponentAddedToPrefabInstance;
	property.m_SerializedObjectVersion = m_SerializedObjectVersion;
}

void SerializedObject::PrepareMultiEditBufferAndIterator (Object& targetObject, SerializedProperty& property, dynamic_array<UInt8>& data)
{
	WriteObjectToVector (targetObject, &data, m_SerializeFlags);				
	GetIteratorInternal(property, GetTypeTreeForMultiEdit (targetObject), &data, &targetObject);
}

void SerializedObject::UpdateIsDifferentCache ()
{
	m_IsDifferent.clear();
	m_MinArraySizes.clear();
	
	if (!ValidateObjectReferences())
		return;
	
	// Compare all objects starting from the second to the
	// first object in the array.
	for (int i=1; i < m_Objects.size(); i++)
	{
		SerializedProperty src;
		SerializedProperty cmp;

		// Set up iterator over properties of first object.
		GetIterator(src);
		
		// Serialize current object into memory.
		dynamic_array<UInt8> serializedCmpData (kMemTempAlloc);
		PrepareMultiEditBufferAndIterator (*m_Objects[i], cmp, serializedCmpData);
		
		// If the two objects happen to not be of the same exact type (meaning we have two different
		// type trees), we need to compare types as well as property paths to make sure we're not
		// comparing apples to oranges.
		const bool needToComparePropertyTypes = &src.GetTypeTree () != &cmp.GetTypeTree ();

		// Walk through all pairs of properties in the two objects.
		while (true)
		{	
			// Go to next property in first object.
			if (!src.Next (true))
				break;

			// Find the corresponding property in the i-th object.  In general, the order of
			// any properties present in both objects should always be the same and we iterate
			// arrays to the same length, so everything else we can just skip.
			bool exit = false;
			while (cmp.GetPropertyPath() != src.GetPropertyPath() ||
				   (needToComparePropertyTypes && cmp.GetTypeTree().m_Type != src.GetTypeTree().m_Type))
			{ 
				if (!cmp.Next(true))
				{
					exit = true;
					break;
				}
			}

			// If we've exhausted all properties of the current object, we're done.
			if (exit)
				break;
			
			const string& path = src.GetPropertyPath();
			TypeTree& type = src.GetTypeTree();
			
			if (type.m_IsArray)
			{
				const int kMaxArraySizeForMultiEditing = 64;
				SInt32 srcArraySize = *reinterpret_cast<SInt32*> (src.GetDataPtr(src.m_ByteOffset));
				SInt32 cmpArraySize = *reinterpret_cast<SInt32*> (cmp.GetDataPtr(cmp.m_ByteOffset));
				int minSize = std::min (srcArraySize, cmpArraySize);
				// If the array is too large for efficient multi-editing, 
				// don't show it at all, to avoid confusion about it's size.
				if (cmpArraySize > kMaxArraySizeForMultiEditing || srcArraySize > kMaxArraySizeForMultiEditing)
					minSize = 0;
				
				// Assign min size of the array. So after going through all objects, m_MinArraySizes [foundPath]
				// will contain the shortest array size or kMaxArraySizeForMultiEditing (whatever is smaller).
				// That way it is always possible to iterate through the SerializedObject and only get properties 
				// found in all objects.
				std::map<std::string, int>::iterator entry = m_MinArraySizes.find (path);
				if (entry != m_MinArraySizes.end())
					m_MinArraySizes [path] = std::min (minSize, entry->second);
				else
					m_MinArraySizes [path] = minSize;
			}
			
			std::map<std::string, UInt32>::iterator entry = m_IsDifferent.find(path);
			
			bool needToSetAnchestorsDifferent = false;
			if (type.m_MetaFlag & kGenerateBitwiseDifferences)
			{
				UInt32* srcData = (UInt32*)src.GetDataPtr(src.m_ByteOffset);
				UInt32* cmpData = (UInt32*)cmp.GetDataPtr(cmp.m_ByteOffset);
				UInt32 diff = (*srcData) ^ (*cmpData);
				if (diff != 0 && (entry == m_IsDifferent.end() || entry->second == 0))
					needToSetAnchestorsDifferent = true;
				if (entry == m_IsDifferent.end())
					m_IsDifferent [path] = diff;
				else
					entry->second |= diff;
			}
			else 
			{
				UInt32 isDifferent = entry != m_IsDifferent.end() && entry->second;
				if (!isDifferent)
				{
					SInt32 size = type.m_ByteSize;
					
					UInt8* srcData = src.GetDataPtr(src.m_ByteOffset);
					UInt8* cmpData = cmp.GetDataPtr(cmp.m_ByteOffset);
					if (size != -1 && memcmp(srcData, cmpData, size) != 0)
					{
						m_IsDifferent [path] = true;
						needToSetAnchestorsDifferent = true;
					}
					else
						m_IsDifferent [path] = false;
				}
			}
			
			if (needToSetAnchestorsDifferent)
			{
				int index = FindTypeTreeSeperator(path.c_str());
				while (index < path.size())
				{
					string partialPath = path.substr(0, index);
					m_IsDifferent[partialPath] = true;
					index = FindTypeTreeSeperator(path.c_str() + index + 1) + index + 1;
				}
			}
		}
	}
	m_IsDifferentCacheDirty = false;
}


static void ApplyToObject (Object* object, const dynamic_array<UInt8>& data, TypeTree& typeTree, int serializeFlags)
{
	if (object == NULL)
		return;
	
	if (object->GetNeedsPerObjectTypeTree ())
		ReadObjectFromVector (object, data, typeTree, serializeFlags);
	else
		ReadObjectFromVector (object, data, serializeFlags);
	
	object->CheckConsistency ();
	object->AwakeFromLoad (kDefaultAwakeFromLoad);
	object->SetDirty ();
}

bool SerializedObject::ApplyModifiedProperties ()
{
	if (!ValidateObjectReferences ())
		return false;
	
	if( m_DidModifyProperty )
		PrepareUndoOfPropertyChanges();
	
	return ApplyModifiedPropertiesWithoutUndo ();
}


bool SerializedObject::ApplyModifiedPropertiesWithoutUndo ()
{
	if (!ValidateObjectReferences ())
		return false;
	
	bool res = m_DidModifyProperty || m_DidModifyPrefabModifications;
	
	if( m_DidModifyProperty )
	{
		// Copy changes to other properties
		SerializedProperty srcProperty;
		GetIterator(srcProperty);
		for (int i=1; i < m_Objects.size(); i++)
		{				
			SerializedProperty dstProperty;
			dynamic_array<UInt8> cmpData (kMemTempAlloc);
			PrepareMultiEditBufferAndIterator (*m_Objects[i], dstProperty, cmpData);
	
			// Apply all changes
			for (vector_set<std::string>::iterator modified = m_Modified.begin(); modified != m_Modified.end(); modified++)
			{
				srcProperty.Rewind();
				if (srcProperty.FindProperty(*modified))
				{
					const SInt32 size = srcProperty.GetTypeTreeUnchecked().m_ByteSize;
					
					// Synchronize multi-edit data that might contain arrays
					if (size == -1)
					{
						dstProperty.Rewind();
						if (!dstProperty.FindProperty(*modified) && dstProperty.GetEditable())
							continue;
						
						int srcStart = srcProperty.m_ByteOffset;
						int dstStart = dstProperty.m_ByteOffset;
						int srcEnd = srcStart;
						int dstEnd = dstStart;
						WalkTypeTree(srcProperty.GetTypeTreeUnchecked(), srcProperty.GetDataPtr(0), &srcEnd);
						WalkTypeTree(dstProperty.GetTypeTreeUnchecked(), dstProperty.GetDataPtr(0), &dstEnd);
						
						int srcSize = srcEnd - srcStart;
						int dstSize = dstEnd - dstStart;
						
						if (srcSize > dstSize)
						{
							dynamic_array<UInt8> tempData(srcSize - dstSize, 0, kMemTempAlloc);
							dstProperty.m_Data->insert(dstProperty.m_Data->begin() + dstStart + dstSize, tempData.begin(), tempData.end());
						}
						else if (srcSize < dstSize)
							dstProperty.m_Data->erase(dstProperty.m_Data->begin() + dstStart + srcSize, dstProperty.m_Data->begin() + dstStart + dstSize);
						
						memcpy(dstProperty.GetDataPtr(dstStart), srcProperty.GetDataPtr(srcStart), srcSize);
					}
					// Array size is handled specially
					else if (IsTypeTreeArraySize(srcProperty.GetTypeTreeUnchecked()))
					{
						dstProperty.Rewind();
						if (dstProperty.FindProperty(*modified) && dstProperty.GetEditable())
							dstProperty.ResizeArray (ExtractArraySize(m_Data, srcProperty.m_ByteOffset));
					}
					// Apply any simple non-array data
					else if (size > 0)
					{
						dstProperty.Rewind();
						if (dstProperty.FindProperty(*modified) && dstProperty.GetEditable())
							memcpy(dstProperty.GetDataPtr(dstProperty.m_ByteOffset), srcProperty.GetDataPtr(srcProperty.m_ByteOffset), size);
					}	
				}
			}

			ApplyToObject(m_Objects[i], *dstProperty.m_Data, m_TypeTree, m_SerializeFlags);
		}

		ApplyToObject(m_Objects[0], m_Data, m_TypeTree, m_SerializeFlags);

		m_Modified.clear();
		m_DidModifyProperty = false;

		// If we are modifying m_Script then this can change the typetree
		// The multi-edit code reads the actual object right away which can at this point be out of sync with the typetree
		// Thus we have to do an Update (which will implicitly reubuild the typetree)
		if (DidTypeTreeChange ())
			Update();
		else
			UpdateIsDifferentCache	();
	}
	
	///@TODO: This needs to be made to work with multi-object editing after the prefab branch is merged into trunk
	if (m_DidModifyPrefabModifications)
	{	
		Prefab* prefab = GetPrefabFromAnyObjectInPrefab(m_Objects[0]);
		if (prefab)
			SetPropertyModifications(*prefab, m_PrefabModifications);
		m_DidModifyPrefabModifications = false;
	}
	
	return res;
}

void SerializedObject::MarkPropertyModified (const std::string& propertyPath)
{
	m_DidModifyProperty = true;
	m_Modified.insert (propertyPath);
	std::map<std::string, UInt32>::iterator found = m_IsDifferent.find(propertyPath);
	if (found != m_IsDifferent.end())
		found->second = false;
	
	m_SerializedObjectVersion++;
}

const map<string, int>* SerializedObject::GetPopupMenuData (const TypeTree& typeTree)
{
	if (typeTree.m_Father == NULL)
		return NULL;
	
	if (m_ScriptPopupDirty)
	{
		m_ScriptPopupMenus.clear ();
		MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*>(GetFirstObject());
		if (behaviour)
			BuildScriptPopupMenus (*behaviour, m_ScriptPopupMenus);
		m_ScriptPopupDirty = false;
	}

	// Use field name to find the right script popup menu
	if (!m_ScriptPopupMenus.empty())
	{
		string name = GetFieldIdentifierForEnum(&typeTree);
		
		ScriptEnumPopup::iterator found = m_ScriptPopupMenus.find (name);
		if (found != m_ScriptPopupMenus.end ())
			return &found->second;	
	}
	
	return GetVariableParameter (typeTree.m_Father->m_Type, typeTree.m_Name);
}

void SerializedObject::SetIsExpanded (const std::string& expanded, bool state)
{
	if (state)
		m_Expanded.insert(expanded);
	else
		m_Expanded.erase(expanded);
	
	GetInspectorExpandedState().SetExpandedData(GetFirstObject(), m_Expanded.get_vector());	
}

bool SerializedObject::GetIsExpanded (const std::string& expanded)
{
	return m_Expanded.count (expanded);
}

bool SerializedObject::ExtractPropertyModification (PropertyModification& modification) const
{
	return ExtractPropertyModificationValueFromBytes (m_TypeTree, m_Data.begin(), modification);
}

bool SerializedObject::SetPropertyModification (const PropertyModification& modification)
{
	if (ApplyPropertyModification(m_TypeTree, m_Data, modification))
	{
		MarkPropertyModified (modification.propertyPath);
		return true;
	}
	return false;
}

bool SerializedProperty::IsArray () const
{
	return IsTypeTreeArrayOrArrayContainer(*m_TypeTree);
}

int SerializedProperty::GetArraySize ()const 
{
	if (IsArray ())
	{
		SInt32 size = *reinterpret_cast<const SInt32*> (GetDataPtr(m_ByteOffset));
		return size;
	}
	else
	{
		ErrorString("Retrieving array size but no array was provided");
		return 0;
	}
}

void SerializedProperty::InsertArrayElementAtIndex (int index)
{
	SerializedProperty property (*this);

	if (property.GetArraySize() == index)
		property.ResizeArray(index + 1);
	else if (property.GetArrayElementAtIndex(index))
		property.DuplicateCommand();
}

void SerializedProperty::DeleteArrayElementAtIndex (int index)
{
	SerializedProperty property (*this);
	if (property.GetArrayElementAtIndex(index))
		property.DeleteCommand();
}

bool SerializedProperty::GetArrayElementAtIndex (int index)
{
	if (!IsArray ())
	{
		ErrorString("Retrieving array element but no array was provided");
		return false;
	}
	int size = GetArraySize();

	if (index >= size)
	{
		ErrorString("Retrieving array element that was out of bounds");
		return false;
	}

	// - Go to size element
	Next(true);
	// There might be an array container in that case we skip over it too
	if (!IsTypeTreeArraySize(*m_TypeTree))
		Next(true);

	// Skip over size element
	Assert(IsTypeTreeArraySize(*m_TypeTree));
	Next(false);

	// Skip elements to reach index
	for (int i=0;i < index;i++)
		Next(false);
	
	return true;
}


bool SerializedProperty::NextVisible (bool enterChildren)
{
	while (true)
	{
		// Try next element
		bool success = NextInternal (enterChildren, true);
		if (success)
		{
			// Element is not visible
			if (!IsVisible(*m_TypeTree))
				continue;

			// Element is an array (We already display the parent, so skip this)
			if (m_TypeTree->m_IsArray)
				continue;

			return true;
		}
		// Last element -> exit
		else
		{
			return false;
		}
	}
}

bool SerializedProperty::Next (bool enterChildren)
{
	return NextInternal (enterChildren, false);
}


bool SerializedProperty::NextInternal (bool enterChildren, bool onlyVisibleOptimization)
{
	SerializedPropertyStack* index = m_Stack.empty() ? NULL : &m_Stack.back();
	
	bool enter = !m_TypeTree->m_Children.empty() && enterChildren;
	bool visible = !onlyVisibleOptimization || IsVisible(*m_TypeTree);
	
	if (enter && visible)
	{
		SerializedPropertyStack newProperty;
		newProperty.iterator = m_TypeTree->m_Children.begin();
		newProperty.arrayIndex = -1;
		newProperty.arrayByteOffset = -1;
		newProperty.multipleObjectsMinArraySize = -1;

		TypeTree* newTypeTree = &*newProperty.iterator;
		
		// We are entering the size element of an array
		// Setup up array iteration
		if (m_TypeTree->m_IsArray)
		{
			newProperty.arrayIndex = 0;
			newProperty.arrayByteOffset = m_ByteOffset;
			
			std::map<std::string, int>::iterator entry = m_SerializedObject->m_MinArraySizes.find (m_PropertyPath);
			if (entry != m_SerializedObject->m_MinArraySizes.end())
				newProperty.multipleObjectsMinArraySize = entry->second;
		}

		AddPropertyNameToBufferSerializedProperty (m_PropertyPath, *newTypeTree);

		m_Stack.push_back(newProperty);
		m_TypeTree = newTypeTree;
	}
	else
	{
		if (index == NULL)
		{
			DebugAssertIf(!m_PropertyPath.empty());
			DebugAssertIf(!m_Stack.empty());
			if (m_ByteOffset == 0)
			{
				AssertString ("Invalid iteration - (You need to call Next (true) on the first element to get to the first element)");
			}
			else if (m_ByteOffset != -1)
			{
				AssertString ("Invalid iteration - (You need to stop calling Next when it returns false)");
			}
			return false;
		}
		
		// When an array is zero size we can not iterate over any elements of course.
		// So skip those when byteoffsetting
		bool isZeroArray = false;
		if (index->arrayByteOffset != -1)
		{
			SInt32 arraySize = ExtractArraySize(*m_Data, index->arrayByteOffset);
			if (index->multipleObjectsMinArraySize != -1)
				arraySize = index->multipleObjectsMinArraySize;
			if (arraySize == 0)
				isZeroArray = true;
		}

		// Skip over this properties data
		WalkTypeTree(*m_TypeTree, &(*m_Data)[0], &m_ByteOffset);
		
		TypeTree* parent = m_TypeTree->m_Father;
		index->iterator++;
		
		// Zero size arrays are a bit special, because we have to immediately skip the element data
		if (isZeroArray && m_TypeTree == &*parent->begin())
		{
			SInt32 arraySize = *reinterpret_cast<SInt32*> (&(*m_Data)[index->arrayByteOffset]);
			m_TypeTree = &*index->iterator;
			
			while (index->arrayIndex < arraySize)
			{
				WalkTypeTree(*m_TypeTree, &(*m_Data)[0], &m_ByteOffset);
				index->arrayIndex++;
			}
			index->iterator++;	
		}
		
		// When iterator reaches end, step out and select parent
		while (index->iterator == parent->m_Children.end())
		{
			// Last property of array element, we might have to continue iterating the array
			if (index->arrayByteOffset != -1)
			{
				SInt32 arraySize = ExtractArraySize(*m_Data, index->arrayByteOffset);
				index->arrayIndex++;
				
				// If this array is shorter in one of the other selected objects, 
				// just ignore the rest and iterate to the end of it.
				if (index->multipleObjectsMinArraySize != -1 && index->arrayIndex >= index->multipleObjectsMinArraySize)
				{
					while (index->arrayIndex < arraySize)
					{
						WalkTypeTree(*m_TypeTree, &(*m_Data)[0], &m_ByteOffset);
						index->arrayIndex++;
					}
				}

				// Keep iterating through array
				if (index->arrayIndex < arraySize)
				{
					index->iterator--;
					
					break;
				}

				// When we have iterated over all arrayelements, we go out of the stack as usual
			}

			ClearPropertyNameFromBufferSerializedProperty(m_PropertyPath);

			m_Stack.pop_back();
			if (m_Stack.empty())
				return false;
			
			index = &m_Stack.back();
			parent = index->iterator->m_Father;

			if (index->iterator->m_MetaFlag & kAlignBytesFlag)
				m_ByteOffset = Align4(m_ByteOffset);

			m_TypeTree = &*index->iterator;
			index->iterator++;
		}

		m_TypeTree = &*index->iterator;
		ClearPropertyNameFromBufferSerializedProperty(m_PropertyPath);
		
		if (index->arrayByteOffset != -1)
			AddPropertyArrayIndexToBufferSerializedProperty(m_PropertyPath, index->arrayIndex);
		else
			AddPropertyNameToBufferSerializedProperty(m_PropertyPath, *m_TypeTree);
	}

	// Make sure the offset we return is within range.
	if (m_TypeTree->m_ByteSize >= 0)
	{
		Assert (m_ByteOffset + m_TypeTree->m_ByteSize <= m_Data->size ());
	}
	else
	{
		Assert (m_ByteOffset < m_Data->size ());
	}

	return true;
}

const char* SerializedProperty::GetMangledName () const
{
	const SerializedPropertyStack* top = m_Stack.empty() ? NULL : &m_Stack.back();

	// Are we in the first level of an array and exclude the first element because thats always the size
	if (top && IsTypeTreeArrayElement(*m_TypeTree))
	{
		static char temp[256];
		// If first value is a string, use that as the name of the property
		if (!m_TypeTree->m_Children.empty() && m_TypeTree->begin()->m_Type == "string")
		{
			int stringByteOffset = m_ByteOffset + sizeof(SInt32);
			SInt32& size = *reinterpret_cast<SInt32*> (&(*m_Data)[m_ByteOffset]);
			if (size != 0)
			{
				char* stringData = reinterpret_cast<char*> (&(*m_Data)[stringByteOffset]);
				memcpy(temp, stringData, min<int>(size, 256));
				temp[min<int>(size, 255)] = 0;
				return temp;
			}
		}
		
		// Otherwise just "Element index"
		snprintf(temp, 256, "Element %d", top->arrayIndex);
		return temp;
	}
	else
	{
		// If there is a custom display name defined in doxygen docs ( a-la > DisplayName { Custom Display Name } ) return that
		// else mangle the member name.
		const char* custom = GetVariableDisplayName (GetTypeTree().m_Father->m_Type, GetTypeTree().m_Name);

		if (custom)
			return custom;
			
		return MangleVariableName(GetTypeTree().m_Name.c_str());
	}
}

int SerializedProperty::GetSerializedPropertyType () const
{
	return GetSerializedPropertyType(m_TypeTree);
}

int SerializedProperty::GetSerializedPropertyType (TypeTree* typeTree) const
{
	TypeTreeString& type = typeTree->m_Type;
	if (type == SerializeTraits<SInt8>::GetTypeString () || type == SerializeTraits<UInt8>::GetTypeString () ||
		type == SerializeTraits<SInt16>::GetTypeString () || type == SerializeTraits<UInt16>::GetTypeString () ||
		type == SerializeTraits<UInt32>::GetTypeString () || type == SerializeTraits<SInt32>::GetTypeString () ||
		type == SerializeTraits<UInt64>::GetTypeString () || type == SerializeTraits<SInt64>::GetTypeString () ||
		type == SerializeTraits<char>::GetTypeString ())
	{
		if (typeTree->m_Father && typeTree->m_Father->m_IsArray && &typeTree->m_Father->m_Children.front() == typeTree)
			return kArraySize;
		else if ((typeTree->m_MetaFlag & kEditorDisplaysCheckBoxMask) != 0)
			return kBoolValue;
		else if (m_SerializedObject->GetPopupMenuData(*typeTree) != NULL) ///@TODO: BAD FOR PERFORMANCE
			return kEnumValue;
		else
			return kIntValue;
	}
	else if(type == SerializeTraits<float>::GetTypeString () || type == SerializeTraits<double>::GetTypeString ())
	{
		return kFloatValue;
	}
	else if(type == SerializeTraits<bool>::GetTypeString ())
	{
		return kBoolValue;
	}
	else if (type == SerializeTraits<UnityStr>::GetTypeString ())
	{
		return kStringValue;
	}
	else if (type == "ColorRGBA")
	{
		return kColorValue;
	}
	else if (IsTypeTreePPtr(*typeTree))
	{
		return kPPtrValue;
	}
	else if (type == "BitField")
	{
		return kLayerMaskValue;
	}
	else if (type == "Vector2f")
	{
		return kVector2Value;
	}
	else if (type == "Vector3f")
	{
		return kVector3Value;
	}
	else if (type == "Rectf")
	{
		return kRectValue;
	}
	else if (type == "AnimationCurve")
	{
		return kAnimationCurveValue;
	}
	else if (type == "GradientNEW")
	{
		return kGradientValue;
	}
	else if (type == "AABB")
	{
		return kBoundsValue;
	}
	else if(type == "Quaternionf" )
	{
		return kQuaternionValue;
	}
	return kOtherValue;
}

int SerializedProperty::GetIntValue () const
{
	void* data = &(*m_Data)[m_ByteOffset];
	const string& type = GetTypeTree ().m_Type;
	
	if (type == SerializeTraits<SInt8>::GetTypeString ())
		return *reinterpret_cast<SInt8*> (data);
	else if (type == SerializeTraits<UInt8>::GetTypeString ())
		return *reinterpret_cast<UInt8*> (data);
	else if (type == SerializeTraits<SInt16>::GetTypeString ())
		return *reinterpret_cast<SInt16*> (data);
	else if (type == SerializeTraits<UInt16>::GetTypeString ())
		return *reinterpret_cast<UInt16*> (data);
	else if (type == SerializeTraits<SInt32>::GetTypeString ())
		return *reinterpret_cast<SInt32*> (data);
	else if (type == SerializeTraits<UInt32>::GetTypeString () || type == "BitField")
		return *reinterpret_cast<UInt32*> (data);
	else if (type == SerializeTraits<char>::GetTypeString ())
		return *reinterpret_cast<char*> (data);
	else
		ErrorString ("type is not a supported int value");
	return 0;
}

void SerializedProperty::ResizeArray (int newArraySize)
{
	TypeTree* arrayTypeTree = NULL;
	
	// Current property is the array itself
	if (m_TypeTree->m_IsArray)
	{
		arrayTypeTree = m_TypeTree;
	}
	else if (IsTypeTreeArrayOrArrayContainer(*m_TypeTree))
	{
		arrayTypeTree = &m_TypeTree->m_Children.back();
	}
	// Current property is the array.size field
	else if (IsTypeTreeArraySize(*m_TypeTree))
	{
		arrayTypeTree = m_TypeTree->m_Father;
	}
	else
	{
		ErrorString("Invalid property to resize array");
		return;
	}
	
	if (ResizeArrayGeneric(*arrayTypeTree, *m_Data, m_ByteOffset, newArraySize, true))
	{
		MarkPropertyModified();
		
		// Make sure we reset the minimum array sizes in the serialized object.
		string path = m_PropertyPath;
		if (IsTypeTreeArrayOrArrayContainer(*m_TypeTree))
			path += ".Array";
		else if (!m_TypeTree->m_IsArray)
			ClearPropertyNameFromBufferSerializedProperty(path);
		map<string, int> &minArraySizes = m_SerializedObject->m_MinArraySizes;
		map<string, int>::iterator entry = minArraySizes.find (path);
		if (entry != minArraySizes.end())
			minArraySizes [path] = std::min (entry->second, newArraySize);
	}
}

bool SerializedProperty::MoveArrayElement (int src, int dst)
{
	// Must be top level array element but not the size
	if (!IsTypeTreeArrayOrArrayContainer(*m_TypeTree))
		return false;

	// Select the actual array type
	if (!IsTypeTreeArray(*m_TypeTree))
		Next(true);
	Assert(IsTypeTreeArray(*m_TypeTree));

	if (m_SerializedObject->m_Objects.size() > 1)
	{
		if (m_SerializedObject->m_IsDifferent[GetPropertyPath()])
		{
			if (!DisplayDialog(
							   "Duplicating an array element will copy the complete array to all other selected objects.",
							   "Unique values in the different selected objects will be lost",
							   "Duplicate",
							   "Cancel"
							   ))
				return false;
		}
	}

	int insertIndex = dst;
	if (dst > src)
		insertIndex++;

	if (!DuplicateArrayElement (*m_TypeTree, *m_Data, m_ByteOffset, src, insertIndex))
		return false;
	
	int deleteIndex = src;
	if (dst < src)
		deleteIndex++;
	
	ErrorIf(!DeleteArrayElement (*m_TypeTree, *m_Data, m_ByteOffset, deleteIndex));
	
	MarkPropertyModified (GetPropertyPath());

	return true;
}

bool SerializedProperty::DuplicateCommand ()
{
	// Must be top level array element but not the size
	if (!IsTypeTreeArrayElement(*m_TypeTree))
		return false;

	if (m_SerializedObject->m_Objects.size() > 1)
	{
		if (m_SerializedObject->m_IsDifferent[GetParentArrayPropertyPath()])
		{
			if (!DisplayDialog(
				"Duplicating an array element will copy the complete array to all other selected objects.",
				"Unique values in the different selected objects will be lost",
				"Duplicate",
				"Cancel"
			))
				return false;
		}
	}

	if (DuplicateArrayElement (*m_TypeTree->m_Father, *m_Data, m_Stack.back().arrayByteOffset, m_Stack.back().arrayIndex, m_Stack.back().arrayIndex))
	{	
		MarkPropertyModified (GetParentArrayPropertyPath());
		
		return true;
	}
	return false;
}

int SerializedProperty::GetDepth () const
{
	int depth = -2;
	const TypeTree* child = m_TypeTree;
	while (child != NULL)
	{
		if (!child->m_IsArray)
			depth++;
		
		child = child->m_Father;
	}
	
	return max(depth, -1);
}


void SerializedProperty::Rewind ()
{
	// Get root
	while (m_TypeTree->m_Father)
		m_TypeTree = m_TypeTree->m_Father;

	m_ByteOffset = 0;

	// Clear stack
	m_Stack.clear();
	m_PropertyPath.clear();
}

bool SerializedProperty::DeleteCommand ()
{
	// Delete pptr
	if (IsTypeTreePPtr(*m_TypeTree))
	{
		SInt32* instanceIDPtr = reinterpret_cast<SInt32*> (GetDataPtr(m_ByteOffset));
		if (*instanceIDPtr != 0 || HasMultipleDifferentValues())
		{
			*instanceIDPtr = 0;
			MarkPropertyModified();
			return true;
		}
	}
	
	// Must be top level array element but not the size
	if (!IsTypeTreeArrayElement(*m_TypeTree))
		return false;

	if (m_SerializedObject->m_Objects.size() > 1)
	{
		if (m_SerializedObject->m_IsDifferent[GetParentArrayPropertyPath()])
		{
			if (!DisplayDialog(
				"Deleting an array element will copy the complete array to all other selected objects.",
				"Unique values in the different selected objects will be lost",
				"Delete",
				"Cancel"
			))
				return false;
		}
	}

	if (DeleteArrayElement (*m_TypeTree->m_Father, *m_Data, m_Stack.back().arrayByteOffset, m_Stack.back().arrayIndex))
	{
		MarkPropertyModified(GetParentArrayPropertyPath());
		Rewind();
		
		// Move Iterator to end, by convention, after a delete command, the iterator is at end position.
		while (!m_TypeTree->m_Children.empty())
			m_TypeTree = &m_TypeTree->m_Children.back();
		m_ByteOffset = -1;

		return true;
	}
	return false;
	
}

template<class T>
T ClampIntToTargetRange (int value)
{
	value = max<int>(value, std::numeric_limits<T>::min());
	value = min<int>(value, std::numeric_limits<T>::max());
	return value;
}

void SerializedProperty::SetIntValue (int value)
{
	void* data = &(*m_Data)[m_ByteOffset];
	const string& type = GetTypeTree ().m_Type;

	// If this is the array size element -> resize array
	if (IsTypeTreeArraySize(*m_TypeTree))
	{
		ResizeArray (value);
		return;
	}
	
	// Apply range
	pair<float, float> range = GetRangeFromDocumentation (GetTypeTree().m_Father->m_Type, GetTypeTree().m_Name);
	if (range.first != -numeric_limits<float>::infinity ())
		value = max<int> (RoundfToInt (range.first), value);
	if (range.second != numeric_limits<float>::infinity ())
		value = min<int> (RoundfToInt (range.second), value);
	
	if (type == SerializeTraits<SInt8>::GetTypeString ())
	{
		if (*reinterpret_cast<SInt8*> (data) != value || HasMultipleDifferentValues())
		{
			*reinterpret_cast<SInt8*> (data) = ClampIntToTargetRange<SInt8>(value);
			MarkPropertyModified();
		}
	}
	else if (type == SerializeTraits<UInt8>::GetTypeString ())
	{
		if (*reinterpret_cast<UInt8*> (data) != value || HasMultipleDifferentValues())
		{
			*reinterpret_cast<UInt8*> (data) = ClampIntToTargetRange<UInt8>(value);
			MarkPropertyModified();
		}
	}
	else if (type == SerializeTraits<SInt16>::GetTypeString ())
	{
		if (*reinterpret_cast<SInt16*> (data) != value || HasMultipleDifferentValues())
		{
			*reinterpret_cast<SInt16*> (data) = ClampIntToTargetRange<SInt16>(value);
			MarkPropertyModified();
		}
	}
	else if (type == SerializeTraits<UInt16>::GetTypeString ())
	{
		if (*reinterpret_cast<UInt16*> (data) != value || HasMultipleDifferentValues())
		{
			*reinterpret_cast<UInt16*> (data) = ClampIntToTargetRange<UInt16>(value);
			MarkPropertyModified();
		}
	}
	else if (type == SerializeTraits<SInt32>::GetTypeString ())
	{
		if (*reinterpret_cast<SInt32*> (data) != value || HasMultipleDifferentValues())
		{
			*reinterpret_cast<SInt32*> (data) = ClampIntToTargetRange<SInt32>(value);
			MarkPropertyModified();
		}
	}
	else if (type == SerializeTraits<UInt32>::GetTypeString () || type == "BitField")
	{
		if (*reinterpret_cast<UInt32*> (data) != value || HasMultipleDifferentValues())
		{
			// We can not clamp to int range in this case, because the input value might already be overflown
			// since we incorrectly use int for setting UInt32 values
			*reinterpret_cast<UInt32*> (data) = (UInt32)value;
			MarkPropertyModified();
		}
	}
	else if (type == SerializeTraits<char>::GetTypeString ())
	{
		if (*reinterpret_cast<char*> (data) != value || HasMultipleDifferentValues())
		{
			*reinterpret_cast<char*> (data) = ClampIntToTargetRange<char>(value);
			MarkPropertyModified();
		}
	}
	else
		ErrorString ("type is not a supported int value");
}

float SerializedProperty::GetFloatValue () const
{
	void* data = &(*m_Data)[m_ByteOffset];
	const string& type = GetTypeTree ().m_Type;

	if (type == SerializeTraits<float>::GetTypeString ())
		return *reinterpret_cast<float*> (data);
	else if (type == SerializeTraits<double>::GetTypeString ())
		return *reinterpret_cast<double*> (data);
	else
		ErrorString ("type is not a supported float value");
	return 0;
}

void SerializedProperty::SetFloatValue (float f)
{
	void* data = &(*m_Data)[m_ByteOffset];
	const string& type = GetTypeTree ().m_Type;

	// Apply range
	pair<float, float> range = GetRangeFromDocumentation (GetTypeTree ().m_Father->m_Type, GetTypeTree ().m_Name);
	if (range.first != -numeric_limits<float>::infinity ())
		f = max (range.first, f);
	if (range.second != numeric_limits<float>::infinity ())
		f = min (range.second, f);

	if (type == SerializeTraits<float>::GetTypeString ())
	{
		if (*reinterpret_cast<float*> (data) != f || HasMultipleDifferentValues())
		{
			*reinterpret_cast<float*> (data) = f;
			MarkPropertyModified();
		}
	}
	else if (type == SerializeTraits<double>::GetTypeString ())
	{
		if (*reinterpret_cast<double*> (data) != f || HasMultipleDifferentValues())
		{
			*reinterpret_cast<double*> (data) = f;
			MarkPropertyModified();
		}
	}
	else
		ErrorString ("type is not a supported float value");
}

string SerializedProperty::GetStringValue () const
{
	if (IsTypeTreeString(GetTypeTree ()))
	{
		int stringByteOffset = m_ByteOffset + sizeof(SInt32);
		SInt32& size = *reinterpret_cast<SInt32*> (&(*m_Data)[m_ByteOffset]);
		if(size == 0)
			return "";
		char* stringData = reinterpret_cast<char*> (&(*m_Data)[stringByteOffset]);
		string outData (stringData, stringData + size);
		return outData;
	}
	else
	{
		ErrorString ("type is not a supported string value");
		return "";
	}
}

void SerializedProperty::SetStringValue (const string& value)
{
	if (!IsTypeTreeString(GetTypeTree ()))
	{
		ErrorString ("type is not a supported string value");
		return;
	}
	
	if (GetStringValue () == value)
		return;
	
	if (::SetStringValue(GetTypeTree(), *m_Data, m_ByteOffset, value) || HasMultipleDifferentValues())
		MarkPropertyModified(GetPropertyPath());
}

bool SerializedProperty::GetBoolValue () const
{
	const string& type = GetTypeTree ().m_Type;
	void* data = &(*m_Data)[m_ByteOffset];
	if (type == SerializeTraits<bool>::GetTypeString ())
		return *reinterpret_cast<char*> (data);
	else
		return GetIntValue () != 0;
}

void SerializedProperty::SetBoolValue (bool value)
{
	const string& type = GetTypeTree ().m_Type;
	void* data = &(*m_Data)[m_ByteOffset];
	if (type == SerializeTraits<bool>::GetTypeString ())
	{
		if (*reinterpret_cast<char*> (data) != (char)value || HasMultipleDifferentValues())
		{
			*reinterpret_cast<char*> (data) = (char)value;
			MarkPropertyModified();
		}
	}
	else
		return SetIntValue (value ? 1 : 0);
}

ColorRGBAf SerializedProperty::GetColorValue () const
{
	const string& type = GetTypeTree ().m_Type;
	int byteSize = GetTypeTree ().m_ByteSize;
	void* data = &(*m_Data)[m_ByteOffset];

	if (type == "ColorRGBA")
	{
		if (byteSize == sizeof (UInt32))
		{
			ColorRGBA32 color32 = *reinterpret_cast<ColorRGBA32*> (data);
			return color32;
		}
		else if (byteSize == sizeof (float) * 4)
		{
			ColorRGBAf color = *reinterpret_cast<ColorRGBAf*> (data);
			return color;
		}
	}

	ErrorString ("type is not a supported color value");
	return ColorRGBAf(0,0,0,0);
}

void SerializedProperty::SetColorValue (const ColorRGBAf& c)
{
	TypeTreeString& type = GetTypeTree ().m_Type;
	int byteSize = GetTypeTree ().m_ByteSize;
	void* data = &(*m_Data)[m_ByteOffset];

	if (type == "ColorRGBA")
	{
		if (c.Equals(GetColorValue()) && !HasMultipleDifferentValues())
			return;

		if (byteSize == sizeof (UInt32))
		{
			*reinterpret_cast<ColorRGBA32*> (data) = c;
			
			MarkPropertyModified();
		}
		else if (byteSize == sizeof (float) * 4)
		{
			*reinterpret_cast<ColorRGBAf*> (data) = c;
			MarkPropertyModified();
		}
	}
	else
	{
		ErrorString ("type is not a supported color value");
	}
}

bool IsAnimationCurveTypeTree (TypeTree& typeTree)
{
	if (typeTree.m_Type != "AnimationCurve")
		return false;
	if (typeTree.m_Children.size () != 3)
		return false;
	if (typeTree.m_Children.front ().m_Name != "m_Curve")
		return false;
	
	return true;
}

AnimationCurve* SerializedProperty::GetAnimationCurveValueCopy ()
{
	if (!IsAnimationCurveTypeTree(GetTypeTree ()))
		return NULL;
	
	UInt8* data = &(*m_Data)[m_ByteOffset];
	SInt32 size = *reinterpret_cast<SInt32*> (data);
	data += sizeof(SInt32);
	
	SInt32 curveByteSize = size * sizeof(AnimationCurve::Keyframe) + 3 * sizeof(SInt32);
	
	if (m_ByteOffset + curveByteSize > m_Data->size())
	{
		AssertString("Invalid formatted animation curve value in serialized object");
		return NULL;
	}

	AnimationCurve* curve = new AnimationCurve ();
	
	AnimationCurve::Keyframe* firstKey = reinterpret_cast<AnimationCurve::Keyframe*> (data);
	curve->Assign(firstKey, firstKey + size);
	
	data += size * sizeof(AnimationCurve::Keyframe);
	
	curve->SetPreInfinityInternal( *reinterpret_cast<SInt32*> (data) );
	data += sizeof(SInt32);
	curve->SetPostInfinityInternal( *reinterpret_cast<SInt32*> (data) );
	
	return curve;
}

void SerializedProperty::SetAnimationCurveValue (const AnimationCurve* c)
{
	if (!IsAnimationCurveTypeTree (GetTypeTree()))
	{
		ErrorString("Not an animation curve");
		return;
	}

	if (c == NULL)
		return;
	
	// Verify computed size against serialized data size from TypeTree
	int endCurveData = m_ByteOffset;
	WalkTypeTree(GetTypeTree(), GetDataPtr(0), &endCurveData);
	if (endCurveData > m_Data->size())
	{
		AssertString("Invalid animation curve data format");
		return;
	}
	
	
	// Build newData curve array
	int curveDataSize = c->GetKeyCount() * sizeof (AnimationCurve::Keyframe);
	dynamic_array<UInt8> newData(kMemTempAlloc);
	newData.resize_initialized(curveDataSize + 3 * sizeof(SInt32));
	UInt8* newDataPtr = newData.begin();
	
	*reinterpret_cast<SInt32*> (newDataPtr) = c->GetKeyCount();
	newDataPtr += sizeof(SInt32);
	
	if (curveDataSize > 0)
	{
		memcpy(newDataPtr, &c->GetKey(0), curveDataSize); // @todo &GetKey(0) is scary. 
		newDataPtr += curveDataSize;
	}
	
	*reinterpret_cast<SInt32*> (newDataPtr) = c->GetPreInfinityInternal();
	newDataPtr += sizeof(SInt32);
	*reinterpret_cast<SInt32*> (newDataPtr) = c->GetPostInfinityInternal();

	
	// Compare against old curve data
	UInt32 oldCompleteSize = CalculateByteSize(GetTypeTree (), &(*m_Data)[m_ByteOffset]);

	if (oldCompleteSize != endCurveData - m_ByteOffset)
	{
		AssertString("Invalid animation curve data format");
		return;
	}
	
	if (oldCompleteSize == newData.size() && memcmp(GetDataPtr(m_ByteOffset), newData.begin(), newData.size()) == 0 && !HasMultipleDifferentValues())
	   return;

	// Replace data			   
	m_Data->erase(m_Data->begin() + m_ByteOffset, m_Data->begin() + m_ByteOffset + oldCompleteSize);
	m_Data->insert(m_Data->begin() + m_ByteOffset, newData.begin(), newData.end());

	MarkPropertyModified();
	MarkPropertyModified(GetPropertyPath());
}


GradientNEW* SerializedProperty::GetGradientValueCopy ()
{
	TypeTreeString& type = GetTypeTree ().m_Type;

	if (type != "GradientNEW")
		return NULL;

	GradientNEW* gradient = new GradientNEW ();
	
	UInt8* data = &(*m_Data)[m_ByteOffset];
	
	ColorRGBA32& firstKey = gradient->GetKey(0);
	size_t tempSize = sizeof(ColorRGBA32) * kGradientMaxNumKeys;
	memcpy(&firstKey, data, tempSize); 
	data += tempSize;
	
	UInt16& firstColorTime = gradient->GetColorTime(0);
	tempSize = sizeof(UInt16) * kGradientMaxNumKeys;
	memcpy(&firstColorTime, data, tempSize); 
	data += tempSize;

	UInt16& firstAlphaTime = gradient->GetAlphaTime(0);
	tempSize = sizeof(UInt16) * kGradientMaxNumKeys;
	memcpy(&firstAlphaTime, data, tempSize); 
	data += tempSize;
	
	UInt8 numColorKeys = *data;
	data += sizeof(UInt8);
	UInt8 numAlphaKeys = *data;

	gradient->SetNumColorKeys(numColorKeys);
	gradient->SetNumAlphaKeys(numAlphaKeys);

	return gradient;
}

void SerializedProperty::SetGradientValue (const GradientNEW* g)
{
	TypeTreeString& type = GetTypeTree ().m_Type;
	if (type == "GradientNEW")
	{
		UInt8* data = &(*m_Data)[m_ByteOffset];

		const ColorRGBA32& firstKey = g->GetKey(0);
		size_t tempSize = sizeof(ColorRGBA32) * kGradientMaxNumKeys;
		memcpy(data, &firstKey, tempSize); 
		data += tempSize;

		const UInt16& firstColorTime = g->GetColorTime(0);
		tempSize = sizeof(UInt16) * kGradientMaxNumKeys;
		memcpy(data, &firstColorTime, tempSize); 
		data += tempSize;

		const UInt16& firstAlphaTime = g->GetAlphaTime(0);
		tempSize = sizeof(UInt16) * kGradientMaxNumKeys;
		memcpy(data, &firstAlphaTime, tempSize); 
		data += tempSize;

		UInt16 numColorKeys = (UInt16)g->GetNumColorKeys();
		UInt16 numAlphaKeys = (UInt16)g->GetNumAlphaKeys();

		*data = numColorKeys;
		data += sizeof(UInt8);
		*data = numAlphaKeys;

		MarkPropertyModified();
	}
	else
	{
		ErrorString ("type is not a supported gradient value");
	}
}


void SerializedProperty::MarkPropertyModified (std::string path)
{
	if (m_SerializedObject)
	{
		int serializedObjectVersionMatches = m_SerializedObject->m_SerializedObjectVersion == m_SerializedObjectVersion;
		
		if (path.empty())
			path = GetPropertyPath();
			
		m_SerializedObject->MarkPropertyModified(path);
		
		// Serialized Object version used to match, so it should continue to match
		if (serializedObjectVersionMatches)
			m_SerializedObjectVersion = m_SerializedObject->m_SerializedObjectVersion;
	}
}

UInt32 SerializedProperty::HasMultipleDifferentValues () const
{
	if (m_SerializedObject->m_Objects.size() == 1)
		return false;
	const std::string& path = GetPropertyPath();
	std::map<std::string, UInt32>::iterator isDifferent = m_SerializedObject->m_IsDifferent.find(path);
	if (isDifferent != m_SerializedObject->m_IsDifferent.end())
		return isDifferent->second;

	return m_SerializedObject->m_IsDifferent [path] = 0xffffffff;
}

Object* SerializedObject::GetTargetObject ()
{
	return GetFirstObject(); 
}

bool SerializedProperty::IsVisible (const TypeTree& typeTree) const
{
	// Property is not marked hidden
	if ((typeTree.m_MetaFlag & kHideInEditorMask) == 0)
		return true;
	
	if (m_SerializedObject->m_InspectorMode == kAllPropertiesInspector)
	{
		// Property is marked hidden and when showing all properties some properties should still not show their children (eg. Color and PPtr for)
		bool isSpecialVisualizedProperty = false;
		if (typeTree.m_Father)
			isSpecialVisualizedProperty = typeTree.m_Father->m_Type == "ColorRGBA" || IsTypeTreePPtr(*typeTree.m_Father) || typeTree.m_Father->m_Type == "string";

		return !isSpecialVisualizedProperty;
	}
	else
	{
		return false;
	}
}

bool SerializedProperty::GetEditable () const
{
	return m_EditableObject && (GetTypeTree().m_MetaFlag & kNotEditableMask) == 0;
}

bool SerializedProperty::IsAnimated () const
{
	return GetAnimationModeSnapshot().IsPropertyAnimated (m_Object, GetPropertyPath().c_str());
}

bool SerializedProperty::GetIsInstantiatedPrefab ()
{
	return m_IsInstantiatedPrefab;
}

bool SerializedProperty::HasVisibleChildren () const
{
	for (TypeTree::iterator i=m_TypeTree->m_Children.begin();i != m_TypeTree->m_Children.end();i++)
	{
		if (IsVisible(*i))
			return true;
	}
	return false;
}

// @TODO: Use a copy so the property passed as argument keeps pointing to the same property.
int SerializedProperty::CountRemaining ()
{
	int count = 0;
	bool expanded = true;
	while (NextVisible(expanded))
	{
		count++;
		expanded = HasVisibleChildren() && GetIsExpanded();
	}
	
	return count;
}

// @TODO: Use a copy so the property passed as argument keeps pointing to the same property.
// @TODO: Optimize to not call GetPropertyPath so much since it's probably slow.
int SerializedProperty::CountInProperty ()
{
	SerializedProperty endProperty = *this;
	endProperty.NextVisible (false);
	string endPropertyPath = endProperty.GetPropertyPath ();
	
	int count = 1;
	bool expanded = HasVisibleChildren() && GetIsExpanded();
	while (NextVisible (expanded) && GetPropertyPath () != endPropertyPath)
	{
		count++;
		expanded = HasVisibleChildren() && GetIsExpanded();
	}
	
	return count;
}

void SerializedProperty::SyncSerializedObjectVersion ()
{
	if (m_SerializedObject->m_SerializedObjectVersion == m_SerializedObjectVersion)
		return;
	
	if (!m_PropertyPath.empty())
	{
		TypeTree* hint = m_TypeTree;
		string path = m_PropertyPath;
		m_SerializedObject->GetIterator(*this);
		if (!FindProperty(path, hint))
		{
			ErrorString("SerializedProperty " + path + " has disappeared!");
		}	
	}
	else
	{
		// Property path is empty. We assume that we are at the end element
		// In this case we need to reset the property and reiterate to the end
		// this is because it may be an array and elements have been added or removed
		m_SerializedObject->GetIterator(*this);
		bool first = true;
		while (Next(first))
			first = false;
	}
}


bool SerializedProperty::FindProperty (const std::string& propertyPath, TypeTree* typeTreeHint)
{
	bool expanded = true;
	while (Next(expanded))
	{
		Assert(m_ByteOffset >= 0);
		
		// First check the hint. This is much faster than doing string comparison.
		if (typeTreeHint == NULL || typeTreeHint == m_TypeTree)
		{
			// Ensure that the property path is the same
			const string& currentPath = GetPropertyPath();
			if (propertyPath == currentPath)
				return true;
		}
		expanded = HasChildren();
		// Do not descend into children if we aren't going to find the
		// property there anyway
		if (expanded && !BeginsWith(propertyPath, GetPropertyPath()))
			expanded = false;
	}
	
	Assert(m_PropertyPath.empty());
	
	return false;
}

bool SerializedProperty::FindRelativeProperty (const std::string& relativePropertyPath)
{
	return FindProperty(GetPropertyPath() + "." + relativePropertyPath);
}

bool SerializedProperty::GetIsExpanded ()
{
	// Don't assert if property has no children. isExpanded property is sometimes used for other things in custom editors.
	return m_SerializedObject->GetIsExpanded(GetPropertyPath());
}

void SerializedProperty::SetIsExpanded (bool value)
{
	// Don't assert if property has no children. isExpanded property is sometimes used for other things in custom editors.
	m_SerializedObject->SetIsExpanded(GetPropertyPath(), value);
}

std::string SerializedProperty::GetPPtrStringValue () const
{
	const TypeTree& typeTree = GetTypeTree();
	if (IsTypeTreePPtr(typeTree))
	{
		//AssertIf (GetTypeTree ().m_ByteSize != 2 * sizeof (SInt32));
		void* data = &(*m_Data)[m_ByteOffset];
		
		string className = ExtractMonoPPtrClassName(GetTypeTree());
		if (className.empty())
			className = ExtractPPtrClassName(typeTree);
		
		SInt32 instanceID = *reinterpret_cast<SInt32*> (data);
		return GetPropertyEditorPPtrName (instanceID, className);
	}
	else
	{
		ErrorString("type is not a supported pptr value");
		return "";	
	}
}

PPtr<Object> SerializedProperty::GetPPtrValue () const
{
	if (IsTypeTreePPtr(GetTypeTree()))
	{
		//AssertIf (GetTypeTree ().m_ByteSize != 2 * sizeof (SInt32));
		void* data = &(*m_Data)[m_ByteOffset];
		
		SInt32 instanceID = *reinterpret_cast<SInt32*> (data);
		return PPtr<Object> (instanceID);
	}
	else
	{
		ErrorString("type is not a supported pptr value");
		return 0;	
	}
}

string SerializedProperty::GetPPtrClassName ()
{
	string monoClassName = ExtractMonoPPtrClassName(*m_TypeTree);
	if (!monoClassName.empty())
		return monoClassName;
	
	string className = ExtractPPtrClassName(*m_TypeTree);
	if (!className.empty())
		return className;
	
	return "";
}


SInt32 SerializedProperty::RemapInstanceID (SInt32 instanceID)
{
	Object* remapObject = PPtr<Object> (instanceID);
	Object* assignOnObject = dynamic_pptr_cast<Object*> (m_Object);
	if (remapObject == NULL)
		return 0;
	if (assignOnObject == NULL)
		return 0;

	// We do not allow dragging scene objects on prefabs etc.	
	if (assignOnObject->IsPersistent() && !remapObject->IsPersistent())
		return 0;

	// If type is an array or vector, see if we can add the object to the array.
	TypeTree* typeTree = m_TypeTree;
	if (typeTree->m_Children.size() == 1 && typeTree->m_Children.back().m_IsArray)
		typeTree = &typeTree->m_Children.back().m_Children.back();
		
	string monoClassName = ExtractMonoPPtrClassName(*typeTree);

	// Calculate classID
	// typename is eg: PPtr<GameObject>, we want to extract "GameObject"
	string requiredClassString = ExtractPPtrClassName(*typeTree);
	if (monoClassName.empty() && requiredClassString.empty())
		return 0;
	
	int requiredClassID = -1;
	if (!requiredClassString.empty())
	{
		requiredClassID = Object::StringToClassID (requiredClassString);
		if (requiredClassID == -1)
			return 0;
	}
	
	if (monoClassName.empty ())
	{
		if (remapObject->IsDerivedFrom (requiredClassID))
			return remapObject->GetInstanceID ();
	}
	else
	{
		if (assignOnObject->CanAssignMonoVariable (monoClassName.c_str(), remapObject))
			return remapObject->GetInstanceID ();
	}		

	if (dynamic_pptr_cast<GameObject*> (remapObject))
	{
		GameObject& go = *static_cast<GameObject*> (remapObject);
		for (int i=0;i<go.GetComponentCount();i++)
		{
			Unity::Component& component = go.GetComponentAtIndex(i);
			if (monoClassName.empty())
			{
				if (component.IsDerivedFrom(requiredClassID))
				{
					return component.GetInstanceID();
				}
			}
			else if (assignOnObject->CanAssignMonoVariable (monoClassName.c_str(), &component))
			{
				return component.GetInstanceID();
			}
		}
	}
	
	return 0;
}

bool SerializedProperty::ValidatePPtrValue (PPtr<Object> value)
{
	return RemapInstanceID (value.GetInstanceID()) != 0;
}

void SerializedProperty::SetPPtrValue (PPtr<Object> value)
{
	if (IsTypeTreePPtr(GetTypeTree()))
	{
		AssertIf (GetTypeTree ().m_ByteSize != 2 * sizeof (SInt32));
		void* data = &(*m_Data)[m_ByteOffset];
		
		SInt32 remapped = RemapInstanceID (value.GetInstanceID());
		if (*reinterpret_cast<SInt32*> (data) != remapped || HasMultipleDifferentValues())
		{
			*reinterpret_cast<SInt32*> (data) = remapped;
			MarkPropertyModified();
		}
	}
	else
	{
		ErrorString("type is not a supported pptr value");
	}
}

void SerializedProperty::AppendFoldoutPPtrValue (PPtr<Object> value)
{
	// Is this really an array property?
	AssertIf (m_TypeTree->m_Children.size() != 1 || !m_TypeTree->m_Children.back().m_IsArray);
	
	// Get actual array
	SerializedProperty arrayProperty = *this;
	arrayProperty.Next (true);
	
	int arraySize = ExtractArraySize(arrayProperty.GetDataPtr(m_ByteOffset));

	bool serializedObjectVersionMatches = m_SerializedObject->m_SerializedObjectVersion == m_SerializedObjectVersion;
	arrayProperty.ResizeArray(++arraySize);
	
	// Match serialized object version
	if (serializedObjectVersionMatches)
		m_SerializedObjectVersion = m_SerializedObject->m_SerializedObjectVersion;
	
	TypeTree& elementType = arrayProperty.m_TypeTree->m_Children.back();

	AssertIf (!IsTypeTreePPtr(elementType));

	// Find the last array element byte start (Walk typetree arraySize - 1 times)
	int lastArrayElementByteStart = arrayProperty.m_ByteOffset + sizeof(SInt32);
	for (int i=0;i<arraySize - 1;i++)
		WalkTypeTree(elementType, GetDataPtr(0), &lastArrayElementByteStart);
	
	void* data = GetDataPtr(lastArrayElementByteStart);

	SInt32 remapped = RemapInstanceID (value.GetInstanceID());

	*reinterpret_cast<SInt32*> (data) = remapped;
}

std::string SerializedProperty::GetEnumStringValue () const
{
	const map<string, int>* enumMapping = m_SerializedObject->GetPopupMenuData (*m_TypeTree);
	if (enumMapping != NULL)
	{
		const int value = GetIntValue ();
		for (map<string, int>::const_iterator found = enumMapping->begin ();
		     enumMapping->end () != found;
		     ++found)
			 if (value == found->second)
				 return found->first;
		return "Undefined";		
	}
	else
	{
		ErrorString("type is not a enum value");
		return "";
	}
}


std::string SerializedProperty::GetTooltip () const
{
	string tooltip = GetVariableDocumentation (GetTypeTree().m_Father->m_Type, GetTypeTree().m_Name);
	return tooltip;
}

static vector<int> GetSortedIndices (const map<string, int> *enumMapping)
{
	vector<int> indices;
	indices.reserve (enumMapping->size ());

	for (map<string, int>::const_iterator i = enumMapping->begin ();
	     enumMapping->end () != i;
	     ++i)
		 indices.push_back (i->second);
	sort (indices.begin (), indices.end ());
	return indices;
}

std::vector<std::string> SerializedProperty::GetEnumNames () const
{
	const map<string, int>* enumMapping = m_SerializedObject->GetPopupMenuData (*m_TypeTree);
	if (enumMapping != NULL)
	{
		vector<string> values, originalValues;
		vector<int> keys, originalKeys;
		vector<bool> used;
		originalValues.reserve (enumMapping->size ());
		values.reserve (enumMapping->size ());
		originalKeys.reserve (enumMapping->size ());
		used.reserve (enumMapping->size ());
		
		for (map<string,int>::const_iterator i=enumMapping->begin();i != enumMapping->end();i++)
		{
			originalValues.push_back (i->first);
			originalKeys.push_back (i->second);
			used.push_back (false);
		}

		keys = GetSortedIndices (enumMapping);
		int index;

		for (vector<int>::const_iterator i = keys.begin (); keys.end () != i; ++i)
		{
			// No vector::find :-(
			for (index = 0;
			     originalKeys.size () != index && (used[index] || originalKeys[index] != *i);
			     ++index)
			{ }
			AssertMsg (originalKeys.size () > index, "Error sorting enum keys");
			values.push_back (originalValues[index]);
			used[index] = true;
		}
		
		return values;
	}
	else
	{
		ErrorString("type is not a enum value");
		return vector<string> ();
	}
}

void SerializedProperty::SetEnumStringValue (string value)
{
	const map<string, int>* enumMapping = m_SerializedObject->GetPopupMenuData (*m_TypeTree);
	if (enumMapping != NULL)
	{
		map<string,int>::const_iterator i = enumMapping->find (value);
		if (enumMapping->end () != i)
			SetIntValue (i->second);
	}
	else
	{
		ErrorString("type is not a enum value");
	}
}

int SerializedProperty::GetEnumValueIndex () const
{
	const map<string, int>* enumMapping = m_SerializedObject->GetPopupMenuData (*m_TypeTree);
	if (enumMapping != NULL) {
		vector<int> indices = GetSortedIndices (enumMapping);
		int value = GetIntValue ();

		for (vector<int>::iterator cursor = indices.begin ();
			 indices.end () != cursor;
			 ++cursor)
			 if (*cursor == value) {
				 int index = distance (indices.begin (), cursor);
				 return index;
			 }
	}
	else
		ErrorString("type is not a enum value");

	return -1;
}

void SerializedProperty::SetEnumValueIndex (int index)
{
	const map<string, int>* enumMapping = m_SerializedObject->GetPopupMenuData (*m_TypeTree);
	if (enumMapping != NULL)
	{
		vector<int> indices = GetSortedIndices (enumMapping);
		vector<int>::const_iterator iter = indices.begin ();
		if (index >= 0 && index < enumMapping->size())
		{
			advance(iter, index);
			SetIntValue(*iter);
		}
		else
		{
			ErrorString("enum index is out of range");
		}
	}
	else
	{
		ErrorString("type is not a enum value");
	}
}

string SerializedProperty::GetParentArrayPropertyPath () const
{
	string path = GetPropertyPath();
	size_t arrayIndexPos = path.rfind(".Array.data[");
	Assert (arrayIndexPos != string::npos);
	string arrayPath = path.substr(0, arrayIndexPos + strlen(".Array"));
	return arrayPath;
}

const std::string& SerializedProperty::GetPropertyPath () const
{
	return m_PropertyPath;
}

void SerializedProperty::GetArrayIndexLessPropertyPath (std::string& propertyPath) const
{
	propertyPath.reserve(m_PropertyPath.size());

	bool isInArrayIndex = false;
	for (int i=0;i<m_PropertyPath.size();i++)
	{
		if (m_PropertyPath[i]  == ']')
		{
			Assert (isInArrayIndex);
			isInArrayIndex = false;
		}
		if (!isInArrayIndex)
			propertyPath.push_back (m_PropertyPath[i]);
		if (m_PropertyPath[i]  == '[')
		{
			Assert (!isInArrayIndex);
			isInArrayIndex = true;
		}
	}
}

bool operator == (SerializedProperty& lhs, SerializedProperty& rhs)
{
	return lhs.m_ByteOffset == rhs.m_ByteOffset && lhs.m_TypeTree == rhs.m_TypeTree;
}

std::string SerializedProperty::GetLayerMaskStringValue () const
{
	if (m_TypeTree->m_Type == "BitField")
	{
		void* data = &(*m_Data)[m_ByteOffset];
		UInt32 bitmask = *reinterpret_cast<UInt32*> (data);
		if (bitmask == 0)
			return "Nothing";
		else if (bitmask == 0xFFFFFFFF)
			return "Everything";
		// Set title by concatenating all layer names that are set
		else
		{
			int highest = HighestBit (bitmask);
			string output;

			if (BitsInMask (bitmask) <= 3)
			{
				for (int i=LowestBit (bitmask);i<=highest;i++)
				{
					const string& name = LayerToString (i);
					
					if ((1 << i) & bitmask)
					{
						// Append undefined mask
						if (name.empty ())
							output += Format("Unnamed %d", i);
						// Append layer mask
						else
							output += name;

						// Append comma
						if (i != highest)
							output += ", ";
					}
				}
				return output;
			}
			else
				return "Mixed ... ";
		}
	}
	else
	{
		ErrorString("type is not a LayerMask value");
		return "";
	}
}

bool SerializedProperty::GetPrefabOverride () const
{
	if (m_ComponentAddedToPrefabInstance)
		return true;

	return HasPrefabOverride(m_SerializedObject->m_PrefabModifications, m_Object, GetPropertyPath());
}

void SerializedProperty::SetPrefabOverride (bool override)
{
	PPtr<Object> prefabParent = GetPrefabParentObject(m_Object);
	if (!prefabParent.IsValid())
		return;
	
	if (override)
	{
		AssertString("Set prefab override to true is not supported");
	}
	else
	{
		bool didRemove = RemovePropertyModification(m_SerializedObject->m_PrefabModifications, m_Object, GetPropertyPath ());
		m_SerializedObject->m_DidModifyPrefabModifications |= didRemove;
	}
}

enum { kEnabledBit = 1 << 30, kSetAllBits = 32, kSetNothing = 33 };

static void GetBitmaskTitles (UInt32 bitmask, vector<pair<string, UInt32> >* titles)
{
	titles->reserve (34);
	if (bitmask == 0)
		titles->push_back (make_pair ("Nothing", (UInt32)(kSetNothing | kEnabledBit)));
	else
		titles->push_back (make_pair ("Nothing", (UInt32)kSetNothing));
	
	if (bitmask == 0xFFFFFFFF)
		titles->push_back (make_pair ("Everything", (UInt32)(kSetAllBits | kEnabledBit)));
	else
		titles->push_back (make_pair ("Everything", (UInt32)kSetAllBits));
		
	for (UInt32 i=0;i<32;i++)
	{
		const string& name = LayerToString (i);
		bool enabled = bitmask & (1 << i);
		if (!name.empty ())
		{
			UInt32 enabledBit = enabled ? kEnabledBit : 0;
			titles->push_back (make_pair (name, i | enabledBit));
		}
	}
}


vector<std::string> SerializedProperty::GetLayerMaskNames () const
{
	if (m_TypeTree->m_Type == "BitField")
	{
		vector<std::string> names;
		void* data = &(*m_Data)[m_ByteOffset];
		UInt32 bitmask = *reinterpret_cast<UInt32*> (data);
		
		vector<pair<string, UInt32> > temp;
		GetBitmaskTitles (bitmask, &temp);

		for (int i=0;i<temp.size();i++)
		{
			names.push_back(temp[i].first);
		}
		return names;
	}
	else
	{
		ErrorString("type is not a LayerMask value");
		return vector<string> ();
	}
}

void SerializedProperty::ToggleLayerMaskAtIndex (int index)
{
	if (m_TypeTree->m_Type == "BitField")
	{
		vector<int> names;
		void* data = &(*m_Data)[m_ByteOffset];
		UInt32 bitmask = *reinterpret_cast<UInt32*> (data);
		if (HasMultipleDifferentValues ())
			bitmask = 0;
		
		vector<pair<string, UInt32> > titles;
		GetBitmaskTitles (bitmask, &titles);
		
		if (index < 0 || index >= titles.size ())
			return;
		
		UInt32 newBitMaskIndex = titles[index].second;
		newBitMaskIndex &= (~kEnabledBit);
		if (newBitMaskIndex == kSetNothing)
			bitmask = 0;
		else if (newBitMaskIndex == kSetAllBits)
			bitmask = 0xFFFFFFFF;
		else
			bitmask = ToggleBit (bitmask, newBitMaskIndex);
			
		if (bitmask != *reinterpret_cast<UInt32*> (data) || HasMultipleDifferentValues())
		{
			*reinterpret_cast<UInt32*> (data) = bitmask;
			MarkPropertyModified();
		}
	}
	else
	{
		ErrorString("type is not a LayerMask value");
	}
}

void SerializedProperty::SetBitAtIndexForAllTargetsImmediate (int index, bool value)
{
	if (m_TypeTree->m_ByteSize != sizeof(UInt32))
	{
		ErrorString ("property must be 32 bits for SetBitAtIndexForAllTargetsImmediate.");
		return;
	}
	
	if (!m_SerializedObject->ValidateObjectReferences())
		return;
		
	const string& path = GetPropertyPath();
	std::vector<PPtr<Object> > objects = m_SerializedObject->GetTargetObjects();

	UInt32 data = GetIntValue();
	if (value)
		data |= 1 << index;
	else
		data &= ~(1 << index);
	SetIntValue(data);
	
	int serializeFlags = m_SerializedObject->m_SerializeFlags;
	ApplyToObject(objects[0], *m_Data, *m_TypeTree, serializeFlags);

	for (int i=1; i < objects.size(); i++)
	{
		SerializedProperty dstProperty;
		dynamic_array<UInt8> dstData (kMemTempAlloc);
		m_SerializedObject->PrepareMultiEditBufferAndIterator (*objects[i], dstProperty, dstData);
		
		if (dstProperty.FindProperty(path))
		{
			UInt32 data = dstProperty.GetIntValue();
			if (value)
				data |= 1 << index;
			else
				data &= ~(1 << index);
			dstProperty.SetIntValue(data);
			
			ApplyToObject(objects[i], dstData, *m_TypeTree, serializeFlags);
		}
	}
	
	m_SerializedObject->SetIsDifferentCacheDirty();
	m_SerializedObject->Update();
}

vector<int> SerializedProperty::GetLayerMaskSelectedIndex () const
{
	if (m_TypeTree->m_Type == "BitField")
	{
		vector<int> names;
		void* data = &(*m_Data)[m_ByteOffset];
		UInt32 bitmask = *reinterpret_cast<UInt32*> (data);
		
		vector<pair<string, UInt32> > temp;
		GetBitmaskTitles (bitmask, &temp);

		for (int i=0;i<temp.size();i++)
		{
			if (temp[i].second & kEnabledBit)
				names.push_back(i);
		}

		return names;
	}
	else
	{
		ErrorString("type is not a LayerMask value");
		return vector<int> ();
	}
}

void SerializedProperty::ApplySerializedData (dynamic_array<UInt8> const& data)
{
	UInt32 oldCompleteSize = CalculateByteSize(GetTypeTree (), &(*m_Data)[m_ByteOffset]);	

	// Replace data			   
	m_Data->erase(m_Data->begin() + m_ByteOffset, m_Data->begin() + m_ByteOffset + oldCompleteSize);
	m_Data->insert(m_Data->begin() + m_ByteOffset, data.begin(), data.end());

	MarkPropertyModified(GetPropertyPath());
}

void SerializedObject::CopyFromSerializedProperty (const SerializedProperty& srcProperty)
{
	SerializedProperty dstProperty;
	GetIterator(dstProperty);
	if (!dstProperty.FindProperty(srcProperty.GetPropertyPath()))
	{
		ErrorString("Destination property could not be found");
		return;
	}
	
	int srcStart = srcProperty.m_ByteOffset;
	int srcEnd = srcStart;
	WalkTypeTree(srcProperty.GetTypeTreeUnchecked(), srcProperty.GetDataPtr(0), &srcEnd);
	
	int dstStart = dstProperty.m_ByteOffset;
	int dstEnd = dstStart;
	WalkTypeTree(dstProperty.GetTypeTreeUnchecked(), dstProperty.GetDataPtr(0), &dstEnd);
	
	m_Data.erase(m_Data.begin() + dstStart, m_Data.begin() + dstEnd);
	m_Data.insert(m_Data.begin() + dstStart, srcProperty.GetDataPtr(srcStart), srcProperty.GetDataPtrNoCheck(srcEnd));
	
	MarkPropertyModified(dstProperty.GetPropertyPath());
}

void SerializedObject::PrepareUndoOfPropertyChanges()
{
	std::vector<Object*> undoObjects;
	for (int i=0; i < m_Objects.size(); i++)
		undoObjects.push_back(m_Objects[i]);

	// Look for any modifications of m_Script
	// The reason being that when m_Script changes, the type tree of the object changes and PropertyDiffUndo cannot handle changes to the type tree
	// hence if m_Script is found we do a full object recording
	if (m_Modified.count("m_Script"))
		RegisterUndo (m_Objects[0], &undoObjects[0], undoObjects.size(), "Inspector");
	else
		RecordUndoDiff (&undoObjects[0], undoObjects.size(), "Inspector");
}
