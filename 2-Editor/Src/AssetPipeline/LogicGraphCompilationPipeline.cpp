#if UNITY_LOGIC_GRAPH
#include "UnityPrefix.h"
#include "LogicGraphCompilationPipeline.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Mono/MonoUtility.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Editor/Src/Graphs/GraphUtils.h"

using std::vector;

const char* kLogicGraphDllDirectory = "Library/CodeGeneratingGraphAssemblies";
static MonoClass* LoadLogicGraph (const std::string& dllPath, const std::string& klassName);

#define GRAPH_ERROR_IDENTIFIER 0xFFEEDDBB
bool g_hasGraphErrors = false;
bool g_logStickyErrors = false;

void ClearLogicGraphCompilationDirectory ()
{
	DeleteFileOrDirectory(kLogicGraphDllDirectory);
	CreateDirectoryRecursive(kLogicGraphDllDirectory);
}

static string GetGraphHash(MonoBehaviour* graph)
{
	void* params[] = { Scripting::ScriptingWrapperFor(graph) };
	return MonoStringToCppChecked(CallStaticMonoMethodFromNamespace(kLogicGraphEditorNamespace, "GraphHash", "GetHash", params));
}

static void DeleteAllOldGraphDlls(string graphName, string currentGraphHash)
{
	std::set<string> allFiles;
	AssertIf(!GetFolderContentsAtPath(kLogicGraphDllDirectory, allFiles));

	for (std::set<std::string>::const_iterator i = allFiles.begin(); i != allFiles.end(); ++i)
		if (i->find(graphName) != string::npos && i->find(currentGraphHash) == string::npos)
			DeleteFileOrDirectory(*i);
}

// Returns dll path if successful
string CompileLogicGraph (MonoScript& script, bool logStickyErrors)
{
	MonoBehaviour* logicGraph = script.GetEditorGraphData();
	if (logicGraph == NULL || logicGraph->GetInstance() == NULL)
		return "";

	string hash = GetGraphHash(logicGraph);
	string dllPath = AppendPathName(kLogicGraphDllDirectory, logicGraph->GetName()) + "_" + hash + ".dll";

	DeleteAllOldGraphDlls(logicGraph->GetName(), hash);

	// don't recompile graph if hash stayed the same
	if (!IsPathCreated(dllPath))
	{
		void* params[] = { logicGraph->GetInstance(), scripting_string_new(dllPath) };

		g_logStickyErrors = logStickyErrors;
		// This assumes all the errors during graph generation and compilation are logged on mono side
		bool success = MonoObjectToBool(CallStaticMonoMethodFromNamespace(kLogicGraphEditorNamespace, "Compiler", "CompileGraph", params));
		g_logStickyErrors = false;

		if (!success)
		{
			g_hasGraphErrors = true;
			return "";
		}
	}

	return dllPath;
}

static MonoClass* LoadLogicGraph (const std::string& dllPath, const std::string& klassName)
{
	MonoAssembly* assembly = mono_domain_assembly_open(mono_domain_get(), dllPath.c_str());
	if (assembly == NULL)
		return NULL;
	
	MonoImage* image = mono_assembly_get_image(assembly);
	if (image == NULL)
		return NULL;

	MonoClass* klass = mono_class_from_name (image, "", klassName.c_str());
	if (klass == NULL)
		WarningString("Failed to load graph: " + klassName);

	return klass;
}

void InitializeMonoBehaviourWithSerializationDummy(int monoBehaviourID, MonoType* type, MonoObject*instance)
{
	MonoClass* klass = mono_class_from_mono_type(type);
	if (klass == NULL)
		throw string("Logic graphs mono type is not a class.");

	MonoBehaviour* behaviour = dynamic_instanceID_cast<MonoBehaviour*>(monoBehaviourID);
	if (behaviour == NULL)
		throw string("Logic graphs mono behaviour is not valid.");

	MonoScript* script = behaviour->GetScript();
	if (script == NULL)
		throw string("Logic graphs mono script is not valid.");

	script->Rebuild(klass);
	behaviour->RebuildMonoInstance(instance);
}

void CompileAllGraphsInTheScene(bool logStickyErrors)
{
	ClearGraphCompilationErrors();

	vector<int> allGraphs = AllGraphsInScene();

	for (vector<int>::iterator i = allGraphs.begin(); i != allGraphs.end(); i++)
	{
		MonoBehaviour* behaviour = dynamic_instanceID_cast<MonoBehaviour*>(*i);
		MonoScript& script = *behaviour->GetScript();
		CompileLogicGraph(script, logStickyErrors);
	}
}

MonoClass* CompileAndLoadLogicGraph(MonoScript& script, bool logStickyErrors)
{
	string dllPath = CompileLogicGraph (script, logStickyErrors);
	if (dllPath.empty())
		return NULL;
	return LoadLogicGraph(dllPath, script.GetEditorGraphData()->GetName());
}

void LogGraphCompilationError(MonoBehaviour &graph, string error)
{
	int mask = kLog | kGraphCompileError | kDontExtractStacktrace | kMayIgnoreLineNumber;

	if (g_logStickyErrors)
		mask |= kStickyError;

	DebugStringToFile (error, 0, graph.GetName(), 
		0, mask, graph.GetScript() ? graph.GetInstanceID() : 0, GRAPH_ERROR_IDENTIFIER);
}

bool HasGraphCompilationErrors()
{
	return g_hasGraphErrors;	
}

void ClearGraphCompilationErrors()
{
	RemoveErrorWithIdentifierFromConsole(GRAPH_ERROR_IDENTIFIER);
	g_hasGraphErrors = false;
}
#endif
