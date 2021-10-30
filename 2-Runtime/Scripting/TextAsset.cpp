#include "UnityPrefix.h"
#include "TextAsset.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#if ENABLE_MONO
#include "Runtime/Mono/MonoIncludes.h"
#endif


TextAsset::TextAsset(MemLabelId label, ObjectCreationMode mode)
	: Super(label, mode)
{
}

TextAsset::~TextAsset ()
{
}

template<class TransferFunction>
void TextAsset::Transfer (TransferFunction& transfer)
{
	Super::Transfer ( transfer);
	transfer.Transfer (m_Script, "m_Script", kHideInEditorMask);
	transfer.Transfer (m_PathName, "m_PathName", kHideInEditorMask);
}

void TextAsset::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad (awakeMode);
}

void TextAsset::SetScriptDontDirty (const ScriptString& script) {
	m_Script = script;
}

bool TextAsset::SetScript (const ScriptString& script) {
	return SetScript(script,false);
}

bool TextAsset::SetScript (const ScriptString& script, bool actuallyContainsBinaryData) 
{
	SET_ALLOC_OWNER(this);
	m_Script = script;

	#if ENABLE_MONO
	// Fallback. If mono doesn't accept the string, strip everything non-ASCII
	if(mono_string_new_wrapper(script.c_str()) == NULL && !actuallyContainsBinaryData)
	{
		m_Script.clear();
		
		for(int i=0; i<script.size(); i++)
		{
			if((unsigned char)script[i] < 0x7f)
				m_Script += script[i];
		}
	}
	#endif

	SetDirty ();
	return true;
}

const UnityStr& TextAsset::GetScriptClassName() const {
	static UnityStr sEmpty;
	return sEmpty;
}


IMPLEMENT_CLASS (TextAsset)
IMPLEMENT_OBJECT_SERIALIZE (TextAsset)
INSTANTIATE_TEMPLATE_TRANSFER (TextAsset)
