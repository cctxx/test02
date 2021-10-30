#ifndef RUNTIME_CLASS_HASHING_H_
#define RUNTIME_CLASS_HASHING_H_

#include "Runtime/Scripting/Backend/ScriptingTypes.h"
#include "Runtime/Utilities/vector_map.h"

#include <vector>
#include <map>
#include <string>

// Generates hash for each class passed in 'classIDs' (all *runtime only* classes otherwise) by generating a TypeTree and hashing it recursively.
void CalculateHashForRuntimeClasses (vector_map<int, UInt32>& typeTreeHashes, TransferInstructionFlags transferInstructions);
void CalculateHashForClasses (vector_map<int, UInt32>& typeTreeHashes, std::vector<SInt32> const& classIDs, TransferInstructionFlags transferInstructions);

// move this out
namespace cil
{	

struct FieldInfoMono
{
	MonoString* name;
	MonoString* typeName;
};

struct ClassInfoMono
{
	MonoString* name;
	MonoArray* fieldInfos;
};

class TypeDB
{
public:
	struct Field
	{
		Field (std::string const& typeName_, std::string const& name_) : typeName(typeName_), name(name_) {}
		
		bool operator!=(Field const& f) const { return typeName != f.typeName || name != f.name; }
		
		std::string typeName;
		std::string name;
	};
	
	typedef std::vector<Field> field_cont_t;
	struct Class
	{
		// TODO: create field/type index for fast search
		bool HasField (std::string const& typeName, std::string const& fieldName) const
		{
			for (field_cont_t::const_iterator it=fields.begin (); it != fields.end (); ++it)
				if (it->typeName == typeName && it->name == fieldName)
					return true;
			
			return false;
		}
		
		field_cont_t fields;
	};
	
	// Map *full* class name to the contents of the class.
	typedef std::map<std::string, Class> class_map_t;
	typedef class_map_t::iterator class_iterator;
	typedef class_map_t::const_iterator class_const_iterator;
	class_map_t m_Classes;
	
	// We also create a multimap from a short class name to Class structure.
	// The reason is that we can get short name very fast in serialization.
	// In case we have more than on short-name class names, we resort
	// to full name search to find out which class precisely we're working with.
	typedef std::multimap<std::string, Class const*> short_name_map_t;
	short_name_map_t m_ShortNameMultimap;
	
	// 
	class_iterator class_begin () { return m_Classes.begin (); }
	class_const_iterator class_begin () const { return m_Classes.begin (); }
	class_iterator class_find (std::string const& className) { return m_Classes.find (className); }
	class_const_iterator class_find (std::string const& className) const { return m_Classes.find (className); }
	class_iterator class_end () { return m_Classes.end (); }	
	class_const_iterator class_end () const { return m_Classes.end (); }
	
	TypeDB ();
	~TypeDB ();
	
	void AddInfo (MonoArray* classInfos);
	void Clear ();
	
	void Dump ();
};

class SerializeTracker
{
	TypeDB::Class const* m_Class;
	TypeDB::field_cont_t::const_iterator m_CurrentField;
	
public:
	SerializeTracker (TypeDB const* typeDB, char const* className);
	SerializeTracker (TypeDB const* typeDB, MonoClass* klass);
	
	bool IsClassValid () const { return m_Class != NULL; }
	bool IsFieldValid () const { Assert (m_Class); return m_CurrentField != m_Class->fields.end (); }
	TypeDB::Field const& CurrentField () const { return *m_CurrentField; }
	bool HasField (std::string const& typeName, std::string const& fieldName) { return m_Class->HasField (typeName, fieldName); }
	bool IsCurrent (std::string const& typeName, std::string const& fieldName) {
		return m_CurrentField->typeName == typeName && m_CurrentField->name == fieldName;
	}
	
	SerializeTracker& operator++() { Assert (m_Class); m_CurrentField++; return *this; }
};

class ExtraFieldTester
{
	// Using pointers to use compiler's default-generated operator= and to imply
	// that objects passed have to outlive this object's lifetime.
	struct Extra
	{
		Extra (std::string const* klassname, TypeDB::Field const* f, TypeDB::Field const* expected_) : klass(klassname), field(f), expected(expected_) {}
		std::string const* klass;
		TypeDB::Field const* field;
		TypeDB::Field const* expected; // can be NULL
	};
	
	typedef std::vector<Extra> extra_cont_t;
	extra_cont_t m_ExtraFields;
	
public:
	ExtraFieldTester (TypeDB& srcDB, TypeDB& dstDB);
	
	bool HasExtra () { return !m_ExtraFields.empty (); }
	void PrintErrorsToConsole ();	
	
	typedef extra_cont_t::const_iterator const_iterator;
	const_iterator begin () const { return m_ExtraFields.begin (); }
	const_iterator end () const { return m_ExtraFields.end (); }
};

// We store a pointer to the types for the current target platform here and
// use this when serializing fot the player. The pointer must be NULL if no DB should be used.
extern TypeDB* g_CurrentTargetTypeDB;

struct AutoResetTargetTypeDB
{
	AutoResetTargetTypeDB (TypeDB* db) { g_CurrentTargetTypeDB = db; }
	~AutoResetTargetTypeDB () { g_CurrentTargetTypeDB = NULL; }
};
	
}

#endif
