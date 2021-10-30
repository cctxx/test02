#pragma once

#include "Runtime/Scripting/Backend/ScriptingTypes.h"

static bool CalculateTransferPrivateVariables (MonoClass* klass);
struct BackupState;
YAMLNode* ConvertBackupToYAML (BackupState& binary);

enum { kClassSerializationDepthLimit  = 7 };


struct MonoPPtr : PPtr<Object>
{
	MonoPPtr () { m_Buffer = NULL; m_Class = SCRIPTING_NULL; } 
	
	char* m_Buffer;
	ScriptingTypePtr m_Class;
};

struct TransferScriptInstance;

template<class TransferFunction>
void TransferScriptData (TransferScriptInstance& info, TransferFunction& transfer);

struct TransferScriptInstance
{
	inline static const char* GetTypeString ()	{ return "Generic Mono";  } 
	inline static bool IsAnimationChannel ()	{ return false; } 
	inline static bool MightContainPPtr ()	{ return true; }
	inline static bool AllowTransferOptimization ()	{ return false; }
	
	template<class TransferFunction> inline
	void Transfer (TransferFunction& transfer)
	{
		TransferScriptData (*this, transfer);
	}
	
	TransferScriptInstance()
	{
		depthCounter = -9999;
	}
	
	ScriptingObjectPtr instance;
	ScriptingTypePtr klass;
	bool transferPrivate;
	const CommonScriptingClasses* commonClasses;
	int depthCounter;
};

ScriptingObjectPtr TransferPPtrToMonoObjectUnChecked(int instanceID, bool threadedLoading);
ScriptingObjectPtr TransferPPtrToMonoObject(int instanceID, ScriptingClassPtr klass, int classID, ScriptingFieldPtr field, ScriptingObjectPtr parentInstance, bool threadedLoading);
