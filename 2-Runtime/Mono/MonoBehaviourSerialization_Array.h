//This is being included by a headerfile. A bit akwardly done, but at the time it seemed like the most sane approach to be able to move serialization of arrays into its own files, to declutter the rest of monobehaviour serializationcode.

#include "Runtime/Scripting/Scripting.h"

#if ENABLE_MONO && !ENABLE_SERIALIZATION_BY_CODEGENERATION

///@TODO: investigate if we can't get rid of the duplication of code between array transfers and non-array transfers.

namespace
{
    template<typename ElementType>
    struct TransferArrayHelper
    {
        MonoArray* array;
        MonoObject* instance;
        MonoClass* klass;
        MonoClassField* field;
        int length;
        int elementSize;
        unsigned* forcedSize;
        
        vector<ElementType> vec;        
        
        TransferArrayHelper (MonoObject* instance, MonoClassField* field, MonoClass* klass, unsigned* forcedSize)
            : array (0),
              length (0),
              elementSize (0),
              forcedSize (forcedSize),
              instance (instance),
              field (field),
              klass (klass)
        {
        }
        
        void SetupWriting ()
        {
            mono_field_get_value (instance, field, &array);
            if (array)
            {
                length = forcedSize != NULL ? *forcedSize : mono_array_length(array);
                AssertIf(forcedSize && *forcedSize > mono_array_length(array));
                elementSize = mono_class_array_element_size (klass);
            }
            vec.resize (length);
        }
        
        void SetupReading ()
        {
            if (array == NULL || length != vec.size())
                array = mono_array_new (mono_domain_get (), klass, vec.size ());
            elementSize = mono_class_array_element_size (klass);
        }
        
        void FinishReading ()
        {
            mono_field_set_value (instance, field, array);
            if (forcedSize != NULL)
                *forcedSize = vec.size();
        }
        
        template<typename T>
        T& Get (int index)
        {
            return Scripting::GetScriptingArrayElement<T> (array, index);
        }
    
        template<typename T>
        void Put (int index, const T& value)
        {
            Scripting::SetScriptingArrayElement (array, index, value);
        }
    };
}


/// TODO: Optimize away where typestring is not required
template<class TransferFunction> inline 
void TransferArrayPPtr (MonoObject* instance, MonoClassField* field, const char* name, MonoClass* klass, int classID, TransferFunction& transfer, unsigned* forcedSize, TransferMetaFlags metaFlags)
{
    TransferArrayHelper<MonoPPtr> helper (instance, field, klass, forcedSize);
	
	// Potential buffer overflow
	char buffer[128];
	MonoPPtr proxy;
	proxy.m_Buffer = buffer;
	proxy.m_Class = klass;
	
	if (transfer.IsWritingPPtr())
	{
        helper.SetupWriting ();
		
		for (int i=0;i<helper.length;i++)
		{
			MonoObject* mono = helper.Get<MonoObject*> (i);
			if (mono)
				helper.vec[i].SetInstanceID(Scripting::GetInstanceIDFromScriptingWrapper(mono));
		}
	}	
	
	TransferWithProxyElement (transfer, helper.vec, proxy, name, metaFlags);
	
	if (transfer.DidReadLastPPtrProperty ())
	{
        helper.SetupReading ();
		
        const int count = helper.vec.size();
		for (int i=0;i<count;i++)
		{
            helper.Put (i, TransferPPtrToMonoObject(helper.vec[i].GetInstanceID(), klass, classID, field, instance, transfer.GetFlags() & kThreadedSerialization));
		}
		
        helper.FinishReading ();
	}
}



template<class TransferFunction> inline 
void TransferArrayString (MonoObject* instance, MonoClassField* field, const char* name, MonoClass* klass, TransferFunction& transfer, unsigned* forcedSize, TransferMetaFlags metaFlags)
{
    TransferArrayHelper<UnityStr> helper (instance, field, klass, forcedSize);
	
	if (transfer.IsWriting())
	{
        helper.SetupWriting ();
		
		for (int i=0;i<helper.length;i++)
		{
			char *p = mono_string_to_utf8 (helper.Get<MonoString*> (i));
			if (p)
				helper.vec[i] = p;
			else
				helper.vec[i].clear ();
			g_free (p);
		}
	}	
	
	transfer.Transfer (helper.vec, name, metaFlags);
	
	if (transfer.DidReadLastProperty ())
	{
        helper.SetupReading ();
    
        const int count = helper.vec.size();
		for (int i=0;i<count;i++)
		{
            helper.Put (i, MonoStringNew (helper.vec[i]));
		}
        
        helper.FinishReading ();
	}
}


/// @TODO: THIS CAN BE OPTIMIZED FOR BOTH SPACE AND SPEED. DONT CONVERT TO STL vectors.

template<class T, class TransferFunction> inline 
void TransferArrayBuiltins (MonoObject* instance, MonoClassField* field, const char* name, MonoClass* klass, TransferFunction& transfer, unsigned* forcedSize, TransferMetaFlags metaFlags)
{
    TransferArrayHelper<T> helper (instance, field, klass, forcedSize);
    
	if (transfer.IsWriting())
	{
        helper.SetupWriting ();
		
		for (int i=0;i<helper.length;i++)
		{
			helper.vec[i] = helper.template Get<T> (i);
		}
	}	
	
	transfer.Transfer (helper.vec, name, metaFlags);
	
	if (transfer.IsReading())
	{
        helper.SetupReading ();
		
        const int count = helper.vec.size();
		for (int i=0;i<count;i++)
		{
            helper.Put (i, helper.vec[i]);
		}
        
        helper.FinishReading ();
	}
}

template<typename T, class TransferFunction> inline 
void TransferArrayOfT (MonoObject* instance, MonoClassField* field, const char* name, MonoClass* klass, TransferFunction& transfer, unsigned* forcedSize, TransferMetaFlags metaFlags)
{
    TransferArrayHelper<T> helper (instance, field, klass, forcedSize);
    
    if (transfer.IsWriting() || transfer.IsWritingPPtr())
    {
        helper.SetupWriting ();
        
        for (int i=0;i<helper.length;i++)
        {
            MonoObject* element = helper.template Get<MonoObject*> (i);
            if (element != NULL)
                helper.vec[i] = *ExtractMonoObjectData<T*> (element);
        }
    }	
    
    transfer.Transfer (helper.vec, name, metaFlags);
    
    if (transfer.DidReadLastProperty () || transfer.DidReadLastPPtrProperty ())
    {
        helper.SetupReading ();
        
        const int count = helper.vec.size();
        for (int i=0;i<count;i++)
        {
            MonoObject*& element = helper.template Get<MonoObject*> (i);
            
            // Make sure the element is allocated
            if (element == NULL)
            {
                element = ScriptingInstantiateObject (klass);
                mono_runtime_object_init_log_exception (element);
            }
            
            // Replace the contents of the element
            T* monoElementRepresentation = ExtractMonoObjectData<T*> (element);
            *monoElementRepresentation = helper.vec[i];
        }
        
        helper.FinishReading ();
    }
}




struct TransferArrayScriptInstance
{
	MonoArray* array;
	MonoClass* elementClass;
	const CommonScriptingClasses* commonClasses;
	MonoClassField* field;
	MonoObject* instanceObject;
	unsigned* forcedSize;
	int depthCounter;
	
	TransferArrayScriptInstance()
	{
		depthCounter = -9999;
	}
	
	typedef TransferScriptInstance value_type; 
	
	struct iterator
	{
		MonoArray*               array;
		int                              index;
		TransferScriptInstance transfer_instance;
		
		friend bool operator  != (const iterator& lhs, const iterator& rhs) { return lhs.index != rhs.index; }
		void operator++() { index++; }
		
		TransferScriptInstance& operator * ()
		{
			MonoObject* instance = GetMonoArrayElement<MonoObject*> (array, index);
			
			if (instance == NULL)
			{
				instance = ScriptingInstantiateObject (transfer_instance.klass);
				mono_runtime_object_init_log_exception (instance);
				Scripting::SetScriptingArrayElement<MonoObject*> (array, index, instance);
			}
			
			transfer_instance.instance = instance;
			
			return transfer_instance;
		}
	};
	
	inline int size () const
	{
		AssertIf (forcedSize != NULL && *forcedSize > mono_array_length_safe(array));
		return forcedSize != NULL ? *forcedSize : mono_array_length_safe(array);
	}
	
	inline void ResizeGenericList (size_t size)
	{
		if (forcedSize != NULL)
			*forcedSize = size; 
	}

	inline bool IsGenericList () const
	{
		return forcedSize != NULL;
	}

	iterator begin ()
	{
		iterator i;
		i.array = array;
		i.index = 0;
		i.transfer_instance.klass = elementClass;
		i.transfer_instance.commonClasses = commonClasses;
		i.transfer_instance.transferPrivate = CalculateTransferPrivateVariables(elementClass);
		i.transfer_instance.depthCounter = depthCounter;
		return i;
	}
	
	iterator end ()
	{
		iterator i;
		i.index = size();
		return i;
	}
};


template<>
class SerializeTraits<TransferArrayScriptInstance> : public SerializeTraitsBase<TransferArrayScriptInstance>
{
public:
	
	typedef TransferArrayScriptInstance value_type ;
	inline static const char* GetTypeString (void *ptr = NULL)	{ return "Array"; } \
	inline static bool IsAnimationChannel ()	{ return false; } \
	inline static bool MightContainPPtr ()	{ return true; } \
	inline static bool AllowTransferOptimization ()	{ return false; }
	
#if SUPPORT_SERIALIZED_TYPETREES
	static void Transfer (value_type& data, SafeBinaryRead& transfer) { transfer.TransferSTLStyleArray (data); }
	static void Transfer (value_type& data, StreamedBinaryRead<true>& transfer) { transfer.TransferSTLStyleArray (data); }
	static void Transfer (value_type& data, StreamedBinaryWrite<true>& transfer) { transfer.TransferSTLStyleArray (data); }
#endif
	static void Transfer (value_type& data, StreamedBinaryRead<false>& transfer) { transfer.TransferSTLStyleArray (data); }
	static void Transfer (value_type& data, StreamedBinaryWrite<false>& transfer) { transfer.TransferSTLStyleArray (data); }
	static void Transfer (value_type& data, RemapPPtrTransfer& transfer) { transfer.TransferSTLStyleArray (data); }
#if SUPPORT_TEXT_SERIALIZATION
 	static void Transfer (value_type& data, YAMLRead& transfer) { transfer.TransferSTLStyleArray (data); }
 	static void Transfer (value_type& data, YAMLWrite& transfer) { transfer.TransferSTLStyleArray (data); }
#endif
	
	//template
	static void Transfer (value_type& data, ProxyTransfer& transfer)
	{
		TransferScriptInstance instance;
		instance.instance = NULL;
		instance.klass = data.elementClass;
		instance.commonClasses = data.commonClasses;
		instance.transferPrivate = CalculateTransferPrivateVariables(instance.klass);
		instance.depthCounter = data.depthCounter;
		transfer.TransferSTLStyleArrayWithElement(instance, kNoTransferFlags);
	}
	
	static bool IsContinousMemoryArray ()	{ return false; }
	
	static void ResizeSTLStyleArray (value_type& data, int rs)	
	{
		bool needsResize;
		// When we deal an List<> we only need to reserve enough memory
		// We also should reallocate if we are decreasing the actual array size because in that case otherwise GC memory
		// that is in the unused part of the array must be cleared for which we currently have no simple function.
		if (data.IsGenericList ())
			needsResize = data.array == NULL || rs > mono_array_length_safe(data.array) || rs < data.size ();
		// When we deal with an array we must ensure the array size matches exactly
		else
			needsResize = data.array == NULL || rs != mono_array_length_safe(data.array);

		// Create the array data
		if( needsResize )
		{
			MonoArray* array = mono_array_new (mono_domain_get (), data.elementClass, rs);
			mono_field_set_value (data.instanceObject, data.field, array);
			data.array = array;
		}

		// Synchronize List<> data to
		data.ResizeGenericList(rs);
	}
};


static bool PrepareTransferEmbeddedClassCommonChecks (MonoClass* referencedClass)
{
	int flags = mono_class_get_flags(referencedClass);
	if ((flags & TYPE_ATTRIBUTE_SERIALIZABLE) == 0)
		return false;
	
	if (mono_unity_class_is_abstract (referencedClass) || mono_unity_class_is_interface (referencedClass))
		return false;
	
	MonoImage* image = mono_class_get_image(referencedClass);
	if (image == mono_get_corlib())
		return false;
	else if (GetMonoManager().GetAssemblyIndexFromImage(image) == -1)
		return false;
	
	return true;
}
/// Returns only true on serializable classes.
/// But only if we are in the same assembly.
static bool PrepareTransferEmbeddedArrayClass (MonoClassField* field, MonoClass* referencedClass, MonoObject* instance, unsigned* forcedSize, TransferArrayScriptInstance& output, int currentDepth)
{
	if (!PrepareTransferEmbeddedClassCommonChecks(referencedClass))
		return false;
	
	MonoArray* referencedObject = NULL;
	if (instance)
		mono_field_get_value (instance, field, &referencedObject);
	
	output.array = referencedObject;
	output.elementClass = referencedClass;
	output.commonClasses = &GetMonoManager().GetCommonClasses();
	output.field = field;
	output.instanceObject = instance;
	output.forcedSize = forcedSize;
	output.depthCounter = currentDepth + 1;
	Assert(currentDepth >= 0);
	
	return true;
}

void ApplyScriptDataModified (TransferScriptInstance& info)
{

	if (info.klass != info.commonClasses->guiStyle)
		return;

	if (!info.instance)
		return;

	//this needs to be kept in sinc with SerializedStateReader.as for flash.
	ScriptingInvocation invocation("UnityEngine", "GUIStyle", "Apply");
	invocation.object = info.instance;
	invocation.Invoke();
}



struct GenericListData
{
	MonoArray* array;
	unsigned size;
	int version;
};

bool PrepareArrayTransfer(int type, MonoType* monoType, MonoClassField*& field, MonoObject*& instance, MonoClass*& elementClass, int& elementClassType, unsigned*& forcedSize)
{
	if (type != MONO_TYPE_GENERICINST)
	{
		elementClass = mono_type_get_class_or_element_class(monoType);
		elementClassType = mono_type_get_type(mono_class_get_type (elementClass));
		return true;
	}
	else
	{
		MonoClassField* arrayField = GetMonoArrayFieldFromList(type, monoType, field);
		if (arrayField == NULL)
			return false;
		
		// Grab the current list class if it doesn't exist, create it
		MonoObject* list = NULL;
		MonoClass *klass = mono_class_from_mono_type (monoType);

		if (instance)
		{
			mono_field_get_value (instance, field, &list);
			if (list == NULL)
			{
				list = ScriptingInstantiateObject (klass);
				mono_runtime_object_init_log_exception (list);
			}
		}
		else
		{
			// Mono seems to crash if we dont create an instance of the object prior to accessing the fields
			ScriptingInstantiateObject (klass);
		}
		if (list)
			forcedSize = &ExtractMonoObjectData<GenericListData>(list).size;
		
		MonoType* arrayType = mono_field_get_type(arrayField);
		
		instance = list;
		field = arrayField;
		elementClass = mono_type_get_class_or_element_class(arrayType);
		elementClassType = mono_type_get_type(mono_class_get_type (elementClass));
		return true;
	}
}	


template<class TransferFunction>
void TransferFieldOfTypeArray(ScriptingObjectPtr instance, MonoClass* klass, MonoClassField* field, const char* name, TransferScriptInstance& info, TransferFunction& transfer, MonoType* monoType, int type, TransferMetaFlags metaFlags)
{
	MonoClass* elementClass;
	int elementClassType;
	MonoObject* tempInstance = instance;
	MonoClassField* arrayField = field;
	const CommonScriptingClasses& commonClasses = *info.commonClasses;
	unsigned* forcedSize = NULL;
	if (!PrepareArrayTransfer(type, monoType, arrayField, tempInstance, elementClass, elementClassType, forcedSize))
		return;
	
	if (elementClassType == MONO_TYPE_I4)
		TransferArrayBuiltins<int> (tempInstance, arrayField, name, elementClass, transfer, forcedSize, metaFlags);
	else if (elementClassType == MONO_TYPE_R4)
		TransferArrayBuiltins<float> (tempInstance, arrayField, name, elementClass, transfer, forcedSize, metaFlags);
	else if (elementClassType == MONO_TYPE_U1)
		TransferArrayBuiltins<UInt8> (tempInstance, arrayField, name, elementClass, transfer, forcedSize, metaFlags);
	else if (elementClassType == MONO_TYPE_STRING)
		TransferArrayString (tempInstance, arrayField, name, elementClass, transfer, forcedSize, metaFlags);
	else if (elementClass == commonClasses.vector3)
		TransferArrayBuiltins<Vector3f> (tempInstance, arrayField, name, elementClass, transfer, forcedSize, metaFlags);
	else if (elementClass == commonClasses.vector2)
		TransferArrayBuiltins<Vector2f> (tempInstance, arrayField, name, elementClass, transfer, forcedSize, metaFlags);
	else if (elementClass == commonClasses.vector4)
		TransferArrayBuiltins<Vector4f> (tempInstance, arrayField, name, elementClass, transfer, forcedSize, metaFlags);
	else if (elementClass == commonClasses.quaternion)
		TransferArrayBuiltins<Quaternionf> (tempInstance, arrayField, name, elementClass, transfer, forcedSize, metaFlags);
	else if (elementClass == commonClasses.matrix4x4)
		TransferArrayBuiltins<Matrix4x4f> (tempInstance, arrayField, name, elementClass, transfer, forcedSize, metaFlags);
	else if (elementClass == commonClasses.color)
		TransferArrayBuiltins<ColorRGBAf> (tempInstance, arrayField, name, elementClass, transfer, forcedSize, metaFlags);
	else if (elementClass == commonClasses.color32)
		TransferArrayBuiltins<ColorRGBA32> (tempInstance, arrayField, name, elementClass, transfer, forcedSize, metaFlags);
	else if (elementClass == commonClasses.rect)
		TransferArrayBuiltins<Rectf> (tempInstance, arrayField, name, elementClass, transfer, forcedSize, metaFlags);
	else if (elementClass == commonClasses.layerMask)
		TransferArrayBuiltins<BitField> (tempInstance, arrayField, name, elementClass, transfer, forcedSize, metaFlags);
	else if (elementClassType == MONO_TYPE_BOOLEAN)
		TransferArrayBuiltins<UInt8> (tempInstance, arrayField, name, elementClass, transfer, forcedSize, metaFlags | kEditorDisplaysCheckBoxMask);
	else if (elementClass == commonClasses.rectOffset)
		TransferArrayOfT<RectOffset> (tempInstance, arrayField, name, elementClass, transfer, forcedSize, metaFlags);
	else if (elementClass == commonClasses.guiStyle)
		TransferArrayOfT<GUIStyle> (tempInstance, arrayField, name, elementClass, transfer, forcedSize, metaFlags);
	else if (mono_class_is_enum (elementClass))
	{
		MonoType* enumMonoType = mono_class_enum_basetype (elementClass);
		int enumType = mono_type_get_type (enumMonoType);
		switch (enumType)
		{
			case MONO_TYPE_I4:
				TransferArrayBuiltins<int> (tempInstance, arrayField, name, elementClass, transfer, forcedSize, metaFlags);
				break;
			case MONO_TYPE_U1:
				TransferArrayBuiltins<UInt8> (tempInstance, arrayField, name, elementClass, transfer, forcedSize, metaFlags);
				break;
			default:
				ErrorString (ErrorMessageForUnsupportedEnumField(mono_class_get_type(elementClass), mono_class_get_type(klass),	name));
				return;
		}
	}
	else 
	{
		int elementClassID = Scripting::GetClassIDFromScriptingClass (elementClass);
		
		if (elementClassID != -1)
			TransferArrayPPtr (tempInstance, arrayField, name, elementClass, elementClassID, transfer, forcedSize, metaFlags);
		else
		{
			if (elementClassType != MONO_TYPE_CLASS)
				return;
			
			// Make sure we don't endless loop. Put a hard cap limit on the level of nested types until we have a better general solution.
			if (info.depthCounter > kClassSerializationDepthLimit)
				return;
			
			TransferArrayScriptInstance transferArrayInstance;
			if (PrepareTransferEmbeddedArrayClass (arrayField, elementClass, tempInstance, forcedSize, transferArrayInstance, info.depthCounter))
			{
				transfer.TransferWithTypeString(transferArrayInstance, name, mono_class_get_name(elementClass), metaFlags);
			}
			else
			{
				return;
			}
		}
	}
	
	// Assign List<> to member variable.
	// Need to do it after serialization because only here we know if we can actually serialize the List<>
	if (field != arrayField && instance)
		mono_field_set_value(instance, field, tempInstance);
}
#endif
