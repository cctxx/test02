#ifndef UMBRA_BACKWARDS_COMPATIBILITY_H
#define UMBRA_BACKWARDS_COMPATIBILITY_H

#include "UmbraBackwardsCompatibilityDefine.h"
#include "UmbraTomeData.h"

#include "External/Umbra/builds/interface/runtime/umbraTome.hpp"
#include "External/Umbra/builds/interface/runtime/umbraQuery.hpp"

#if SUPPORT_BACKWARDS_COMPATIBLE_UMBRA
#   include "External/Umbra_3_0/source/interface/runtime/umbraTome_3_0.hpp"
#   include "External/Umbra_3_0/source/interface/runtime/umbraQuery_3_0.hpp"
#endif

inline void CleanupUmbraTomeData (UmbraTomeData& tomeData)
{
	if (tomeData.tome)
	{
		Umbra::TomeLoader::freeTome(tomeData.tome);
		tomeData.tome = NULL;
	}

#if SUPPORT_BACKWARDS_COMPATIBLE_UMBRA
	if (tomeData.legacyTome)
	{
		Umbra_3_0::TomeLoader::freeTome(tomeData.legacyTome);
		tomeData.legacyTome = NULL;
	}
#endif	
}
inline const UmbraTomeData LoadUmbraTome (const UInt8* buf, size_t bytes)
{
	if (buf == NULL || bytes == 0)
		return UmbraTomeData ();
	
    const Umbra::Tome* tome = Umbra::TomeLoader::loadFromBuffer(buf, bytes);
	if (tome->getStatus() == Umbra::Tome::STATUS_OK)
	{
		UmbraTomeData tomeData;
		tomeData.tome = tome;
		return tomeData;
	}
	
	
#if SUPPORT_BACKWARDS_COMPATIBLE_UMBRA
	if (tome && tome->getStatus() == Umbra::Tome::STATUS_OLDER_VERSION)
	{
		const Umbra_3_0::Tome* legacyTome = Umbra_3_0::TomeLoader::loadFromBuffer(buf, bytes);
		if (legacyTome && legacyTome->getStatus() == Umbra_3_0::Tome::STATUS_OK)
		{
			UmbraTomeData tomeData;
			tomeData.legacyTome = legacyTome;
			return tomeData;
		}
		
		ErrorString("Failed to load legacy tome data");
		return UmbraTomeData();
	}
#endif
	
	WarningString ("Loading deprecated Occlusion Culling is not supported. Please rebake the occlusion culling data.");
	return UmbraTomeData ();
}

inline void SetGateStates( Umbra::Query* query, const UmbraTomeData& tomeData, Umbra::GateStateVector& gateVector)
{
#if SUPPORT_BACKWARDS_COMPATIBLE_UMBRA
	if (tomeData.IsLegacyTome ())
	{
		((Umbra_3_0::QueryExt*)query)->setGateStates((const Umbra_3_0::GateStateVector*)&gateVector);
		return;
	}
#endif
	
	query->setGateStates(&gateVector);
}

#if SUPPORT_BACKWARDS_COMPATIBLE_UMBRA
	#define UMBRA_TOME_METHOD(TOME,method) (TOME.tome != NULL ? TOME.tome->method : TOME.legacyTome->method)
#else
	#define UMBRA_TOME_METHOD(TOME,method) (TOME.tome->method)
#endif


#endif