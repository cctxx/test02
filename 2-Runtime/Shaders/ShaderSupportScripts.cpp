#include "UnityPrefix.h"
#include "ShaderSupportScripts.h"
#include "Runtime/Utilities/PathNameUtility.h"

IMPLEMENT_CLASS (CGProgram)

CGProgram::CGProgram (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{}

CGProgram::~CGProgram ()
{
}

void CGProgram::SetCGProgram( const std::string& str )
{
	m_CGProgram = str;
	m_CGProgramName = GetLastPathNameComponent(m_CGProgram);
}
