#ifndef GIZMO_RENDERER_H
#define GIZMO_RENDERER_H

#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "GizmoVertexFormats.h"

class Matrix4x4f;
namespace Unity { class Material; }
class Texture;
class Texture2D;


namespace gizmos {
	extern ColorRGBAf g_GizmoColor;
	extern ColorRGBA32 g_GizmoPickID;
	extern bool g_GizmoPicking;

	// Usage is this:
	// for each logical gizmo object:
	//     BeginGizmo with its logical center
	//     Add*Primitives arbitrary number of times. Each primitive batch will just append to previous one of this object
	//       if it can.
	// also, AddIcon can be called at arbitrary times; icons go into their own queue, and are rendered by RenderGizmos as well
	// RenderGizmos
	//
	// Rendering the gizmos will sort all logical objects back to front.

	void BeginGizmo( const Vector3f& center );

	void AddLinePrimitives( GfxPrimitiveType primType, int vertexCount, const Vector3f* verts, bool depthTest );
	void AddColorPrimitives( GfxPrimitiveType primType, int vertexCount, const ColorVertex* verts );
	void AddLitPrimitives( GfxPrimitiveType primType, int vertexCount, const LitVertex* verts );
	void AddColorOcclusionPrimitives( GfxPrimitiveType primType, int vertexCount, const ColorVertex* verts );

	void AddIcon( const Vector3f& center, Texture2D* icon, const char* text = 0, bool allowScaling = true, ColorRGBA32 tint = ColorRGBA32(255,255,255,255) );

	void RenderGizmos();
	void ClearGizmos();
	
	Unity::Material* GetMaterial();
	Unity::Material* GetColorMaterial();
	Unity::Material* GetLitMaterial();

} // namespace gizmo renderer


#endif
