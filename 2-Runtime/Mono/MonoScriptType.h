#ifndef MONOSCRIPTTYPE_H
#define MONOSCRIPTTYPE_H

enum MonoScriptType
{
	kScriptTypeMonoBehaviourDerived = 0,
	kScriptTypeScriptableObjectDerived = 1,
	kScriptTypeEditorScriptableObjectDerived = 2,

	kScriptTypeNotInitialized = -1,
	kScriptTypeNothingDerived = -2,
	kScriptTypeClassNotFound = -3,
	kScriptTypeClassIsAbstract = -4,
	kScriptTypeClassIsInterface = -5,
	kScriptTypeClassIsGeneric = -6,
	kScriptTypeScriptMissing = -7
};

#endif // !MONOSCRIPTTYPE_H
