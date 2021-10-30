#include "UnityPrefix.h"
#include "MonoCompile.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Mono/MonoScript.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Threads/Thread.h"
#include "Configuration/UnityConfigureVersion.h"
#include "Editor/Src/BuildPipeline/BuildTargetPlatformSpecific.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Misc/PlayerSettings.h"
#include "MonoImporter.h"
#include "Runtime/Scripting/Backend/ScriptingMethodRegistry.h"
#include "Runtime/Scripting/ScriptingManager.h"

using namespace std;

// Wrapper around ScriptCompiler objects
struct ActiveCompilerTask
{
	MonoIsland* island;
	guint32 instance;
	ScriptingMethodPtr pollMethod;
	CompileFinishedCallback* callback;
	int options;
	bool buildingForEditor;
	ActiveCompilerTask (const MonoIsland& island, CompileFinishedCallback* callback, int options, bool buildingForEditor);
	~ActiveCompilerTask();
	bool IsValid() { return instance != 0; }
	bool Poll();
	bool GetErrors(MonoCompileErrors& errors);
	void CleanupManaged();
	void WaitForCompletion();
};


MonoObject* CreateCompilerInstance(MonoIsland& island, bool buildingForEditor) {
	ManagedMonoIsland monoIsland = island.CreateManagedMonoIsland();

	ScriptingInvocation invocation("UnityEditor.Scripting","ScriptCompilers","CreateCompilerInstance");
	invocation.AddStruct(&monoIsland);
	invocation.AddBoolean(buildingForEditor);
	invocation.AddEnum(island.targetPlatform);
	return invocation.Invoke();
}

#if MONO_2_10 || MONO_2_12
const std::string kMonoFramework = "MonoBleedingEdge";
#else
const std::string kMonoFramework = "Mono";
#endif

#if UNITY_OSX
std::string kMonoDistroFolder = AppendPathName ("Frameworks", kMonoFramework);
#else
std::string kMonoDistroFolder = kMonoFramework;
#endif

string GetMonoBinDirectory (BuildTargetPlatform targetPlatform)
{
	///@TODO: This should get cleaned up once we have a single version of mono across the board
	if (
		targetPlatform == kBuildXBOX360 ||
		targetPlatform == kBuildPS3 ||
		targetPlatform == kBuildWii) 
		return GetMonoLibDirectory(targetPlatform);
	else
	{
		std::string classlib = kMonoDistroFolder;
		classlib += "/bin";
		return AppendPathName (GetApplicationContentsPath (), classlib);
	}
}


ActiveCompilerTask::ActiveCompilerTask(const MonoIsland& i_Island, CompileFinishedCallback* i_Callback, int i_Options, bool i_buildingForEditor) : 
	island(new MonoIsland(i_Island)), instance(0), callback(i_Callback), options(i_Options), buildingForEditor(i_buildingForEditor)
{
	vector<string> i_Defines = i_Island.defines;
	
	// Construct
	MonoObject* compiler = CreateCompilerInstance(*island, buildingForEditor);

	if( !compiler )
	{
		ErrorString("Failed to create compiler instance");
		return;
	}
	MonoClass* klass= mono_object_get_class(compiler);
	
	ScriptingMethodPtr method = GetScriptingMethodRegistry().GetMethod(klass, "BeginCompiling");
	if (!method)
		return;

	pollMethod = GetScriptingMethodRegistry().GetMethod(klass, "Poll");

	ScriptingInvocation invocation(method);
	MonoException* exception = NULL;
	invocation.logException=false;
	invocation.object = compiler;
	invocation.Invoke(&exception);

	if(exception != NULL)
	{
		Scripting::LogException(exception, NULL, "Could not start compilation");
		return;
	}
	
	instance=mono_gchandle_new(compiler, false); // Store the compiler object for later use
}


bool ActiveCompilerTask::Poll()
{
	MonoException* exception=NULL;
	if (! IsValid() ) return true;

	if ( ! mono_gchandle_is_in_domain(instance, mono_domain_get()) ) {
		AssertString("Domain mismatch error!");
		return true;
	}

	if (pollMethod)
	{
		MonoObject* compiler = mono_gchandle_get_target(instance);
		MonoObject* result = mono_runtime_invoke_profiled(pollMethod->monoMethod, compiler, NULL, &exception);
		if(exception != NULL) {
			Scripting::LogException(exception, NULL, "Failed when polling compilation process");
		}
		else {
			return (bool)*((unsigned char *) mono_object_unbox(result));
		}
	}
	return true;
}

// This struct reflects the mono side of MonoCompileError
// Used for translating from mono to c++
struct MonoMonoCompileError
{
	MonoString* message;
	MonoString* file;
	int line;
	int column;
	int type;
};


bool ConvertManagedCompileErrorsToNative(MonoArray* messages,MonoCompileErrors& errors)
{
	bool success = true;
	int messageCount = mono_array_length(messages);
	for(int i = 0; i < messageCount ; i++ )
	{
		MonoMonoCompileError message = GetMonoArrayElement<MonoMonoCompileError> (messages, i);
		MonoCompileError error;
		error.error = MonoStringToCpp(message.message);
		error.file = MonoStringToCpp(message.file);
		error.line = message.line;
		error.column = message.column;
		error.type = message.type;
		if(error.type == MonoCompileError::kError)
			success=false;
		errors.push_back(error);
	}
	
	return success;
}

bool ActiveCompilerTask::GetErrors(MonoCompileErrors& errors)
{
	MonoObject* compiler = mono_gchandle_get_target(instance);
	if(!compiler)
	{
		MonoCompileError error;
		error.error="Invalid compiler object found in task list";
		error.type = MonoCompileError::kError;
		errors.push_back(error);
		return false;
	}
	MonoClass* klass= mono_object_get_class(compiler);
	MonoException* exception=NULL;
	ScriptingMethodPtr method = GetScriptingMethodRegistry().GetMethod(klass, "GetCompilerMessages");

	AssertIf( ! mono_gchandle_is_in_domain(instance, mono_domain_get()) );

	if( method ) 
	{
		MonoArray* messages = (MonoArray*) mono_runtime_invoke_profiled(method->monoMethod, compiler, NULL, &exception);
		if(exception != NULL)
		{
			Scripting::LogException(exception, NULL, "Could not fetch error messages from compiler");
		}
		else
		{
			return ConvertManagedCompileErrorsToNative(messages,errors);
		}
	}
	return false;
}

void ActiveCompilerTask::CleanupManaged()
{
	if(! IsValid() ) return;

	MonoObject* compiler = mono_gchandle_get_target(instance);
	if (!compiler)
		return;

	
	if( ! mono_gchandle_is_in_domain(instance, mono_domain_get()) )
	{
		AssertString("Domain mismatch error!");
		return;
	}
	
	MonoMethod* disposeMethod = mono_object_get_virtual_method(compiler, GetScriptingManager().GetCommonClasses().IDisposable_Dispose->monoMethod);

	if (!disposeMethod)
	{
		AssertString("Unable to find Dispose method on compiler instance");
		return;
	}

	
	MonoException* exception;
	mono_runtime_invoke_profiled(disposeMethod, compiler, NULL, &exception);
	if(exception != NULL)
		Scripting::LogException(exception, NULL, "Failed when disposing compilation process");
	
	// Terminate it here
	mono_gchandle_free(instance);
	instance = 0;
}

ActiveCompilerTask::~ActiveCompilerTask()
{
	delete island;
	island = NULL;

	CleanupManaged();
}

typedef map<int,ActiveCompilerTask*> ActiveCompilerTasks;
ActiveCompilerTasks gActiveTasks;

static void CheckForDuplicates (const set<string>& files, MonoCompileErrors* errors);

void StopAllCompilation()
{
	while (!gActiveTasks.empty())
		StopCompilation(gActiveTasks.begin()->first);
}

void StopCompilation (int index)
{
	ActiveCompilerTasks::iterator found = gActiveTasks.find(index);
	if(found != gActiveTasks.end() )
	{
		ActiveCompilerTask* task=found->second;
		gActiveTasks.erase (found);
		delete task;
	}
}

bool UpdateMonoCompileTasks(bool waitForCompletion)
{
	if (gActiveTasks.empty ())
		return false;
	
	list<ActiveCompilerTask*> done;
	bool completedAnyTasks = false;
	for (ActiveCompilerTasks::iterator i=gActiveTasks.begin ();i != gActiveTasks.end ();i++)
	{
		ActiveCompilerTask* task=i->second;
		
		if (waitForCompletion)
		{
			if(! task->IsValid() )
				continue;
			
			// Do the waiting here...
			while( ! task->Poll() )
				Thread::Sleep( 0.1 );
		}
		
		if (task->Poll())
			done.push_back(task);
	}

	// Clean up finished tasks
	for (list<ActiveCompilerTask*>::iterator i=done.begin(); i != done.end(); i++)
	{
		ActiveCompilerTask* task=*i;
		
		// Check if the task has been stolen by a StopCompiler in between
		bool contains = false;
		for (ActiveCompilerTasks::iterator j=gActiveTasks.begin ();j != gActiveTasks.end ();j++)
		{
			if (j->second == task)
				contains = true;
		}
		if (!contains)
			continue;
		
		AssertIf(task->island == NULL);
		gActiveTasks.erase (task->island->assemblyIndex);
		
		MonoCompileErrors tempErrors;
		bool success = task->GetErrors(tempErrors);
		task->CleanupManaged();
		if(task->callback)
		{	
			task->callback(success, task->options, task->buildingForEditor, *task->island, tempErrors);
			completedAnyTasks = true;
		}
		else
			AssertString("Something's wrong here. Compile task callback is NULL");
		delete task;
	}
	
	return completedAnyTasks;
}

bool IsCompiling (int assemblyIndex)
{
	for (ActiveCompilerTasks::iterator i=gActiveTasks.begin ();i != gActiveTasks.end ();i++)
	{
		ActiveCompilerTask* task=i->second;
		
		if (task->island->assemblyIndex == assemblyIndex)
			return true;
	}
	
	return false;
}


bool IsCompiling ()
{
	return !gActiveTasks.empty ();
}

const char* kLaunchFailed = "Compilation failed because the compiler couldn't be executed!";


/// We don't want duplicate names in one dll that will just result in weird compile errors!
static void CheckForDuplicates (const set<string>& files, MonoCompileErrors* errors)
{
	set<string> duplicates;
	for (set<string>::const_iterator i=files.begin();i != files.end();i++)
	{
		string name = GetLastPathNameComponent(*i);
		if (duplicates.count(name))
		{
			for (set<string>::const_iterator j=files.begin();j != files.end();j++)
			{
				if (GetLastPathNameComponent(*j) == name)
				{
					MonoCompileError err;
					err.type=MonoCompileError::kError;
					err.line=err.column=1;
					err.file=*j;
					err.error=Format("Scripts named '%s' exist in multiple locations (%s). Please rename one of the scripts to a unique name.", name.c_str(), j->c_str());
					errors->push_back(err);
				}
			}
		}
		else
		{
			duplicates.insert(name);
		}
	}
}

void CompileFiles (CompileFinishedCallback* callback, bool buildingForEditor, const MonoIsland& island, int options)
{
	StopCompilation (island.assemblyIndex);
	
	// Check for duplicate scripts (Unityscript gives weird errors in that case so we just fix it here.)
	if (island.language.languageName == "UnityScript")
	{
		MonoCompileErrors errors;
		CheckForDuplicates(island.paths, &errors);
		if(! errors.empty() )
		{
			callback (false, options, buildingForEditor, island, errors);
			return;
		}
	}

	ActiveCompilerTask* compileTask= new ActiveCompilerTask(island, callback, options,buildingForEditor);

	if(! compileTask->IsValid() )
	{		
		MonoCompileError error;
		error.error = kLaunchFailed;
		MonoCompileErrors tempErrors; 
		tempErrors.push_back (error);
		callback (false, options, buildingForEditor, island, tempErrors);
		delete compileTask;
		return;
	}
	
	gActiveTasks[island.assemblyIndex]=compileTask;
}

