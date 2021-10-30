#include "UnityPrefix.h"

#if ENABLE_ASSET_STORE
#include "Editor/Src/LicenseActivationWindowCustomJS.h"
#include "Editor/Src/LicenseInfo.h"

static void dummy_init_cb(JSContextRef ctx, JSObjectRef object)
{
}

static void dummy_finalize_cb(JSObjectRef object)
{
}

static JSValueRef unity_close(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LicenseInfo::Get()->SignalUserClosedWindow();
	return JSValueMakeNull(context);
}

static const JSStaticFunction class_staticfuncs[] =
{
	{ "close", unity_close, kJSPropertyAttributeReadOnly },
	{ NULL, NULL, 0 }
};

static const JSClassDefinition class_def =
{
	0,
	kJSClassAttributeNone,
	"Unity",
	NULL,

	NULL,
	class_staticfuncs,

	dummy_init_cb,
	dummy_finalize_cb,

	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

void AddJSClasses(JSGlobalContextRef context)
{
	JSClassRef classDef = JSClassCreate(&class_def);
	JSObjectRef classObj = JSObjectMake(context, classDef, context);
	JSObjectRef globalObj = JSContextGetGlobalObject(context);
	JSStringRef str = JSStringCreateWithUTF8CString("Unity");
	JSObjectSetProperty(context, globalObj, str, classObj,
		kJSPropertyAttributeNone, NULL);
}

#endif // ENABLE_ASSET_STORE
