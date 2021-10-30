#include "UnityPrefix.h"

#if ENABLE_UNIT_TESTS

#include "HighLevelTest.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Serialize/DumpSerializedDataToText.h"
#include "Runtime/Serialize/TypeTree.h"
#include "Runtime/BaseClasses/SupportedMessageOptimization.h"
#include "Runtime/Allocator/MemoryManager.h"

#if !UNITY_XENON && !UNITY_ANDROID
#include "PlatformDependent/CommonWebPlugin/Verification.h"
#endif
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Graphics/Texture2D.h"

#include "Runtime/Misc/BuildSettings.h"
#if UNITY_EDITOR
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Src/Prefabs/PrefabMergingTest.h"
#include "Editor/Src/Prefabs/PropertyModification.h"
#endif

void* RunObjectResetTests (void* userData);
void RunAllObjectResetTests();
void TestVersions ();
void TestLaunchingALotOfTasks ();
void TestSerializedPropertyPath ();
void TestEditorComponentUtility();
void TestAssetImporterCallsPostTransfer ();

void RunHighLevelTest ()
{	
	#if !UNITY_XENON && !UNITY_ANDROID && !UNITY_PEPPER
	ErrorIf(!VerifySignature("a", "98ad49733651a83f333d7b91a2f76f77510bfaf89316e34d0025b5b38526c8f8d269e47be24044b9f0dd8675fc781b21fc425da801e33702f9744462c488b400ce9af75b8ae3ec02e6915ce980adcc700fd9d5b2633812ac168d2dea24906bb1cdb3de2bffa4ddeeeb9bc5966b0768e7bc20a8320bf6a0a48cf57b26e31f98b0"));
	ErrorIf(!VerifySignature("abc", "5ac52e1f8f7e331cf1b1c5bef6254cfbc81c4b48296ae4b4e8a8c536de61c9cade1c77c45b047ce880b8f74829f52860c87b2ecf82a96803059e9189c879a63770a071483ab9d7b688cadcf3aa2a857849f86ff3fdecd725cf34e99f5fc5e4496ca9b810c841dc3a5527c7b3bc866ced1daf120ade08614ec6273d293c293880"));
	ErrorIf(!VerifySignature("abcd", "584e7bf6042e8a93585bd9a1c08115705f59e0e7e07656a09b16a526b4b76cbc92f5a7246fdd706031f4186361d8cf87ae2062ab877ea9a2576c59be8a9a3139c374345b61a56046376e174ec510cc0d9be46176f8fb0096aeca259b8e754abd424dd3ef50f3473d552fcba6c946bff1908e70cea57584c9002c4bc245e15e35"));

	ErrorIf(!VerifySignature("http://webplayer.unity3d.com/", "99874573f0651e46ac2c9b0f872a7ecf1ee9c101195bf2d06cd6cd00508b412aa58db8b61fa898ecc0f5e37b7efd6c2762081eb5aa9810e6001c9e5a7d32d9b1fdd6ce11d7cd666a997b2c443f123173dac380ca9e06d9ac7aef7c3794d4f3dfc2b62eaf0684cb5a8ba762700b1ae74ef35ce768312c483020455936ed47bf46"));
	ErrorIf(!VerifySignature("http://wp-fusionfall.unity3d.com/", "606558a42652f6d31130505aa3a9a24dfbd9801b15f79181fac171e0807064799313a6f4369883c276361efb757fd6e979e607be4e981e6ff36308078c13b7bdf59a1f4f06a8f4a5bf78848fb8f4597e2028e9fcd43882a1dcdcf2c1f3756ebf2afab65492e117d53894b81e19435ba8c913a89d07c64c1095b1abe7fa632c15"));
	ErrorIf(!VerifySignature("http://wp-jumpstart.unity3d.com/", "9f5dce353ddccf0adca627c070135fdc8de70925b366ee661d704c4e90c33fa760a452cc741862ca2e87bc722a2cbf1b0948b2116132f88e45a11e6461cc70190db80f97268a55e0cc96d062272d9eb22d5041e9152d1cc980a4448de1282bdf4dbfe27c558ec0f922b1277fcad57b607364d724d0658500a2b5c2dd0245b050"));
	#endif
	
	#if !UNITY_PEPPER
	//Some access the default resources as part of their reset procedure, which are not loaded at this stage in NaCl yet.
	//So we skip this test in Pepper.
	RunAllObjectResetTests ();
	#endif
	
	TestVersions();

	void TestHighLevelShaderNameRegistryTests();
	TestHighLevelShaderNameRegistryTests();
	
	
	#if UNITY_EDITOR
		
	TestPropertyModification();
	TestPrefabModification();
	
	// TestLaunchingALotOfTasks ();

	TestEditorComponentUtility();

	TestAssetImporterCallsPostTransfer();

	#if UNITY_WIN
	void TestGetActualPathWindows();
	TestGetActualPathWindows();
	#endif // #if UNITY_WIN

	#endif // #if UNITY_EDITOR
}

#if UNITY_EDITOR
/// @TODO: Cant do this on every launch of unity, integrate into properl C++ unit test suite
void TestLaunchingALotOfTasks ()
{
	#if UNITY_OSX
	for (int i=0;i<400;i++)
	{
		if (!LaunchTask ("/bin/ls", NULL, NULL))
			LogString("Fail");
	}

	for (int i=0;i<400;i++)
	{
		string output;
		if (!LaunchTask ("/bin/ls", &output, NULL) && !output.empty())
			LogString("Fail");
	}
	#endif
}
#endif


void TestVersions ()
{
	int current = 0;
	int previous = 0;
	
	current = GetNumericVersion("2.6.0b1"); Assert(current > previous); previous = current;
	current = GetNumericVersion("2.6.0b7"); Assert(current > previous); previous = current;
	current = GetNumericVersion("2.6.0b9"); Assert(current > previous); previous = current;
	current = GetNumericVersion("2.6.0"); Assert(current > previous); previous = current;
	current = GetNumericVersion("2.6.0f1"); Assert(current > previous); previous = current;
	current = GetNumericVersion("2.6.0f4"); Assert(current > previous); previous = current;
	current = GetNumericVersion("2.6.0f8"); Assert(current > previous); previous = current;
	current = GetNumericVersion("2.6.0f9"); Assert(current > previous); previous = current;
	current = GetNumericVersion("2.6.0f14"); Assert(current > previous); previous = current;
	current = GetNumericVersion("2.6.1b1"); Assert(current > previous); previous = current;
	current = GetNumericVersion("2.6.1b10"); Assert(current > previous); previous = current;
	current = GetNumericVersion("2.6.1"); Assert(current > previous); previous = current;
	current = GetNumericVersion("2.6.1f1"); Assert(current > previous); previous = current;
	current = GetNumericVersion("2.6.1f15"); Assert(current > previous); previous = current;
	current = GetNumericVersion("2.6.1f16"); Assert(current > previous); previous = current;
	current = GetNumericVersion("2.6.2b1"); Assert(current > previous); previous = current;
}

// If tests fail because they access the GameObject or other components in Awake, DO NOT
// disable them here. Instead fix Awake not to do that in if IsActive() is not true. 
// That will also fix crashes for importing broken prefabs, and is cleaner in general.
const int kDisabledAwakeFromLoad[] = { 
0
, 89 // Cubemap (Error using invalid texture)
, 93 // TextureRect (Error using invalid texture)
, 117 // Texture3D (Error using invalid texture)
, 117 // Texture3D (Error using invalid texture)
, 28 // Texture2D (Error using invalid texture)
, 48 // Shader  (Gives error parsing invalid shader)
, 152 // MovieTexture gives error on awake from load because movie data is not valid
, 156 // TerrainData gives error loading because of invalid heightmap

};

#define kDisabledAwakeFromLoadThreaded kDisabledAwakeFromLoad

#if ENABLE_MEMORY_MANAGER
#define ADD_CUSTOM_ALLOCATOR(Allocator) GetMemoryManager().AddCustomAllocator(Allocator)
#define REMOVE_CUSTOM_ALLOCATOR(Allocator) GetMemoryManager().RemoveCustomAllocator(Allocator)
#else
#define ADD_CUSTOM_ALLOCATOR(Allocator) kMemDefault
#define REMOVE_CUSTOM_ALLOCATOR(Allocator) {}
#endif

class UsePreallocatedMemory : public BaseAllocator
	{
	public:
		UInt8 uninitializedValue;
		
		UsePreallocatedMemory (int size, UInt8 value, const char* name) : BaseAllocator(name)
		{
			baseMemory = memory = (unsigned char*) MemoryManager::LowLevelAllocate(size);
			total = 0;
			totalSize = size;
			uninitializedValue = value;
			for (int i=0;i<size;i++) 
				baseMemory[i] = uninitializedValue;
			id = ADD_CUSTOM_ALLOCATOR(this).label;
		}
		
		virtual ~UsePreallocatedMemory () { 
			MemoryManager::LowLevelFree(baseMemory);
			REMOVE_CUSTOM_ALLOCATOR(this);
		}
		
		virtual void* Allocate (size_t size, int align)
		{
			Assert(*memory == uninitializedValue);
			
			memory = (unsigned char*)AlignPtr(memory + sizeof(SInt32), align);
			*reinterpret_cast<SInt32*> (memory - sizeof(SInt32)) = size;
			memory += size;
			total += size;
			
			Assert(baseMemory+totalSize > memory);

			return memory - size;
		}
		
		virtual void Deallocate (void* p) { total -= *(reinterpret_cast<SInt32*> (p) - 1); }
		virtual bool Contains (const void* p)  { return p > baseMemory && p <= memory ; }
		
		virtual size_t GetPtrSize(const void* ptr) const {return *(reinterpret_cast<const SInt32*> (ptr) - 1); }

		unsigned char* memory;
		unsigned char* baseMemory;
		int total;
		int totalSize;
		MemLabelIdentifier id;
	};

enum { kMaxGeneratedObjectCount = 5000 };

struct Parameters
{
	ObjectCreationMode mode;
	Object* objects[kMaxGeneratedObjectCount];
	BaseAllocator* allocator[kMaxGeneratedObjectCount];
};

void SetObjectDirtyDisallowCalling (Object* obj)
{
	AssertString("SetDirty should never be called from Awake / Reset / Constructor etc");
}

static void ClearAllManagers ()
{
	for (int i=0;i<ManagerContext::kManagerCount;i++)
		SetManagerPtrInContext(i, NULL);
}

static void AssignAllManagers (Object** managers)
{
	for (int i=0;i<ManagerContext::kManagerCount;i++)
		SetManagerPtrInContext(i, managers[i]);
}


void RunAllObjectResetTests ()
{
#if UNITY_EDITOR
	Object::ObjectDirtyCallbackFunction* oldCallBack  = Object::GetDirtyCallback ();
	
////@TODO: Enable this some day. Creating an object, especially when loaded from disk should not mark it dirty. 	
//	Object::RegisterDirtyCallback (SetObjectDirtyDisallowCalling);
	

	Object*	managers[ManagerContext::kManagerCount];
	memcpy(managers, GetManagerContext().m_Managers, sizeof(managers));
	
	Parameters paramMainThread;
	memset(&paramMainThread, 0, sizeof(paramMainThread));
	paramMainThread.mode = kCreateObjectDefault;
	RunObjectResetTests (&paramMainThread);
	
	ClearAllManagers();
	
	Parameters paramResetThread;
	memset(&paramResetThread, 0, sizeof(paramResetThread));
	paramResetThread.mode = kCreateObjectFromNonMainThread;
	Thread resetThread;
	resetThread.Run(RunObjectResetTests, &paramResetThread);
	resetThread.WaitForExit();
	
	for (int i=0;i<kMaxGeneratedObjectCount;i++)
	{
		if (paramResetThread.objects[i])
		{
			Object::RegisterInstanceID(paramResetThread.objects[i]);
			bool ignoreAwake = false;
			for (int j=0;j<sizeof(kDisabledAwakeFromLoadThreaded) / sizeof(int);j++)
				ignoreAwake |= kDisabledAwakeFromLoadThreaded[j] == paramResetThread.objects[i]->GetClassID();
			if (!ignoreAwake)
				paramResetThread.objects[i]->AwakeFromLoad(kDidLoadThreaded);
		}
		
		// IntegrateLoadedImmediately must be called between AwakeFromLoadThreaded and AwakeFromLoad!
		Texture2D::IntegrateLoadedImmediately();
		
		AssignAllManagers(managers);

		DestroyObjectHighLevel(paramResetThread.objects[i]);
		memset(&paramMainThread, 0, sizeof(paramMainThread));
		REMOVE_CUSTOM_ALLOCATOR(paramResetThread.allocator[i]);
		UNITY_DELETE(paramResetThread.allocator[i],kMemDefault);
	}
	
	AssignAllManagers(managers);
	
	Object::RegisterDirtyCallback (oldCallBack);
#endif
}

// Compare only every 4 bytes. This is because we can't detect unaligned variables.
// Unaligned blocks of memory will never be touched, thus will trigger an error.
void CompareMemoryForAllValuesTouched (UsePreallocatedMemory& lhs, UsePreallocatedMemory& rhs, int klassID, const char* msg)
{
	// this is not a valid or complete test. We need custom code in order to test this for each object. turned off for now
	return;
	const int kAlignedDataSize = 16;
	for (int j=0;j<lhs.total;j+=kAlignedDataSize)
	{
		bool anythingModified = false;
		for (int p=0;p<kAlignedDataSize;p++)
		{
			if (lhs.baseMemory[j+p] != lhs.uninitializedValue || rhs.baseMemory[j+p] != rhs.uninitializedValue)
				anythingModified = true;
		}
		
		if (!anythingModified)
		{
			ErrorString("Class is not completely initialized by constructor +  " + string(msg)  + " " + Object::ClassIDToString(klassID));
			return;
		}
	}
}

void DumpData (Object& obj)
{
	printf_console("--- Dumping %s - %s\n", obj.GetClassName().c_str(), obj.GetName());
	TypeTree typeTree;
	dynamic_array<UInt8> output(kMemTempAlloc);
	GenerateTypeTree(obj, &typeTree);
	WriteObjectToVector(obj, &output);
	DumpSerializedDataToText(typeTree, output);
}

const static int kBaseVeryHighInstanceID = (std::numeric_limits<SInt32>::max()-1) - 5000 * 4;
const static int kBaseVeryHighInstanceID2 = (std::numeric_limits<SInt32>::max()-1) - 10000 * 4;

void* RunObjectResetTests (void* userData)
{
	Parameters& parameters = *((Parameters*)userData);
	
	vector<SInt32> klasses;
	Object::FindAllDerivedClasses(ClassID(Object), &klasses);
	
	int kMaxSize = 1024 * 64;

	for (int i=0;i<klasses.size();i++)
	{
		int klassID = klasses[i];
		
		if (Object::ClassIDToRTTI(klassID)->isAbstract)
			continue;
		// Ignore game manager derived
		if (Object::IsDerivedFromClassID(klassID, 9))
			continue;
			
		if(klassID == ClassID(Transform))
			continue;	// ???
		
		if(klassID == 1027) // ignore GUISerializer. This is really just a very custom class that sets up some global state
			continue;
		if(klassID == 1048) // ignore InspectorExpandedState since it is a singleton class
			continue;
		if(klassID == 1049) // ignore AnnotationManager since it is a singleton class
			continue;
		if(klassID == 159) // ignore editor settings
			continue;
		if(klassID == 162) // ignore editor user settings 
			continue;
		if(klassID == 115) // ignore MonoScript, it accesses MonoManager from serialization. This is fine but doesn't fit well into the framework.
			continue;
		if(klassID == 148) // ignore NetworkView, Registers itself in constructor with networkmanager
			continue;
		if(klassID == 1037) // ignore AssetServerCache, singleton
			continue;
		if(klassID == 142) // ignore AssetBundle, for it to destroy we need to call UnloadAssetBundle
			continue;
		if(klassID == 184 || klassID == 185 || klassID == 186) // ignore SubstanceArchive, ProceduralMaterial, ProceduralTexture
			continue;
		
		////// @TODO: Work in progress, need to be fixed!!!!
		if (klassID == 156) // Terrain has some issues left
			continue;

		bool ignoreAwake = false;
		for (int j=0;j<sizeof(kDisabledAwakeFromLoad) / sizeof(int);j++)
			ignoreAwake |= kDisabledAwakeFromLoad[j] == klassID;

		/// Run for editor classes , we might be using datatemp
		
		dynamic_array<UInt8> serialized1(kMemTempAlloc);
		dynamic_array<UInt8> serialized2(kMemTempAlloc);
		
		UsePreallocatedMemory* mem1 = UNITY_NEW(UsePreallocatedMemory (kMaxSize, 0, "Mem1"),kMemDefault);
		MemLabelId mem1Label = ADD_CUSTOM_ALLOCATOR(mem1);
		Object* obj1 = Object::Produce(klassID, kBaseVeryHighInstanceID + i * 2, mem1Label, parameters.mode);

		obj1->Reset();
		
		WriteObjectToVector(*obj1, &serialized1);

		UsePreallocatedMemory* mem2 = UNITY_NEW(UsePreallocatedMemory (kMaxSize, 255, "Mem2"),kMemDefault);
		MemLabelId mem2Label = ADD_CUSTOM_ALLOCATOR(mem2);
		Object* obj2 = Object::Produce(klassID, kBaseVeryHighInstanceID + 2 + i * 2, mem2Label, parameters.mode);

		obj2->Reset();
		
		WriteObjectToVector(*obj2, &serialized2);
		
		if (!ignoreAwake)
		{
			if (parameters.mode == kCreateObjectDefault)
			{
				obj1->AwakeFromLoad(kDidLoadFromDisk);
				obj2->AwakeFromLoad(kDidLoadFromDisk);
			}
			else if (parameters.mode == kCreateObjectFromNonMainThread)
			{
				obj1->AwakeFromLoadThreaded();
				obj2->AwakeFromLoadThreaded();
			}
		}
		else
		{
			obj1->HackSetAwakeWasCalled();
			obj2->HackSetAwakeWasCalled();
		}
		
		if (mem1->total != mem2->total)
		{
			ErrorString("Class is different size after initialized by constructor + Reset " + Object::ClassIDToString(klassID));
			// Dump serialized data to console.log
			DumpData(*obj1);
			DumpData(*obj2);
		}
		
		if( !serialized1.equals(serialized2) )
		{
			ErrorString("Class is serialized different after initialized by constructor + Reset " + Object::ClassIDToString(klassID));
			// Dump serialized data to console.log			
			DumpData(*obj1);
			DumpData(*obj2);
		}
		
		if (!ignoreAwake && parameters.mode == kCreateObjectDefault)
		{
			// Compare only every 4 bytes. This is because we can't detect unaligned variables.
			// Unaligned blocks of memory will never be touched, thus will trigger an error.
			CompareMemoryForAllValuesTouched (*mem1, *mem2, klassID, "Reset");
		}
		
		UsePreallocatedMemory* mem3 = UNITY_NEW(UsePreallocatedMemory (kMaxSize, 125, "Mem3"),kMemDefault);
		MemLabelId mem3Label = ADD_CUSTOM_ALLOCATOR(mem3);
		Object* obj3 = Object::Produce(klassID, kBaseVeryHighInstanceID2 + i * 2, mem3Label, parameters.mode);

		ReadObjectFromVector(obj3, serialized1);
		obj3->HackSetAwakeWasCalled();

		CompareMemoryForAllValuesTouched (*mem1, *mem3, klassID, "Serialize Read");

		WriteObjectToVector(*obj3, &serialized2);
		if(!serialized1.equals(serialized2))
		{
			ErrorString("Class is serialized different after write and reading back " + Object::ClassIDToString(klassID));
			// Dump serialized data to console.log			
			DumpData(*obj3);
		}
		
		if (parameters.mode == kCreateObjectDefault)
		{
			DestroyObjectHighLevel(obj1);
			DestroyObjectHighLevel(obj2);
			DestroyObjectHighLevel(obj3);
		
			AssertIf(mem1->total != 0);
			AssertIf(mem2->total != 0);
			AssertIf(mem3->total != 0);
			
			REMOVE_CUSTOM_ALLOCATOR(mem1);
			UNITY_DELETE( mem1,kMemDefault);
			REMOVE_CUSTOM_ALLOCATOR(mem2);
			UNITY_DELETE( mem2,kMemDefault);
			REMOVE_CUSTOM_ALLOCATOR(mem3);
			UNITY_DELETE( mem3,kMemDefault);
		}
		else
		{
			parameters.objects[i * 3 + 0] = obj1;
			parameters.objects[i * 3 + 1] = obj2;
			parameters.objects[i * 3 + 2] = obj3;

			parameters.allocator[i * 3 + 0] = mem1;
			parameters.allocator[i * 3 + 1] = mem2;
			parameters.allocator[i * 3 + 2] = mem3;
		}
		
	}

	return NULL;
}

struct TestData
{
	PPtr<Texture2D> m_Tex;
	Texture2D* m_CachedTexture;
	
	// Property m_ActiveMatrixName; /// HOW TO SUPPORT THIS???

	Vector2f m_Position;
	Vector2f m_Scale;
	
	TestData ()
	{
		m_CachedTexture = NULL;
		m_Tex = NULL;
		m_Position = Vector2f(0,0);
		m_Scale = Vector2f(1,1);
	}
};

#if UNITY_EDITOR
#include "Editor/Src/AssetPipeline/AssetImporter.h"

void TestAssetImporterCallsPostTransfer ()
{
	vector<SInt32> klasses;
	Object::FindAllDerivedClasses (ClassID (AssetImporter), &klasses);
	UsePreallocatedMemory* mem1 = new UsePreallocatedMemory (50 * 1024, 0, "Mem1");
	MemLabelId mem1Label = ADD_CUSTOM_ALLOCATOR(mem1);

	for (int i = 0; i < klasses.size (); i++)
	{
		int klassID = klasses[i];
		AssetImporter* importer = static_cast<AssetImporter*> (Object::Produce (klassID, 0, mem1Label, kCreateObjectDefault));
		importer->Reset ();
		importer->HackSetAwakeWasCalled ();

		YAMLWrite write (0);
		importer->VirtualRedirectTransfer (write);
		std::string str;
		write.OutputToString (str);

		size_t offset = str.find ("m_UserData:");
		if (offset == string::npos)
			ErrorString (Format ("%s did not call PostTransfer in its Transfer function", importer->GetClassName ().c_str ()).c_str ());

		DestroyObjectHighLevel (importer);
	}

	REMOVE_CUSTOM_ALLOCATOR(mem1);
	delete mem1;
}

#endif



#endif
