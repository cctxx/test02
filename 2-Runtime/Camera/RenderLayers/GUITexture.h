#ifndef GUITEXTURE_H
#define GUITEXTURE_H

#include "GUIElement.h"
#include "Runtime/Graphics/Texture.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Math/Rect.h"


namespace Unity { class Material; }
namespace ShaderLab { class PropertySheet; }

// Attached to any game object in the scene.
// Registers with GUILayer, GUILayer renders it.
// Position comes from transform.position.x,y
// size comes from transform.scale.x,y
class GUITexture : public GUIElement
{
public:
	
	REGISTER_DERIVED_CLASS (GUITexture, GUIElement)
	DECLARE_OBJECT_SERIALIZE (GUITexture)
	
	GUITexture (MemLabelId label, ObjectCreationMode mode);

	virtual void Reset ();
			
	// GUIElement
	virtual void RenderGUIElement (const Rectf& cameraRect);
	virtual Rectf GetScreenRect (const Rectf& cameraRect);
	
	virtual void AwakeFromLoad(AwakeFromLoadMode mode);

	void SetColor (const ColorRGBAf& color);
	inline ColorRGBAf GetColor () { return m_Color; }

	void SetTexture (Texture* tex);
	Texture* GetTexture ();

	int m_LeftBorder; ///< The border pixels - this part of texture is never scaled.
	int m_RightBorder; ///< The border pixels - this part of texture is never scaled.
	int m_TopBorder; ///< The border pixels - this part of texture is never scaled.
	int m_BottomBorder; ///< The border pixels - this part of texture is never scaled.
	
	PPtr<Texture>  m_Texture;	///< The texture to use.
	ColorRGBAf     m_Color;		///< Tint color.

	static void InitializeClass();
	static void CleanupClass();

	const Rectf &GetPixelInset() const { return m_PixelInset; }
	void SetPixelInset(const Rectf &r) { m_PixelInset = r; SetDirty(); }
	
private:
	void DrawGUITexture (const Rectf& bounds);
	void BuildSheet ();
	Rectf CalculateDrawBox (const Rectf& screenViewportRect);

	Rectf          m_PixelInset;
	
	ShaderLab::PropertySheet *m_Sheet;
	int		m_PrevTextureWidth;
	int		m_PrevTextureHeight;
	int		m_PrevTextureBaseLevel;
};

// Immediate mode DrawGUITexture API
void DrawGUITexture (const Rectf &screenRect, Texture* texture, const Rectf &sourceRect, int leftBorder, int rightBorder, int topBorder, int bottomBorder, ColorRGBA32 color, Material* material = NULL);
void DrawGUITexture (const Rectf &screenRect, Texture* texture, int leftBorder, int rightBorder, int topBorder, int bottomBorder, ColorRGBA32 color, Material* material = NULL);
void DrawGUITexture (const Rectf &screenRect, Texture* texture, ColorRGBA32 color, Material* material = NULL);
void HandleGUITextureProps (ShaderLab::PropertySheet *sheet, Texture *texture);

#endif
