#ifndef MONOTYPES_H
#define MONOTYPES_H

//TODO use mono headers directly, so we don't get burned when the struct definitions in this file
//go out of sync with mono's.
//this is not done yet, because it's tricky, as the mono headers define symbols that we also define in UnityFunctions.h,
//so we'd need to find some way to either remove those defines from the mono headers, or somehow mangle them.
#if ENABLE_MONO

struct MonoException;
struct MonoAssembly;
struct MonoObject;
struct MonoClassField;
struct MonoClass;
struct MonoDomain;
struct MonoImage;
struct MonoType;
struct MonoMethodSignature;
struct MonoArray;
struct MonoThread;
struct MonoVTable;
struct MonoProperty;
struct MonoReflectionAssembly;
struct MonoReflectionMethod;
struct MonoAppDomain;
struct MonoCustomAttrInfo;
struct MonoDl;
#if UNITY_STANDALONE || UNITY_EDITOR
struct MonoDlFallbackHandler;
#endif

#if UNITY_EDITOR
struct MonoMethodDesc;
#endif

typedef const void* gconstpointer;
typedef void* gpointer;
typedef int gboolean;
typedef unsigned int guint32;
typedef int gint32;
typedef unsigned long gulong;
#if UNITY_WII
	typedef signed long long gint64;
#else
	typedef long gint64;
#endif
typedef unsigned char   guchar;
typedef UInt16 gunichar2;
struct MonoString 
{
	void* monoObjectPart1;
	void* monoObjectPart2;
	gint32 length;
	gunichar2 firstCharacter;
};

struct MonoMethod {
	UInt16 flags;
	UInt16 iflags;
};

struct GPtrArray {
	gpointer *pdata;
	guint32 len;
};

typedef enum
{
	MONO_VERIFIER_MODE_OFF,
	MONO_VERIFIER_MODE_VALID,
	MONO_VERIFIER_MODE_VERIFIABLE,
	MONO_VERIFIER_MODE_STRICT
} MiniVerifierMode;

typedef enum {
	MONO_SECURITY_MODE_NONE,
	MONO_SECURITY_MODE_CORE_CLR,
	MONO_SECURITY_MODE_CAS,
	MONO_SECURITY_MODE_SMCS_HACK
} MonoSecurityMode;

typedef enum {
	MONO_TYPE_NAME_FORMAT_IL,
	MONO_TYPE_NAME_FORMAT_REFLECTION,
	MONO_TYPE_NAME_FORMAT_FULL_NAME,
	MONO_TYPE_NAME_FORMAT_ASSEMBLY_QUALIFIED
} MonoTypeNameFormat;

typedef struct {
        const char *name;
        const char *culture;
        const char *hash_value;
        const UInt8* public_key;
        // string of 16 hex chars + 1 NULL
        guchar public_key_token [17];
        guint32 hash_alg;
        guint32 hash_len;
        guint32 flags;
        UInt16 major, minor, build, revision;
#if MONO_2_10 || MONO_2_12
        UInt16 arch;
#endif
} MonoAssemblyName;

typedef void GFuncRef (void*, void*);
typedef GFuncRef* GFunc;

typedef enum {
	MONO_UNHANDLED_POLICY_LEGACY,
	MONO_UNHANDLED_POLICY_CURRENT
} MonoRuntimeUnhandledExceptionPolicy;

typedef enum {
	MONO_DL_LAZY  = 1,
	MONO_DL_LOCAL = 2,
	MONO_DL_MASK  = 3
} MonoDynamicLibraryFlag;

#if MONO_2_12
typedef enum {
	MONO_SECURITY_CORE_CLR_OPTIONS_DEFAULT = 0,
	MONO_SECURITY_CORE_CLR_OPTIONS_RELAX_REFLECTION = 1,
	MONO_SECURITY_CORE_CLR_OPTIONS_RELAX_DELEGATE = 2
} MonoSecurityCoreCLROptions;
#endif

#endif //ENABLE_MONO
#endif
