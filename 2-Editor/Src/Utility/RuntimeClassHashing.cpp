#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"

#include "Editor/Src/AssetPipeline/MdFourGenerator.h"
#include "Editor/Src/Utility/RuntimeClassHashing.h"
#include "Runtime/Serialize/TypeTree.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Serialize/SerializationMetaFlags.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/BaseClasses/GameManager.h"
#include "Runtime/Misc/Allocator.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Utilities/algorithm_utility.h"
#include "Runtime/Scripting/ScriptingUtility.h"

UInt32 CalculateClassHash (int classID, TransferInstructionFlags transferInstructions)
{
	Object* obj = Object::Produce (classID, 0, kMemBaseObject, kCreateObjectDefaultNoLock);
	obj->Reset();
	obj->HackSetAwakeWasCalled ();
	
	TypeTree typeTree;
	GenerateTypeTree (*obj, &typeTree, transferInstructions);
	UInt32 hash = HashTypeTree (typeTree);
	DestroyObjectHighLevel (obj, true);
	
	return hash;
}

void CalculateHashForClasses (vector_map<int, UInt32>& typeTreeHashes, std::vector<SInt32> const& classIDs, TransferInstructionFlags transferInstructions)
{
	LockObjectCreation ();
	for (int i=0; i<classIDs.size(); ++i)
	{
		int classID = classIDs[i];
		UInt32 hash = CalculateClassHash (classID, transferInstructions);
		typeTreeHashes.insert (std::make_pair(classID, hash));
	}
	UnlockObjectCreation ();
	
#if 0
	printf_console ("Generated hashes for the following classes, count: %d\n", (int)typeTreeHashes.size ());
	size_t index = 0;
	for (vector_map<int, UInt32>::iterator it = typeTreeHashes.begin (), end = typeTreeHashes.end (); it != end; ++it)
	{
		int classID = it->first;
		printf_console ("  [%2d] %s (%d) - 0x%08X\n", index++, Object::ClassIDToString(classID).c_str(), classID, it->second);
	}
#endif
}

namespace {

static SInt16 g_skipClassIDs[] = {
	142, // AssetBundle
};

static size_t const g_skipClassIDsCount = sizeof(g_skipClassIDs)/sizeof(g_skipClassIDs[0]);

struct UnwantedClassPredicate {
	bool operator () (int classID) const
	{
		if (classID >= ClassID(SmallestEditorClassID))
			return true;
		
		if (Object::IsDerivedFromClassID (classID, ClassID (GlobalGameManager)))
			return true;
		
		if (std::binary_search (g_skipClassIDs, g_skipClassIDs + g_skipClassIDsCount, classID))
			return true;
		
		return false;
	}
};
}

void CalculateHashForRuntimeClasses (vector_map<int, UInt32>& typeTreeHashes, TransferInstructionFlags transferInstructions)
{
	typeTreeHashes.clear ();
	
	std::vector<SInt32> classIDs;
	Object::FindAllDerivedClasses (ClassID(Object), &classIDs);

	Assert (::is_sorted (g_skipClassIDs, g_skipClassIDs + g_skipClassIDsCount));
	
	classIDs.erase (std::remove_if (classIDs.begin (),  classIDs.end (), UnwantedClassPredicate ()), classIDs.end ());
	CalculateHashForClasses (typeTreeHashes, classIDs, transferInstructions);
}

namespace cil
{
	
TypeDB::TypeDB()
{
}

TypeDB::~TypeDB()
{
}

void TypeDB::Clear ()
{
	TypeDB temp;
	std::swap (m_Classes, temp.m_Classes);
}
	
void TypeDB::AddInfo (MonoArray* classInfos)
{
	for (size_t i=0, len = mono_array_length_safe (classInfos); i < len; ++i)
	{
		ClassInfoMono const& ciMono = GetMonoArrayElement<ClassInfoMono> (classInfos, i);
		
		std::string className = MonoStringToCpp(ciMono.name);		

		TypeDB::Class c;
		c.fields.reserve (mono_array_length_safe(ciMono.fieldInfos));
		
		for (size_t q=0, qlen=mono_array_length_safe(ciMono.fieldInfos); q<qlen; ++q)
		{
			FieldInfoMono const& fiMono = GetMonoArrayElement<FieldInfoMono>(ciMono.fieldInfos, q);
			std::string name = MonoStringToCpp(fiMono.name);
			std::string typeName = MonoStringToCpp(fiMono.typeName);
			
			c.fields.push_back (TypeDB::Field(typeName, name));
		}

		// Swap-in the constructed value into the map
		std::swap (m_Classes[className], c);
		
		std::string shortName = className;
		size_t dot = className.find_last_of ('.');
		if (dot != std::string::npos)
			shortName.erase (0, dot+1);
		
		m_ShortNameMultimap.insert (std::make_pair (shortName, &m_Classes[className]));
	}
}
	
void TypeDB::Dump ()
{
	for (cil::TypeDB::class_map_t::iterator it=m_Classes.begin (); it != m_Classes.end (); ++it)
	{
		printf_console ("%s: %d fields\n", it->first.c_str(), it->second.fields.size ());
		cil::TypeDB::Class const& klass = it->second;
		for (size_t i=0; i<klass.fields.size (); ++i)
		{
			printf_console ("  [%2d]: %s %s\n", i, klass.fields[i].typeName.c_str(), klass.fields[i].name.c_str());
		}
	}
}

SerializeTracker::SerializeTracker (TypeDB const* typeDB, char const* className)
:	m_Class(NULL)
{
	if (typeDB)
	{
		TypeDB::class_const_iterator classIt = typeDB->class_find (className);
		if (classIt != typeDB->class_end ())
		{
			m_Class = &classIt->second;
			m_CurrentField = m_Class->fields.begin ();
		}
	}
}
	
SerializeTracker::SerializeTracker (TypeDB const* typeDB, MonoClass* klass)
:	m_Class(NULL)
{
	if (typeDB)
	{
		char const* shortClassName = mono_class_get_name (klass);
		TypeDB::short_name_map_t::const_iterator lo = typeDB->m_ShortNameMultimap.lower_bound (shortClassName);
		if (lo != typeDB->m_ShortNameMultimap.end ())
		{
			TypeDB::short_name_map_t::const_iterator up = typeDB->m_ShortNameMultimap.upper_bound (shortClassName);
			TypeDB::short_name_map_t::const_iterator losave = lo++;
			
			// If equal, we have only one class name
			if (lo == up)
				m_Class = losave->second;
		}

		if (!m_Class)
		{
			char* fullClassName = mono_type_get_name_full (mono_class_get_type (klass), MONO_TYPE_NAME_FORMAT_IL);
			
			TypeDB::class_const_iterator classIt = typeDB->class_find (fullClassName);
			
			g_free (fullClassName);
			
			if (classIt == typeDB->class_end ())
				return;

			m_Class = &classIt->second;
		}

		m_CurrentField = m_Class->fields.begin ();
	}
}
	

ExtraFieldTester::ExtraFieldTester (TypeDB& srcDB, TypeDB& dstDB)
{
	// Iterate all dstDB class types and check if any of them has more serializable fields than src,
	// in which case we have an error.
	for (TypeDB::class_const_iterator it = dstDB.class_begin (); it != dstDB.class_end (); ++it)
	{
		TypeDB::class_const_iterator srcIt = srcDB.class_find (it->first);
		if (srcIt == srcDB.class_end ())
			continue;
		
		TypeDB::Class const& srcClass = srcIt->second;
		TypeDB::Class const& dstClass = it->second;
		
		TypeDB::field_cont_t::const_iterator
			sf_it = srcClass.fields.begin (), sf_end = srcClass.fields.end (),
			df_it = dstClass.fields.begin (), df_end = dstClass.fields.end ();
		
		while (sf_it != sf_end && df_it != df_end)
		{
//			std::string sf = sf_it->name;
//			std::string df = df_it->name;
			
			if (*sf_it != *df_it)
			{
				// If we still have src field in the dst, means we have additional field(s)
				// in dest (we shouldn't have had skipped any as field are in same order!),
				// so those recording.
				if (dstClass.HasField (sf_it->typeName, sf_it->name))
				{
					m_ExtraFields.push_back (Extra (&srcIt->first, &*df_it++, &*sf_it));
				}
				else
				{
					// Forward source, it's OK to have more fields in the source.
					++sf_it;
				}
			}
			else
				++sf_it, ++df_it;
		}
		
		// Do we have any trailing fields?
		if (sf_it == sf_end)
		{
			while (df_it != df_end)
				m_ExtraFields.push_back (Extra (&srcIt->first, &*df_it++, NULL));
		}
		
	}
}
	
void ExtraFieldTester::PrintErrorsToConsole ()
{
	for (cil::ExtraFieldTester::const_iterator it = begin (), end = this->end (); it != end; ++it)
	{
		std::string expected;
		if (it->expected)
			expected = Format (" (expected '%s' of type '%s')", it->expected->name.c_str(), it->expected->typeName.c_str());
		ErrorString (Format ("Type '%s' has an extra field '%s' of type '%s' in the player and thus can't be serialized%s",
							 it->klass->c_str(), it->field->name.c_str(), it->field->typeName.c_str(), expected.c_str()));
	}
}

TypeDB* g_CurrentTargetTypeDB = NULL;

}
