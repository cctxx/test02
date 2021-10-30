#include "UnityPrefix.h"
#include "BuildSerialization.h"
#include "Runtime/Camera/GraphicsSettings.h"
#include "Runtime/Shaders/ShaderNameRegistry.h"
#include "Runtime/Testing/Testing.h"

#if ENABLE_UNIT_TESTS

SUITE (BuildSerializationTests)
{
	TEST (IsAlwaysIncludedShaderOrDependency_NullEntry_DoesNotCrash)
	{
		// Arrange.
		GetGraphicsSettings ().AddAlwaysIncludedShader (PPtr<Shader> ());
		Shader* specularShader = GetScriptMapper ().FindShader ("Specular");

		// Act.
		IsAlwaysIncludedShaderOrDependency (specularShader->GetInstanceID ());

		// Cleanup.
		GetGraphicsSettings ().SetDefaultAlwaysIncludedShaders ();
	}
}

#endif
