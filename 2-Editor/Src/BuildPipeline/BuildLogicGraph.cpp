#ifdef UNITY_LOGIC_GRAPH
static void BuildLogicGraphsDll()
{
	if (!IsFileCreated(kLogicGraphCodeFile))
	{
		int index = GetMonoManager().GetAssemblyIndexFromAssemblyName(GetGraphsDllName());
		
		// Don't include UnityEditor.Graphs.LogicGraph.dll if there are no logic graphs in the project.
		// Will remove dll if LogicGraph is only referenced by user code or through reflection.
		if (index != -1)
			GetMonoManager().SetAssemblyName(index, "");
		
		return;
	}
	
	MonoException* exception = NULL;
	void* params[] = { MonoStringNew(kLogicGraphCodeFile), MonoStringNew(kBuildLogicGraphDllFile) };
	
	MonoObject* res = CallStaticMonoMethodFromNamespace (kLogicGraphEditorNamespace, "Compiler", "BuildAllSceneGraphs", params, &exception);
	
	if (exception || !MonoObjectToBool(res))
		throw string("Building logic graphs failed.");
	
	RegisterOneCustomDll(kBuildLogicGraphDllFile);
}

static void GenerateSceneLogicGraphs(int generateCodeFile, int setupForSerialization)
{
	MonoException* exception = NULL;
	void* params[] = { MonoStringNew(kLogicGraphCodeFile), &generateCodeFile, &setupForSerialization};
	
	CallStaticMonoMethodFromNamespace (kLogicGraphEditorNamespace, "Compiler", "GenerateReleaseCodeAndSerializationDummiesForAllSceneGraphs", params, &exception);
	
	if (exception)
		throw string("Generating scene logic graphs failed.");
}
#endif
