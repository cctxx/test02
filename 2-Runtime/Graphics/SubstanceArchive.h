#pragma once

#include "Configuration/UnityConfigure.h"

#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "External/Allegorithmic/builds/Engines/include/substance/handle.h"

class ColorRGBAf;
class ColorRGBA32;
class ProceduralTexture;
class SubstanceSystem;

/* A SubstanceArchive is a resource representation for an imported Substance package.
 * It hosts the persisted binary version of the package that is either imported 
 * or reloaded from disk.
 */


class	SubstanceArchive : public NamedObject
{
protected:	// FIELDS

	// The SBSAR package as a binary
	dynamic_array<UInt8>				m_PackageData;

public:		// METHODS

	REGISTER_DERIVED_CLASS( SubstanceArchive, NamedObject )
	DECLARE_OBJECT_SERIALIZE( SubstanceArchive )
	
	SubstanceArchive( MemLabelId label, ObjectCreationMode mode );

	// Reloads the package from disk
	void AwakeFromLoad( AwakeFromLoadMode awakeMode );
	
#if UNITY_EDITOR
	// Creates the package from a binary SBSBIN file and the XML description file (one time call only when importing the SBSBIN file)
	void Init( const UInt8* _pPackage, unsigned int _PackageLength );
#endif	
	
	UInt8* GetBufferData ();
	unsigned GetBufferSize ();

#if ENABLE_SUBSTANCE
public:
	// Flag indicating whether the package's SBSASM has already pushed for linking
	bool m_isPushed;
	// Set of graph names that have already been generated from this package
	std::set<UnityStr> m_generatedGraphs;

	// Cache of single-package no-const-inputs SBSBIN data used for cloning
public:
	bool   SaveLinkedBinaryData (const UnityStr& prototypeName, const UInt8* data, const int size);
	UInt8* GetLinkedBinaryData  (const UnityStr& prototypeName) const;
	bool   IsCloneDataAvailable (const UnityStr& prototypeName) const;
private:
	std::map< UnityStr, UInt8* > m_linkedBinaryData;
#endif

public:

	// Substance system initialization
	static void InitializeClass (){}
	static void PostInitializeClass ();
	static void CleanupClass ();

#if ENABLE_SUBSTANCE     
	static SubstanceSystem& GetSubstanceSystem() { return *g_SubstanceSystem; }
	static SubstanceSystem* GetSubstanceSystemPtr() { return g_SubstanceSystem; }
private:
	static SubstanceSystem* g_SubstanceSystem;
#endif
};
