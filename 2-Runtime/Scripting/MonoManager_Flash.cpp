#include "UnityPrefix.h"

#include "MonoManager_Flash.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/Scripting/CommonScriptingClasses.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"

#include "Runtime/Scripting/Backend/Flash/ScriptingTypeProvider_Flash.h"
#include "Runtime/Scripting/Backend/Flash/ScriptingMethodFactory_Flash.h"

#include "Runtime/Scripting/Backend/ScriptingMethodRegistry.h"

#include "Runtime/Mono/MonoBehaviourSerialization_ByCodeGeneration.h"

MonoManager::MonoManager (MemLabelId label, ObjectCreationMode mode)
	: ScriptingManager(label, mode, UNITY_NEW( ScriptingTypeProvider_Flash(), kMemManager), UNITY_NEW( ScriptingMethodFactory_Flash(), kMemManager))
{
	FillCommonScriptingClasses(m_CommonScriptingClasses);
}

MonoManager::~MonoManager ()
{

}

template<class TransferFunction>
void MonoManager::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	transfer.SetVersion(2);
	transfer.Transfer (m_MonoScriptManager.m_RuntimeScripts, "m_Scripts");
}


IMPLEMENT_OBJECT_SERIALIZE (MonoManager)
IMPLEMENT_CLASS (MonoManager)
GET_MANAGER (MonoManager)
GET_MANAGER_PTR (MonoManager)
