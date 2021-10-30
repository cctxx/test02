#include "UnityPrefix.h"
#include "GraphicsSettings.h"
#include "RenderManager.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#if UNITY_EDITOR
#include "Runtime/Misc/ResourceManager.h"
#endif


GraphicsSettings::GraphicsSettings (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_NeedToInitializeDefaultShaders(false)
{
}

GraphicsSettings::~GraphicsSettings ()
{
}


void GraphicsSettings::InitializeClass ()
{
	RenderManager::InitializeClass();
}

void GraphicsSettings::CleanupClass ()
{
	RenderManager::CleanupClass();
}


void GraphicsSettings::SetDefaultAlwaysIncludedShaders()
{
	#if UNITY_EDITOR
	m_AlwaysIncludedShaders.clear();
	if (BuiltinResourceManager::AreResourcesInitialized())
		m_AlwaysIncludedShaders.push_back(GetBuiltinExtraResource<Shader> ("Normal-Diffuse.shader"));
	else
		m_NeedToInitializeDefaultShaders = true;
	SetDirty();
	#endif
}

bool GraphicsSettings::IsAlwaysIncludedShader (PPtr<Shader> shader) const
{
	for (int i = 0; i < m_AlwaysIncludedShaders.size (); ++i)
		if (m_AlwaysIncludedShaders[i] == shader)
			return true;

	return false;
}

#if UNITY_EDITOR

void GraphicsSettings::AddAlwaysIncludedShader (PPtr<Shader> shader)
{
	m_AlwaysIncludedShaders.push_back (shader);
	SetDirty ();
}

#endif

void GraphicsSettings::Reset ()
{
	Super::Reset ();
	SetDefaultAlwaysIncludedShaders ();
}

template<class TransferFunction>
void GraphicsSettings::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	transfer.Transfer (m_AlwaysIncludedShaders, "m_AlwaysIncludedShaders");
}

IMPLEMENT_CLASS_HAS_INIT (GraphicsSettings)
IMPLEMENT_OBJECT_SERIALIZE (GraphicsSettings)
GET_MANAGER (GraphicsSettings)
