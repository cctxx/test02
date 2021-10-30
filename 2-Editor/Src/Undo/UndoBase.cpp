#include "UnityPrefix.h"
#include "UndoBase.h"
#include "Runtime/BaseClasses/IsPlaying.h"

UndoBase::UndoBase ()
{
	m_Identifier = 0;
	m_PlatformDependentData = NULL;
	m_NamePriority = 0;
	m_UndoGroup = 0;
	m_UndoType = kAssetUndo;
}

UndoBase::~UndoBase ()
{
}

void UndoBase::SetIsSceneUndo (bool sceneUndo)
{
	if (sceneUndo)
	{
		if (IsWorldPlaying())
			m_UndoType = kPlayModeSceneUndo;
		else
			m_UndoType = kEditModeSceneUndo;
	}
	else
		m_UndoType = kAssetUndo;
}