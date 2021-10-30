#ifndef OBJECTDEFINES_H
#define OBJECTDEFINES_H

#include "Runtime/Allocator/BaseAllocator.h"
#include "Configuration/UnityConfigure.h"
#include "Runtime/Allocator/MemoryMacros.h"

#define NEW_OBJECT(class_) reinterpret_cast<class_*> (::Object::AllocateAndAssignInstanceID( UNITY_NEW_AS_ROOT ( class_(kMemBaseObject, kCreateObjectDefault), kMemBaseObject, NULL, NULL) ))
#define NEW_OBJECT_USING_MEMLABEL(class_, memlabel_) reinterpret_cast<class_*> (::Object::AllocateAndAssignInstanceID( UNITY_NEW_AS_ROOT ( class_(memlabel_, kCreateObjectDefault), memlabel_, NULL, NULL) ))
#define NEW_OBJECT_MAIN_THREAD(class_) reinterpret_cast<class_*> (::Object::AllocateAndAssignInstanceID(UNITY_NEW_AS_ROOT ( class_(kMemBaseObject, kCreateObjectDefault), kMemBaseObject, NULL, NULL) ))
#define NEW_OBJECT_FULL(class_,param) UNITY_NEW_AS_ROOT( class_(kMemBaseObject, param), kMemBaseObject, NULL, NULL)

#define NEW_OBJECT_RESET_AND_AWAKE(class_) ResetAndAwake (NEW_OBJECT (class_))

// Every non-abstract class that is derived from object has to place this inside the class Declaration
// (REGISTER_DERIVED_CLASS (Foo, Object))
#define		REGISTER_DERIVED_CLASS(x, d) \
public: \
	virtual int GetClassIDVirtualInternal () const; \
	static int GetClassIDStatic ()				{ return ClassID (x); } \
	static const char* GetClassStringStatic (){ return #x; }			\
	static const char* GetPPtrTypeString ()	 { return "PPtr<"#x">"; }	\
	static bool IsAbstract ()						{ return false; }\
	static Object* PRODUCE (MemLabelId label, ObjectCreationMode mode)	{ return UNITY_NEW_AS_ROOT( x (label, mode), label, NULL, NULL); } \
	typedef d Super; \
	static void RegisterClass (); \
	protected: \
	~x (); \
	public:


// Every abstract class that is derived from object has to place this inside the class Declaration
// (REGISTER_DERIVED_ABSTRACT_CLASS (Foo, Object))
#define		REGISTER_DERIVED_ABSTRACT_CLASS(x, d) \
	public: \
	virtual int GetClassIDVirtualInternal () const; \
	static int GetClassIDStatic ()				{ return ClassID (x); } \
	static Object* PRODUCE (MemLabelId, ObjectCreationMode)						{ AssertString ("Can't produce abstract class"); return NULL; } \
	static bool IsAbstract ()						{ return true; }\
	static const char* GetClassStringStatic (){ return #x; }				\
	static const char* GetPPtrTypeString ()	 { return "PPtr<"#x">"; }	\
	typedef d Super; \
	static void RegisterClass (); \
	protected: \
	~x (); \
	public:

typedef void RegisterClassCallback ();
void EXPORT_COREMODULE RegisterInitializeClassCallback (int classID,
														RegisterClassCallback* registerClass,
														RegisterClassCallback* initClass,
														RegisterClassCallback* postInitClass,
														RegisterClassCallback* cleanupClass);


#if DEBUGMODE
#define VERIFY_OBJECT_IS_REGISTERED(x)  \
struct IMPLEMENT_CONSTRUCTOR_CLASS##x { IMPLEMENT_CONSTRUCTOR_CLASS##x () {  AddVerifyClassRegistration (ClassID (x)); } }; \
IMPLEMENT_CONSTRUCTOR_CLASS##x gVAR_CONSTRUCTOR_CLASS##x;

#else
#define VERIFY_OBJECT_IS_REGISTERED(x)
#endif

#define IMPLEMENT_CLASS_FULL(x, INIT, POSTINIT, CLEANUP)  \
VERIFY_OBJECT_IS_REGISTERED(x) \
void RegisterClass_##x () { RegisterInitializeClassCallback (ClassID (x), x::RegisterClass, INIT, POSTINIT, CLEANUP);  }  \
void x::RegisterClass () 					\
{																	\
	Assert(!Super::IsSealedClass ()); \
	if (Object::ClassIDToRTTI (Super::GetClassIDStatic ()) == NULL)	\
		Super::RegisterClass ();	\
	Object::RegisterClass (ClassID (x), Super::GetClassIDStatic (), #x, sizeof (x), PRODUCE, IsAbstract ());		\
} \
int x::GetClassIDVirtualInternal () const				{ return ClassID (x); }

#define IMPLEMENT_CLASS(x)  IMPLEMENT_CLASS_FULL(x, NULL, NULL, NULL)
#define IMPLEMENT_CLASS_HAS_INIT(x)  IMPLEMENT_CLASS_FULL(x, x::InitializeClass, NULL, x::CleanupClass)
#define IMPLEMENT_CLASS_HAS_POSTINIT(x)  IMPLEMENT_CLASS_FULL(x, x::InitializeClass, x::PostInitializeClass, x::CleanupClass)
#define IMPLEMENT_CLASS_INIT_ONLY(x)  IMPLEMENT_CLASS_FULL(x, x::InitializeClass, NULL, NULL)

// Should be placed in every serializable object derived class (DECLARE_OBJECT_SERIALIZE (Transform))
#if UNITY_EDITOR
	#define DECLARE_OBJECT_SERIALIZE(x) \
	static const char* GetTypeString ()	 { return GetClassStringStatic(); }				\
	static bool IsAnimationChannel ()	 { return false; }				\
	static bool MightContainPPtr ()	 { return true; }				\
	static bool AllowTransferOptimization () { return false; } \
	template<class TransferFunction> void Transfer (TransferFunction& transfer); \
	virtual void VirtualRedirectTransfer (ProxyTransfer& transfer);		\
	virtual void VirtualRedirectTransfer (SafeBinaryRead& transfer);		\
	virtual void VirtualRedirectTransfer (StreamedBinaryWrite<false>& transfer);\
	virtual void VirtualRedirectTransfer (StreamedBinaryWrite<true>& transfer);\
	virtual void VirtualRedirectTransfer (StreamedBinaryRead<false>& transfer);	\
	virtual void VirtualRedirectTransfer (StreamedBinaryRead<true>& transfer);	\
	virtual void VirtualRedirectTransfer (RemapPPtrTransfer& transfer); \
	virtual void VirtualRedirectTransfer (YAMLRead& transfer); \
	virtual void VirtualRedirectTransfer (YAMLWrite& transfer);
#elif SUPPORT_SERIALIZED_TYPETREES
	#define DECLARE_OBJECT_SERIALIZE(x) \
	static const char* GetTypeString ()	 { return GetClassStringStatic(); }				\
	static bool IsAnimationChannel ()	 { return false; }				\
	static bool MightContainPPtr ()	 { return true; }				\
	static bool AllowTransferOptimization () { return false; } \
	template<class TransferFunction> void Transfer (TransferFunction& transfer); \
	virtual void VirtualRedirectTransfer (ProxyTransfer& transfer);		\
	virtual void VirtualRedirectTransfer (SafeBinaryRead& transfer);		\
	virtual void VirtualRedirectTransfer (StreamedBinaryRead<false>& transfer);	\
	virtual void VirtualRedirectTransfer (StreamedBinaryRead<true>& transfer);	\
	virtual void VirtualRedirectTransfer (StreamedBinaryWrite<false>& transfer); \
	virtual void VirtualRedirectTransfer (RemapPPtrTransfer& transfer);
#else
	#define DECLARE_OBJECT_SERIALIZE(x) \
	static const char* GetTypeString ()	 { return GetClassStringStatic(); }				\
	static bool IsAnimationChannel ()	 { return false; }				\
	static bool MightContainPPtr ()	 { return true; }				\
	static bool AllowTransferOptimization () { return false; } \
	template<class TransferFunction> void Transfer (TransferFunction& transfer); \
	virtual void VirtualRedirectTransfer (ProxyTransfer& transfer); \
	virtual void VirtualRedirectTransfer (StreamedBinaryRead<false>& transfer);	\
	virtual void VirtualRedirectTransfer (StreamedBinaryWrite<false>& transfer); \
	virtual void VirtualRedirectTransfer (RemapPPtrTransfer& transfer);
#endif

// Has to be placed in the cpp file of a serializable class (IMPLEMENT_OBJECT_SERIALIZE (Transform))
// you also have to #include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"

#if UNITY_EDITOR // Editor needs to support swapped endian writing, player doesnt.

#define INSTANTIATE_TEMPLATE_TRANSFER_WITH_DECL(x, decl)	\
template decl void x::Transfer(ProxyTransfer& transfer); \
template decl void x::Transfer(SafeBinaryRead& transfer); \
template decl void x::Transfer(StreamedBinaryRead<false>& transfer); \
template decl void x::Transfer(StreamedBinaryRead<true>& transfer); \
template decl void x::Transfer(StreamedBinaryWrite<false>& transfer); \
template decl void x::Transfer(StreamedBinaryWrite<true>& transfer); \
template decl void x::Transfer(RemapPPtrTransfer& transfer); \
template decl void x::Transfer(YAMLRead& transfer); \
template decl void x::Transfer(YAMLWrite& transfer);

#define IMPLEMENT_OBJECT_SERIALIZE(x)	\
void x::VirtualRedirectTransfer (ProxyTransfer& transfer)                   { transfer.Transfer (*this, "Base"); }	\
void x::VirtualRedirectTransfer (SafeBinaryRead& transfer)                  { SET_ALLOC_OWNER(this); transfer.Transfer (*this, "Base"); }	\
void x::VirtualRedirectTransfer (StreamedBinaryRead<false>& transfer)       { SET_ALLOC_OWNER(this); transfer.Transfer (*this, "Base"); }	\
void x::VirtualRedirectTransfer (StreamedBinaryRead<true>& transfer)        { SET_ALLOC_OWNER(this); transfer.Transfer (*this, "Base"); }	\
void x::VirtualRedirectTransfer (RemapPPtrTransfer& transfer)               { transfer.Transfer (*this, "Base"); }  \
void x::VirtualRedirectTransfer (StreamedBinaryWrite<false>& transfer)      { transfer.Transfer (*this, "Base"); }	\
void x::VirtualRedirectTransfer (StreamedBinaryWrite<true>& transfer)       { transfer.Transfer (*this, "Base"); }	\
void x::VirtualRedirectTransfer (YAMLRead& transfer)						{ SET_ALLOC_OWNER(this); transfer.Transfer (*this, "Base"); }	\
void x::VirtualRedirectTransfer (YAMLWrite& transfer)						{ transfer.Transfer (*this, "Base"); }	\

#elif SUPPORT_SERIALIZED_TYPETREES
#define INSTANTIATE_TEMPLATE_TRANSFER_WITH_DECL(x, decl)	\
template decl void x::Transfer(ProxyTransfer& transfer); \
template decl void x::Transfer(SafeBinaryRead& transfer); \
template decl void x::Transfer(StreamedBinaryRead<false>& transfer); \
template decl void x::Transfer(StreamedBinaryRead<true>& transfer); \
template decl void x::Transfer(StreamedBinaryWrite<false>& transfer); \
template decl void x::Transfer(RemapPPtrTransfer& transfer);

	#define IMPLEMENT_OBJECT_SERIALIZE(x)	\
	void x::VirtualRedirectTransfer (ProxyTransfer& transfer)                   { transfer.Transfer (*this, "Base"); }	\
	void x::VirtualRedirectTransfer (SafeBinaryRead& transfer)                  { SET_ALLOC_OWNER(this); transfer.Transfer (*this, "Base"); }	\
	void x::VirtualRedirectTransfer (StreamedBinaryRead<false>& transfer)       { SET_ALLOC_OWNER(this); transfer.Transfer (*this, "Base"); }	\
	void x::VirtualRedirectTransfer (StreamedBinaryRead<true>& transfer)       { SET_ALLOC_OWNER(this); transfer.Transfer (*this, "Base"); }	\
	void x::VirtualRedirectTransfer (StreamedBinaryWrite<false>& transfer)       { transfer.Transfer (*this, "Base"); }	\
	void x::VirtualRedirectTransfer (RemapPPtrTransfer& transfer)               { transfer.Transfer (*this, "Base"); }	\

#else
#define INSTANTIATE_TEMPLATE_TRANSFER_WITH_DECL(x, decl)	\
template decl void x::Transfer(ProxyTransfer& transfer); \
template decl void x::Transfer(StreamedBinaryRead<false>& transfer); \
template decl void x::Transfer(StreamedBinaryWrite<false>& transfer); \
template decl void x::Transfer(RemapPPtrTransfer& transfer);

	#define IMPLEMENT_OBJECT_SERIALIZE(x)	\
	void x::VirtualRedirectTransfer (ProxyTransfer& transfer)                   { transfer.Transfer (*this, "Base"); }	\
	void x::VirtualRedirectTransfer (StreamedBinaryRead<false>& transfer)       { SET_ALLOC_OWNER(this); transfer.Transfer (*this, "Base"); }	\
	void x::VirtualRedirectTransfer (StreamedBinaryWrite<false>& transfer)       { transfer.Transfer (*this, "Base"); }	\
	void x::VirtualRedirectTransfer (RemapPPtrTransfer& transfer)               { transfer.Transfer (*this, "Base"); }  \

#endif

#if UNITY_WIN
#define EXPORTDLL __declspec(dllexport)
#elif UNITY_OSX
#define EXPORTDLL __attribute__((visibility("default")))
#else
#define EXPORTDLL
#endif

#define INSTANTIATE_TEMPLATE_TRANSFER(x) INSTANTIATE_TEMPLATE_TRANSFER_WITH_DECL(x, )
#define INSTANTIATE_TEMPLATE_TRANSFER_EXPORTED(x) INSTANTIATE_TEMPLATE_TRANSFER_WITH_DECL(x, EXPORTDLL)


// Use this to make a Generic C++ GET/SET function: GET_SET (float, Velocity, m_Velocity)
// 	Implements GetVelocity, SetVelocity
#define GET_SET(TYPE,PROP_NAME,VAR_NAME)	void Set##PROP_NAME (TYPE val) { VAR_NAME = val; }	const TYPE Get##PROP_NAME () const {return (const TYPE)VAR_NAME; }
#define GET_SET_DIRTY(TYPE,PROP_NAME,VAR_NAME)	void Set##PROP_NAME (TYPE val) { VAR_NAME = val; SetDirty(); }	const TYPE Get##PROP_NAME () const {return (const TYPE)VAR_NAME; }
#define GET_SET_COMPARE_DIRTY(TYPE,PROP_NAME,VAR_NAME)	void Set##PROP_NAME (TYPE val) { if ((TYPE)VAR_NAME == val) return; VAR_NAME = val; SetDirty(); }	const TYPE Get##PROP_NAME () const {return (const TYPE)VAR_NAME; }

#endif
