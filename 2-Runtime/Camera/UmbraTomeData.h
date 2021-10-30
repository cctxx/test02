#pragma once

namespace Umbra { class Tome; };
namespace Umbra_3_0 { class Tome; };

struct UmbraTomeData
{
	const Umbra::Tome*      tome;
	const Umbra_3_0::Tome*  legacyTome;
	
	UmbraTomeData ()				{ tome = NULL; legacyTome = NULL; }
	
	
	bool HasTome () const			{ return tome != NULL || legacyTome != NULL; }
	bool IsLegacyTome () const		{ return legacyTome != NULL; }
	
	
	friend inline bool operator == (const UmbraTomeData& lhs, const UmbraTomeData& rhs)
	{
		return lhs.tome == rhs.tome && lhs.legacyTome == rhs.legacyTome;
	}
};