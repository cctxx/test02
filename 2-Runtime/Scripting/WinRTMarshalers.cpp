#include "UnityPrefix.h"

#if UNITY_WINRT && ENABLE_SCRIPTING
#include "Runtime/Scripting/Backend/ScriptingTypes.h"
#include "Runtime/Scripting/ScriptingUtility.h"


#include "Runtime/Misc/AssetBundleUtility.h"
#include "Runtime/Camera/LODGroup.h"
#include "Runtime/NavMesh/NavMeshTypes.h"
#include "Runtime/Terrain/SplatDatabase.h"
#include "Runtime/Terrain/DetailDatabase.h"
#include "Runtime/Terrain/TreeDatabase.h"
#include "Runtime/IMGUI/GUIContent.h"
#include "Runtime/Mono/Coroutine.h"
#include "Runtime/Export/WWW.h"
#include "Runtime/Graphics/LightmapSettings.h"
#include "Runtime/Animation/AnimationEvent.h"

#include "Runtime/Dynamics/CharacterController.h"
#include "Runtime/Misc/AsyncOperation.h"
#include "Runtime/Dynamics/Collider.h"
#include "Runtime/Physics2D/Collider2D.h"
#include "Runtime/Physics2D/CollisionListener2D.h"
#include "Runtime/Video/VideoTexture.h"
#include "Runtime/Threads/ThreadSpecificValue.h"
#include "Runtime/Filters/Misc/Font.h"

UNITY_TLS_VALUE(void*) s_CurrentTarget;

void _MarshalSetMonoLOD(float screenRelativeTransitionHeight, /*WinRTScriptingObjectWrapper*/long long renderers)
{
	MonoLOD* monoLOD = (MonoLOD*)s_CurrentTarget;
	monoLOD->screenRelativeTransitionHeight = screenRelativeTransitionHeight;
	monoLOD->renderers = renderers;
}
void _MarshalSetNavMeshPath(int ptr, /*WinRTScriptingObjectWrapper*/long long corners)
{
	MonoNavMeshPath* monoNavMeshPath = (MonoNavMeshPath*)s_CurrentTarget;
	monoNavMeshPath->native = (NavMeshPath*)ptr;
	monoNavMeshPath->corners = corners;
}
void _MarshalSetMonoGUIContent(Platform::String^ text, Platform::String^ tooltip, /*WinRTScriptingObjectWrapper*/long long image)
{
	MonoGUIContent* monoGUIContent = (MonoGUIContent*)s_CurrentTarget;
	monoGUIContent->m_Text = text;
	monoGUIContent->m_Tooltip = tooltip;
	monoGUIContent->m_Image = image;
}
void _MarshalSetLightmapDataMono(/*WinRTScriptingObjectWrapper*/long long lightmap, /*WinRTScriptingObjectWrapper*/long long indirectLightmap)
{
	LightmapDataMono* monoLightmap = (LightmapDataMono*)s_CurrentTarget;
	monoLightmap->m_Lightmap = lightmap;
	monoLightmap->m_IndirectLightmap = indirectLightmap;
}
void _MarshalSetMonoTreePrototype(/*WinRTScriptingObjectWrapper*/long long prefab, float bendFactor)
{
	MonoTreePrototype* monoTreePrototype = (MonoTreePrototype*)s_CurrentTarget;
	monoTreePrototype->prefab = prefab;
	monoTreePrototype->bendFactor = bendFactor;
}


void _MarshalSetMonoDetailPrototype1(/*WinRTScriptingObjectWrapper*/long long prototype, /*WinRTScriptingObjectWrapper*/long long prototypeTexture,
									float healthyColorR, float healthyColorG, float healthyColorB, float healthyColorA,
									float dryColorR, float dryColorG, float dryColorB, float dryColorA)
{
	MonoDetailPrototype* monoDetailPrototype = (MonoDetailPrototype*)s_CurrentTarget;
	monoDetailPrototype->prototype = prototype;
	monoDetailPrototype->prototypeTexture = prototypeTexture;

	monoDetailPrototype->healthyColor = ColorRGBAf(healthyColorR, healthyColorG, healthyColorB, healthyColorA);
	monoDetailPrototype->dryColor = ColorRGBAf(dryColorR, dryColorG, dryColorB, dryColorA);
}
void _MarshalSetMonoDetailPrototype2(float minWidth, float maxWidth,
									float minHeight, float maxHeight,
									float noiseSpread,
									float bendFactor,
									int renderMode,
									int usePrototypeMesh)
{
	MonoDetailPrototype* monoDetailPrototype = (MonoDetailPrototype*)s_CurrentTarget;

	monoDetailPrototype->minWidth = minWidth;
	monoDetailPrototype->maxWidth = maxWidth;
	monoDetailPrototype->minHeight = minHeight;
	monoDetailPrototype->maxHeight = maxHeight;
	monoDetailPrototype->noiseSpread = noiseSpread;
	monoDetailPrototype->bendFactor = bendFactor;
	monoDetailPrototype->renderMode = renderMode;
	monoDetailPrototype->usePrototypeMesh = usePrototypeMesh;
}
void _MarshalSetMonoSplatPrototype(/*WinRTScriptingObjectWrapper*/long long texture, /*WinRTScriptingObjectWrapper*/long long normalMap, 
								   float tileSizeX, float tileSizeY,
								   float tileOffsetX, float tileOffsetY)
{
	MonoSplatPrototype* monoSplatPrototype = (MonoSplatPrototype*)s_CurrentTarget;
	monoSplatPrototype->texture = texture;
	monoSplatPrototype->normalMap = normalMap;
	monoSplatPrototype->tileSize = Vector2f(tileSizeX, tileSizeY);
	monoSplatPrototype->tileOffset = Vector2f(tileOffsetX, tileOffsetX);
}
void _MarshalSetScriptingCharacterInfo(int index, BridgeInterface::RectWrapper uv, BridgeInterface::RectWrapper vert, float width, int size, int style, bool flipped)
{
	ScriptingCharacterInfo* o = (ScriptingCharacterInfo*)s_CurrentTarget;
	o->index = index;
	o->uv = Rectf(uv.m_XMin, uv.m_YMin, uv.m_Width, uv.m_Height);
	o->vert = Rectf(vert.m_XMin, vert.m_YMin, vert.m_Width, vert.m_Height);
	o->width = width;
	o->size = size;
	o->style = style;
	o->flipped = flipped;
}

void SetupMarshalCallbacks()
{
	GetWinRTMarshalling()->SetupMarshalCallbacks(
		ref new BridgeInterface::MarshalSetMonoLOD(_MarshalSetMonoLOD),
		ref new BridgeInterface::MarshalSetNavMeshPath(_MarshalSetNavMeshPath),
		ref new BridgeInterface::MarshalSetMonoGUIContent(_MarshalSetMonoGUIContent),
		ref new BridgeInterface::MarshalSetLightmapDataMono(_MarshalSetLightmapDataMono),
		ref new BridgeInterface::MarshalSetMonoTreePrototype(_MarshalSetMonoTreePrototype),
		ref new BridgeInterface::MarshalSetMonoDetailPrototype1(_MarshalSetMonoDetailPrototype1),
		ref new BridgeInterface::MarshalSetMonoDetailPrototype2(_MarshalSetMonoDetailPrototype2),
		ref new BridgeInterface::MarshalSetMonoSplatPrototype(_MarshalSetMonoSplatPrototype),
		ref new BridgeInterface::MarshalSetScriptingCharacterInfo(_MarshalSetScriptingCharacterInfo));
}
//
// Marshalling: Managed - to - Native
//

void MarshallManagedStructIntoNative(ScriptingObjectPtr so, AssetBundleCreateRequest** dest)
{
	*dest = (AssetBundleCreateRequest*)GetWinRTMarshalling()->MarshalGetAssetBundleCreateRequestPtr(so.GetHandle());
}
void MarshallManagedStructIntoNative(ScriptingObjectPtr so, MonoLOD* dest)
{
	s_CurrentTarget = dest;
	GetWinRTMarshalling()->MarshallManagedMonoLOD(so.GetHandle());
}
void MarshallManagedStructIntoNative(ScriptingObjectPtr so, MonoNavMeshPath* dest)
{
	s_CurrentTarget = dest;
	GetWinRTMarshalling()->MarshallManagedMonoNavMeshPath(so.GetHandle());
}
void MarshallManagedStructIntoNative(ScriptingObjectPtr so, MonoGUIContent* dest)
{
	s_CurrentTarget = dest;
	GetWinRTMarshalling()->MarshallManagedMonoGUIContent(so.GetHandle());
}
void MarshallManagedStructIntoNative(ScriptingObjectPtr so, float* dest)
{
	*dest = GetWinRTMarshalling()->MarshallGetManagedWaitForSecondsWait(so.GetHandle());
}
void MarshallManagedStructIntoNative(ScriptingObjectPtr so, Coroutine** dest)
{
	*dest = (Coroutine*)GetWinRTMarshalling()->MarshallGetManagedCoroutinePtr(so.GetHandle());
}
void MarshallManagedStructIntoNative(ScriptingObjectPtr so, WWW** dest)
{
	*dest = (WWW*)GetWinRTMarshalling()->MarshallGetManagedWWWPtr(so.GetHandle());
}
void MarshallManagedStructIntoNative(ScriptingObjectPtr so, LightmapDataMono* dest)
{
	s_CurrentTarget = dest;
	GetWinRTMarshalling()->MarshallManagedLightmapDataMono(so.GetHandle());
}
void MarshallManagedStructIntoNative(ScriptingObjectPtr so, MonoTreePrototype* dest)
{
	s_CurrentTarget = dest;
	GetWinRTMarshalling()->MarshallManagedMonoTreePrototype(so.GetHandle());
}
void MarshallManagedStructIntoNative(ScriptingObjectPtr so, MonoDetailPrototype* dest)
{
	s_CurrentTarget = dest;
	GetWinRTMarshalling()->MarshallManagedMonoDetailPrototype(so.GetHandle());
}
void MarshallManagedStructIntoNative(ScriptingObjectPtr so, MonoSplatPrototype* dest)
{
	s_CurrentTarget = dest;
	GetWinRTMarshalling()->MarshallManagedMonoSplatPrototype(so.GetHandle());
}

void MarshallManagedStructIntoNative(ScriptingObjectPtr so, ScriptingCharacterInfo* dest)
{
	s_CurrentTarget = dest;
	GetWinRTMarshalling()->MarshallManagedScriptingCharacterInfo(so.GetHandle());
}

//
// Marshalling: Native - to - Managed
//
void MarshallNativeStructIntoManaged(const AnimationEvent* src, ScriptingObjectPtr& dest)
{
	GetWinRTMarshalling()->MarshallNativeAnimationEventPtr((int)src, dest.GetHandle());
}
void MarshallNativeStructIntoManaged(const AssetBundleRequestMono& src, ScriptingObjectPtr& dest)
{
	GetWinRTMarshalling()->MarshallNativeAssetBundleRequestTo((int)src.m_Result, src.m_AssetBundle, src.m_Path, src.m_Type, dest.GetHandle());
}
void MarshallNativeStructIntoManaged(const MonoNavMeshPath& src, ScriptingObjectPtr& dest)
{
	GetWinRTMarshalling()->MarshallNativeNavMashPathTo((int)src.native, src.corners, dest.GetHandle());
}
void MarshallNativeStructIntoManaged(const void* src, ScriptingObjectPtr& dest)
{
	GetWinRTMarshalling()->MarshallNativeVoidPtrTo((int)src, dest.GetHandle());
}

void MarshallNativeStructIntoManaged(const ControllerColliderHit& src, ScriptingObjectPtr& dest)
{
	GetWinRTMarshalling()->MarshallNativeControllerColliderHitTo(src.controller, src.collider, 
		src.point.x, src.point.y, src.point.z,
		src.normal.x, src.normal.y, src.normal.z,
		src.motionDirection.x, src.motionDirection.y, src.motionDirection.z,
		src.motionLength,
		src.push,
		dest);
}
void MarshallNativeStructIntoManaged(const AsyncOperation* src, ScriptingObjectPtr& dest)
{
	GetWinRTMarshalling()->MarshallNativeAsyncOperationPtrTo((int)src, dest.GetHandle());
}
void MarshallNativeStructIntoManaged(const MonoContactPoint& src, ScriptingObjectPtr& dest)
{
	dest = GetWinRTMarshalling()->MarshallNativeMonoContactPointTo(src.point.x, src.point.y, src.point.z,
		src.normal.x, src.normal.y, src.normal.z,
		src.thisCollider,
		src.otherCollider);
}
void MarshallNativeStructIntoManaged(const MonoTreePrototype& src, ScriptingObjectPtr& dest)
{
	GetWinRTMarshalling()->MarshallNativeMonoTreePrototypeTo(src.prefab, src.bendFactor, dest.GetHandle());
}

#if ENABLE_WEBCAM
void MarshallNativeStructIntoManaged(const MonoWebCamDevice& src, ScriptingObjectPtr& dest)
{
	dest = GetWinRTMarshalling()->MarshallNativeMonoWebCamDeviceTo(src.name, src.flags);
}
#endif

void MarshallNativeStructIntoManaged(const MonoCollision& src, ScriptingObjectPtr& dest)
{
	GetWinRTMarshalling()->MarshallNativeMonoCollisionTo(src.relativeVelocity.x, src.relativeVelocity.y, src.relativeVelocity.z,
		src.rigidbody, src.collider, src.contacts.GetHandle(), dest.GetHandle());
}
void MarshallNativeStructIntoManaged(const LightmapDataMono& src, ScriptingObjectPtr& dest)
{
	GetWinRTMarshalling()->MarshallNativeLightmapDataMonoTo(src.m_Lightmap, src.m_IndirectLightmap, dest.GetHandle());
}

void MarshallNativeStructIntoManaged(const MonoDetailPrototype& src, ScriptingObjectPtr& dest)
{
	GetWinRTMarshalling()->MarshallNativeMonoDetailPrototype1To(
		src.prototype, src.prototypeTexture, 
		src.healthyColor.r, src.healthyColor.g, src.healthyColor.b, src.healthyColor.a, 
		src.dryColor.r, src.dryColor.g, src.dryColor.b, src.dryColor.a, 
		dest);
	GetWinRTMarshalling()->MarshallNativeMonoDetailPrototype2To(
		src.minWidth, src.maxWidth,
		src.minHeight, src.maxHeight,
		src.noiseSpread, src.bendFactor,
		src.renderMode, src.usePrototypeMesh,
		dest);
}
void MarshallNativeStructIntoManaged(const MonoSplatPrototype& src, ScriptingObjectPtr& dest)
{
	GetWinRTMarshalling()->MarshallNativeMonoSplatPrototypeTo(src.texture, src.normalMap, 
		src.tileSize.x, src.tileSize.y,
		src.tileOffset.x, src.tileOffset.y,
		dest);
}
#if ENABLE_2D_PHYSICS
void MarshallNativeStructIntoManaged(const ScriptingCollision2D& src, ScriptingObjectPtr& dest)
{
	GetWinRTMarshalling()->MarshallNativeCollision2DTo(src.rigidbody.GetHandle(), src.collider.GetHandle(), src.contacts.GetHandle(), dest.GetHandle());
}

void MarshallNativeStructIntoManaged(const ScriptingContactPoint2D& src, ScriptingObjectPtr& dest)
{
	dest = GetWinRTMarshalling()->MarshallNativeContactPoint2DTo(
		src.point.x, src.point.y,
		src.normal.x, src.normal.y,
		src.collider,
		src.otherCollider);
}
#endif
void MarshallNativeStructIntoManaged(const ScriptingCharacterInfo& src, ScriptingObjectPtr& dest)
{
	BridgeInterface::RectWrapper uv;
	BridgeInterface::RectWrapper vert;
	uv.m_XMin = src.uv.x;
	uv.m_YMin = src.uv.y;
	uv.m_Width = src.uv.width;
	uv.m_Height = src.uv.height;

	vert.m_XMin = src.vert.x;
	vert.m_YMin = src.vert.y;
	vert.m_Width = src.vert.width;
	vert.m_Height = src.vert.height;
	dest = GetWinRTMarshalling()->MarshallNativeScriptingCharacterInfoTo(src.index, 
		uv, vert,
		src.width, src.size, src.style, src.flipped);
}
#endif
