#pragma once

class Shader;

// Shader replacement
struct ShaderReplaceData
{
	Shader*		replacementShader;
	int         replacementTagID;
	bool        replacementTagSet;
	
	
	ShaderReplaceData () { replacementShader = NULL; replacementTagID = 0; replacementTagSet = false; }
};