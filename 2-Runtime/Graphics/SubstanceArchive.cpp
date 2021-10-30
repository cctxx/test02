#include "UnityPrefix.h"
#include "SubstanceArchive.h"
#include "SubstanceSystem.h"

#if UNITY_EDITOR
#include "Editor/Src/AssetPipeline/SubstanceImporter.h"
#include "Editor/Src/AssetPipeline/SubstanceCache.h"
#include "Editor/Src/EditorUserBuildSettings.h"
#endif

using namespace std;

IMPLEMENT_CLASS_HAS_POSTINIT( SubstanceArchive )

IMPLEMENT_OBJECT_SERIALIZE( SubstanceArchive )

SubstanceArchive::SubstanceArchive( MemLabelId label, ObjectCreationMode mode )
	: Super( label, mode )
{
#if ENABLE_SUBSTANCE
	m_isPushed = false;
	m_generatedGraphs.clear();
	m_linkedBinaryData.clear();
#endif
}

SubstanceArchive::~SubstanceArchive()
{
#if ENABLE_SUBSTANCE
	GetSubstanceSystem().NotifyPackageDestruction(this);
	for (std::map< UnityStr, UInt8* >::iterator i=m_linkedBinaryData.begin() ; i != m_linkedBinaryData.end() ; ++i)
	{
		UNITY_FREE(kMemSubstance, i->second);
	}
	m_linkedBinaryData.clear();
#endif
}

void SubstanceArchive::AwakeFromLoad( AwakeFromLoadMode awakeMode )
{
	Super::AwakeFromLoad( awakeMode );
}

UInt8* SubstanceArchive::GetBufferData ()
{
	return &m_PackageData[0];
}

unsigned SubstanceArchive::GetBufferSize ()
{
	return m_PackageData.size();
}

#if ENABLE_SUBSTANCE
bool SubstanceArchive::SaveLinkedBinaryData( const UnityStr& prototypeName, const UInt8* data, const int size)
{
	if (IsCloneDataAvailable(prototypeName))
	{
		WarningStringMsg("Trying to save linked substance data to a package that already has it");
		return false;
	}

	UInt8* linkedBinaryData = (UInt8*) UNITY_MALLOC_ALIGNED_NULL(kMemSubstance, size, 32);
	if (!linkedBinaryData)
	{
		WarningStringMsg("Could not allocate memory for a Substance package linked data");
		return false;
	}

	memcpy( (void *) linkedBinaryData, (const void *) data, size);
	m_linkedBinaryData[prototypeName] = linkedBinaryData;
	return true;
}

UInt8* SubstanceArchive::GetLinkedBinaryData( const UnityStr& prototypeName ) const
{
	return (m_linkedBinaryData.find(prototypeName)->second);
}

bool SubstanceArchive::IsCloneDataAvailable( const UnityStr& prototypeName ) const
{
	return (m_linkedBinaryData.count(prototypeName) == 1);
}
#endif

#if UNITY_EDITOR
void SubstanceArchive::Init( const UInt8* _pPackage, unsigned int _PackageLength )
{
	// Copy the package content
	m_PackageData.assign( _pPackage, _pPackage + _PackageLength );

    AwakeFromLoad(kDefaultAwakeFromLoad);
}
#endif

template<class T>
void SubstanceArchive::Transfer( T& transfer )
{
	Super::Transfer( transfer );
	
	transfer.Transfer( m_PackageData, "m_PackageData" );
	transfer.Align();
}

void SubstanceArchive::PostInitializeClass ()
{
#if ENABLE_SUBSTANCE
	g_SubstanceSystem = new SubstanceSystem();	
#endif
}

void SubstanceArchive::CleanupClass ()
{
#if ENABLE_SUBSTANCE
	delete g_SubstanceSystem;
#endif
}

#if ENABLE_SUBSTANCE
SubstanceSystem* SubstanceArchive::g_SubstanceSystem = NULL;
#endif
