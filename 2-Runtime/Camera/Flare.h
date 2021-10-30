#ifndef FLARE_H
#define FLARE_H

#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/GameCode/Behaviour.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Graphics/Texture.h"

class Camera;
class ChannelAssigns;

// Source asset for a lens flare.
// Essentially it has some standard settings and a list of flare elements that make up the flare.
// All per-camera visibility is handled by the FlareManager, which is also responsible for rendering this flare.
class Flare : public NamedObject {
public:
	REGISTER_DERIVED_CLASS   (Flare, NamedObject)
	DECLARE_OBJECT_SERIALIZE (Flare)
	
	Flare(MemLabelId label, ObjectCreationMode mode);
	
	virtual void Reset ();
	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);

	// Render this flare with the center at pos, a visibility factor and a tint Color.
	void Render (Vector3f &pos, float visibility, const ColorRGBAf &tintColor, const ChannelAssigns& channels);
	
	// Set/Get the texture to use as a source for this flare
	void SetTexture (Texture *texture) { m_FlareTexture = texture; }
	Texture *GetTexture () const { return m_FlareTexture; }

private:
	struct FlareElement {
		unsigned int m_ImageIndex;	///< The image index from the flare texture
		float m_Position;				///< The position of the element (0 = light, 1 = screen center)
		float m_Size;					///< The size of the element
		ColorRGBAf	m_Color;			///< Element color tint
		bool m_UseLightColor;			///< Pick up the color from a light source?
		bool m_Rotate;					///< Rotate the flare in respect to light angle?
		bool m_Zoom;					///< Make the flare size dependent on visibility?
		bool m_Fade;					///< Make the flare fade dependent on visibility?
		
		FlareElement() {m_Fade=true;}
		
		DECLARE_SERIALIZE (FlareElement)
	};
	std::vector<FlareElement> m_Elements;	///< The individual flare elements.
	PPtr<Texture> m_FlareTexture;			///< The texture used for the flare elements.

	int m_TextureLayout;				///< enum { 1 Large 4 Small = 0, 1 Large 2 Medium 8 Small, 1 Texture, 2x2 Grid, 3x3 Grid, 4x4 Grid } Flare element layout in the texture.
	
	bool m_UseFog;
	Vector2f m_PixOffset;
};

template<class TransferFunc>
void Flare::FlareElement::Transfer (TransferFunc& transfer) {
	TRANSFER_SIMPLE (m_ImageIndex);
	TRANSFER_SIMPLE (m_Position);
	TRANSFER_SIMPLE (m_Size);
	TRANSFER_SIMPLE (m_Color);
	TRANSFER (m_UseLightColor);
	TRANSFER (m_Rotate);
	TRANSFER (m_Zoom);
	TRANSFER (m_Fade);
}

/// \todo Show flare outside screen option
/// \todo fade non-inf flares dependant on fog settings
class FlareManager {
public:
	struct FlareEntry {
		ColorRGBAf color;
		Vector3f position; // The world-space position of the flare, OR the direction vector if inf
		PPtr<Flare> flare; 
		UInt32 layers;
		UInt32 ignoredLayers;
		float brightness;
		float fadeSpeed;
		bool infinite;
		bool used;
		FlareEntry () 
		  : position (Vector3f (0,0,0)), layers (-1), ignoredLayers(-1), brightness(0.0f), infinite (false), used (true), fadeSpeed (3.0f) {}
	};
	static FlareManager &Get ();
	
	// Add a flare entry. returns the handle to the flare element.
	int AddFlare ();
	void UpdateFlare (	int  handle, Flare *flare, const Vector3f &position, 
						bool infinite, float brightness, const ColorRGBAf &color,
						float fadeSpeed, UInt32 layers, UInt32 ignoredLayers
					 );
	void DeleteFlare (int handle);

	/// Add and remove a camera from the list of cameras to track
	/// Used by the flarelayer
	void AddCamera (Camera &camera);
	void RemoveCamera (Camera &camera);

	void RenderFlares ();

private:
	/// The brightness for each camera for each flare.
	typedef std::map <const Camera *, std::vector<float> > RendererList;
	RendererList m_Renderers;
	
	typedef std::vector<FlareEntry> FlareList;
	FlareList m_Flares;
	void Update ();
};

inline FlareManager &GetFlareManager () {
	 return FlareManager::Get();
}

class LensFlare : public Behaviour {
public:
	REGISTER_DERIVED_CLASS   (LensFlare, Behaviour)
	DECLARE_OBJECT_SERIALIZE (LensFlare)

	LensFlare(MemLabelId label, ObjectCreationMode mode);

	static void InitializeClass ();
	static void CleanupClass () {}

	virtual void AddToManager ();
	virtual void RemoveFromManager ();
	virtual void Reset ();
	virtual void AwakeFromLoad (AwakeFromLoadMode awakeMode);
	inline void UpdateFlare ();

	void TransformChanged ();

	void SetBrightness (float brightness);
	float GetBrightness () const { return m_Brightness; }

	void SetFadeSpeed (float m_FadeSpeed);
	float GetFadeSpeed () const { return m_FadeSpeed; }
	
	void SetColor (const ColorRGBAf& color);
	const ColorRGBAf &GetColor () const {return m_Color; }
	
	
	void SetFlare (Flare *flare);
	Flare *GetFlare () { return m_Flare; }
private:
	PPtr<Flare> m_Flare;		///< Source flare asset to render.
	ColorRGBAf m_Color;			///< Color of the flare.
	float m_Brightness;			///< Brightness scale of the flare.
	float m_FadeSpeed;			///< Fade speed of the flare.
	BitField m_IgnoreLayers;	///< mask for layers that cannot hide flare
	int m_Handle;
	bool m_Directional;			///< Is this lensflare directional (true) or positional (false)
};

class FlareLayer : public Behaviour {
public:
	REGISTER_DERIVED_CLASS (FlareLayer, Behaviour)
	
	FlareLayer (MemLabelId label, ObjectCreationMode mode);

	virtual void AddToManager ();
	virtual void RemoveFromManager ();
};


#endif
