#include "UnityPrefix.h"
#include "SerializationWeaver.h"

#include "Runtime/Utilities/File.h"
#include "Runtime/Mono/MonoManager.h"
#include "Editor/Src/AssetPipeline/MonoCompilationPipeline.h"

static void DoWave(const string &assemblyPath, const string &destPath)
{
	ScriptingInvocation invocation("UnityEditor.Scripting.Serialization","Weaver","WeaveInto");
	invocation.AddString(const_cast<string&>(assemblyPath));
	invocation.AddString(const_cast<string&>(destPath));
	invocation.Invoke();
}

void WeaveSerializationCodeIntoAssemblies()
{
	for (int i=MonoManager::kScriptAssemblies;i<GetMonoManager().GetAssemblyCount();i++)
	{
		string assemblyPath = GetMonoManager ().GetAssemblyPath (i);
		if (!IsEditorOnlyAssembly (i) && IsFileCreated(assemblyPath))
		{
			DoWave(assemblyPath, assemblyPath);
		}
	}
}