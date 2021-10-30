#include "UnityPrefix.h"
#include "GenerateCachedTypeTree.h"
#include "Runtime/Serialize/TypeTree.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Serialize/TransferUtility.h"

class CachedTypeTreeGenerator
{
	TypeTree m_ReusedCachedTypeTreeForDynamicTypeTrees;
	std::map<pair<int, int>, TypeTree> m_Cache;

	public:
	
	const TypeTree& GenerateCachedTypeTree (Object& object, int flags)
	{
		if (object.GetNeedsPerObjectTypeTree ())
		{
			SET_ALLOC_OWNER (this);
			GenerateTypeTree (object, &m_ReusedCachedTypeTreeForDynamicTypeTrees, kSerializeForPrefabSystem);
			return m_ReusedCachedTypeTreeForDynamicTypeTrees;
		}
		else
		{
			pair<int, int> key = std::make_pair (object.GetClassID (), flags);
			if (m_Cache.count (key))
				return m_Cache[key];
			else
			{
				SET_ALLOC_OWNER (this);
				TypeTree& temp = m_Cache[key];
				GenerateTypeTree (object, &temp, flags);
				return temp;
			}	
		}
	}
};

static CachedTypeTreeGenerator* gCachedTypeTreeGenerator = NULL;
CachedTypeTreeGenerator& GetCachedTypeTreeGenerator ()
{
	if (gCachedTypeTreeGenerator == NULL)
		gCachedTypeTreeGenerator = UNITY_NEW_AS_ROOT(CachedTypeTreeGenerator(), kMemTypeTree, "CachedTypeTrees", "");
	return *gCachedTypeTreeGenerator;
}

const TypeTree& GenerateCachedTypeTree (Object& object, int flags)
{
	return GetCachedTypeTreeGenerator ().GenerateCachedTypeTree(object, flags);
}

void CleanupTypeTreeCache ()
{
	UNITY_DELETE(gCachedTypeTreeGenerator, kMemSerialization);
}
