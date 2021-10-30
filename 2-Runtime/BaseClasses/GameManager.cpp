#include "UnityPrefix.h"
#include "GameManager.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "ManagerContext.h"

GameManager::~GameManager ()
{
	for (int i=0;i<ManagerContext::kManagerCount;i++)
	{
		if (GetManagerContext().m_Managers[i] == this)
			SetManagerPtrInContext(i, NULL);
	}
}

LevelGameManager::~LevelGameManager () { }
GlobalGameManager::~GlobalGameManager () { }

template<class TransferFunction>
void LevelGameManager::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
}

template<class TransferFunction>
void GlobalGameManager::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
}

char const* GlobalGameManager::GetName () const
{
	return GetClassName ().c_str ();
}

GameManager* GetGameManagerIfExists (int index)
{
	GameManager* manager = static_cast<GameManager*> (GetManagerPtrFromContext(index));
	Assert(manager == dynamic_pptr_cast<GameManager*> (GetManagerPtrFromContext(index)));
	return manager;
}

LevelGameManager::LevelGameManager(MemLabelId label, ObjectCreationMode mode) : Super(label, mode)
{ }

GlobalGameManager::GlobalGameManager(MemLabelId label, ObjectCreationMode mode) : Super(label, mode)
{ }



IMPLEMENT_CLASS (LevelGameManager)
IMPLEMENT_CLASS (GlobalGameManager)
IMPLEMENT_CLASS (GameManager)

IMPLEMENT_OBJECT_SERIALIZE (LevelGameManager)
IMPLEMENT_OBJECT_SERIALIZE (GlobalGameManager)

INSTANTIATE_TEMPLATE_TRANSFER_EXPORTED (LevelGameManager)
INSTANTIATE_TEMPLATE_TRANSFER_EXPORTED (GlobalGameManager)
