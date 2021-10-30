#include "UnityPrefix.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/File.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"

void PrepareEditorForIntegrationTestReload ()
{
	printf_console ("**** ---- BEGIN INTEGRATION TEST CLEANUP ------\n");
	
	ErrorIf(!DeleteFileOrDirectory("Assets"));
	ErrorIf(!CreateDirectory("Assets"));
	
	GetMonoManager().SetLogAssemblyReload(false);
	
	int oldFlags = AssetInterface::Get().GetGlobalPostprocessFlags();
	AssetInterface::Get().SetGlobalPostprocessFlags(oldFlags | kForceSynchronousImport);
	AssetInterface::Get().Refresh(kForceSynchronousImport);
	AssetInterface::Get().SetGlobalPostprocessFlags(oldFlags);
	
	GetMonoManager().SetLogAssemblyReload(true);
	
	printf_console ("**** ---- END INTEGRATION TEST CLEANUP ------\n");
	
	LogString("CompletedRecycle");		
}


std::string ExecuteMacroScript (const std::string& eval)
{
	ScriptingInvocation invocation("UnityEditor.Macros", "MacroEvaluator", "Eval");
	invocation.AddString(eval.c_str());
	
	MonoException* exception = NULL;
	MonoObject* result = invocation.Invoke(&exception);
	if (exception != NULL)
		return "Exception";
	
	char* utf8 = mono_string_to_utf8((MonoString*)result);
	std::string str_result(utf8);
	g_free(utf8);
	return str_result;
}


std::string ExecuteMacroClientCommand (const std::string& eval)
{
	if (BeginsWith(eval, "_COMMAND_PrepareEditorForIntegrationTestReload"))
	{
		PrepareEditorForIntegrationTestReload();
		return std::string();
	}
	else if (BeginsWith(eval, "_COMMAND_RefreshAllowSynchronous"))
	{
		AssetInterface::Get().Refresh(kAllowForceSynchronousImport);
		return std::string();
	}
	else
		return ExecuteMacroScript(eval);
}
