#ifndef WINRTMARSHALERS_H
#define WINRTMARSHALERS_H

#if UNITY_WINRT && ENABLE_SCRIPTING

class AssetBundleCreateRequest;
struct AssetBundleRequestMono;
struct MonoLOD;
struct MonoNavMeshPath;
struct MonoGUIContent;
struct Coroutine;
class WWW;
struct LightmapDataMono;
struct MonoTreePrototype;
struct AnimationEvent;
struct ControllerColliderHit;
class AsyncOperation;
struct MonoContactPoint;
struct MonoWebCamDevice;
struct MonoCollision;
struct MonoDetailPrototype;
struct MonoSplatPrototype;
struct ScriptingCollision2D;
struct ScriptingContactPoint2D;
struct ScriptingCharacterInfo;

void SetupMarshalCallbacks();

void MarshallManagedStructIntoNative(ScriptingObjectPtr so, AssetBundleCreateRequest** dest);
void MarshallManagedStructIntoNative(ScriptingObjectPtr so, MonoLOD* dest);
void MarshallManagedStructIntoNative(ScriptingObjectPtr so, MonoNavMeshPath* dest);
void MarshallManagedStructIntoNative(ScriptingObjectPtr so, MonoGUIContent* dest);
void MarshallManagedStructIntoNative(ScriptingObjectPtr so, float* dest);
void MarshallManagedStructIntoNative(ScriptingObjectPtr so, Coroutine** dest);
void MarshallManagedStructIntoNative(ScriptingObjectPtr so, WWW** dest);
void MarshallManagedStructIntoNative(ScriptingObjectPtr so, LightmapDataMono* dest);
void MarshallManagedStructIntoNative(ScriptingObjectPtr so, MonoTreePrototype* dest);
void MarshallManagedStructIntoNative(ScriptingObjectPtr so, MonoDetailPrototype* dest);
void MarshallManagedStructIntoNative(ScriptingObjectPtr so, MonoSplatPrototype* dest);
void MarshallManagedStructIntoNative(ScriptingObjectPtr so, ScriptingCharacterInfo* dest);

template<class T> inline
void MarshallManagedStructIntoNative(ScriptingObjectPtr so, T* dest)
{
	MarshallManagedStructIntoNative(so, dest);
}

void MarshallNativeStructIntoManaged(const AnimationEvent* src, ScriptingObjectPtr& dest);
void MarshallNativeStructIntoManaged(const AssetBundleRequestMono& src, ScriptingObjectPtr& dest);
void MarshallNativeStructIntoManaged(const MonoNavMeshPath& src, ScriptingObjectPtr& dest);
void MarshallNativeStructIntoManaged(const void* src, ScriptingObjectPtr& dest);
void MarshallNativeStructIntoManaged(const ControllerColliderHit& src, ScriptingObjectPtr& dest);
void MarshallNativeStructIntoManaged(const AsyncOperation* src, ScriptingObjectPtr& dest);
void MarshallNativeStructIntoManaged(const MonoContactPoint& src, ScriptingObjectPtr& dest);
void MarshallNativeStructIntoManaged(const MonoTreePrototype& src, ScriptingObjectPtr& dest);
void MarshallNativeStructIntoManaged(const MonoWebCamDevice& src, ScriptingObjectPtr& dest);
void MarshallNativeStructIntoManaged(const MonoCollision& src, ScriptingObjectPtr& dest);
void MarshallNativeStructIntoManaged(const LightmapDataMono& src, ScriptingObjectPtr& dest);
void MarshallNativeStructIntoManaged(const MonoDetailPrototype& src, ScriptingObjectPtr& dest);
void MarshallNativeStructIntoManaged(const MonoSplatPrototype& src, ScriptingObjectPtr& dest);
void MarshallNativeStructIntoManaged(const ScriptingCollision2D& src, ScriptingObjectPtr& dest);
void MarshallNativeStructIntoManaged(const ScriptingContactPoint2D& src, ScriptingObjectPtr& dest);
void MarshallNativeStructIntoManaged(const ScriptingCharacterInfo& src, ScriptingObjectPtr& dest);
#endif

#endif
