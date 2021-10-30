#ifndef SHADERSUPPORTSCRIPTS_H
#define SHADERSUPPORTSCRIPTS_H

#include "Runtime/Scripting/TextAsset.h"
#include <string>


// Shader include files (mainly exist so that a different icon can be used on them)
class CGProgram : public TextAsset {
public:
	REGISTER_DERIVED_CLASS (CGProgram, TextAsset)
	
	CGProgram (MemLabelId label, ObjectCreationMode mode);
	
	virtual const UnityStr& GetScriptClassName() const { return m_CGProgramName; }
	
	void SetCGProgram( const std::string& str );
	const UnityStr& GetCGProgram () const { return m_CGProgram; }
	
private:
	// Placeholder string used for the original CG program - we only use this at importing
	UnityStr m_CGProgram;
	UnityStr m_CGProgramName;
};

#endif
