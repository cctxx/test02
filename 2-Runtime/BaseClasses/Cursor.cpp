#include "Cursor.h"

#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Graphics/RenderTexture.h"
#include "Runtime/Graphics/RenderBufferManager.h"
#include "Runtime/Camera/ImageFilters.h"
#include "Runtime/Graphics/ScreenManager.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Camera/CameraUtil.h"
#include "Runtime/Camera/RenderLayers/GUITexture.h"
#include "Runtime/Input/InputManager.h"
#include "Runtime/Misc/PlayerSettings.h"

namespace Cursors
{
void RenderSoftwareCursor ()
{
	Texture2D* softCursor = GetSoftwareCursor();
	if (softCursor && GetScreenManager().GetShowCursor())
	{
		DeviceMVPMatricesState preserveMVP;
		SetupPixelCorrectCoordinates();

		Vector2f pos = GetInputManager().GetMousePosition();
		Vector2f hotSpotOffset = GetCursorHotspot();

		pos.x -= hotSpotOffset.x;
		pos.y += hotSpotOffset.y;

		// the color is set to 0.5f because the gui-texture shader multiplies the vertex color by 2 for some reason
		// pos is floored to match the behaviour of the hardware cursors
		DrawGUITexture (Rectf ((int)pos.x, (int)pos.y, softCursor->GetGLWidth(), -softCursor->GetGLHeight()), softCursor, ColorRGBAf(0.5f, 0.5f, 0.5f, 0.5f));
	}
}

#if !PLATFORM_SUPPORTS_HARDWARE_CURSORS


typedef UnityCursor<int> SoftCursor;
typedef CursorManager<SoftCursor> SoftCursorManager;

template<> SoftCursorManager* SoftCursorManager::s_CursorManager = NULL;

static SoftCursor GenerateCursor (Texture2D* texture, Vector2f hotSpot)
{
	// if this is null you are doing it wrong
	assert(texture);

	SoftCursor c;
	c.sCursor = texture;
	c.hotspot = hotSpot;
	return c;
}

void SetCursor (Texture2D* texture, Vector2f hotSpot, CursorMode forceHardware)
{
	SoftCursorManager& manager = SoftCursorManager::Instance();
	if (!texture)
	{
		manager.m_CurrentCursor = manager.m_DefaultCursor;
		return;
	}

	// try and find the cursor in the cache
	SoftCursorManager::CursorCache::iterator found = manager.m_CursorCache.find (texture->GetTextureID());
	SoftCursor cursorToSet;
	bool shouldGenerateCursor = true;
	if (manager.m_CursorCache.end() != found)
	{
		// see if old hotspot
		// is the same as the one requested now...
		// if it's not then delete the old cursor and recreate it!
		cursorToSet = found->second;

		if (!CompareApproximately (hotSpot.x, cursorToSet.hotspot.x)
			|| !CompareApproximately (hotSpot.y, cursorToSet.hotspot.y))
		{
			manager.m_CursorCache.erase(found);
		}
		else
		{
			shouldGenerateCursor = false;
		}
	}

	if (shouldGenerateCursor)
	{
		cursorToSet = GenerateCursor (texture, hotSpot);
		manager.m_CursorCache[texture->GetTextureID()] = cursorToSet;
	}

	manager.m_CurrentCursor = cursorToSet;
}

Texture2D* GetSoftwareCursor()
{
	return SoftCursorManager::Instance().m_CurrentCursor.sCursor;
}

Vector2f GetCursorHotspot()
{
	return SoftCursorManager::Instance().m_CurrentCursor.hotspot;
}

void InitializeCursors(Texture2D* defaultCursorTexture, Vector2f defaultCursorHotSpot)
{
	SoftCursorManager& manager = SoftCursorManager::Instance ();
	if (defaultCursorTexture)
	{
		manager.m_DefaultCursor = GenerateCursor (defaultCursorTexture, defaultCursorHotSpot);
		manager.m_CurrentCursor = manager.m_DefaultCursor;
		manager.m_UsingBuiltinDefaultCursor = true;
	}
}

void CleanupCursors()
{
	SoftCursorManager::Instance().Cleanup ();
}

// needed for windows linkage with hardware cursors disabled
#if UNITY_WIN 
void ResetCursor ()
{}

bool HandleMouseCursor (UINT message, LPARAM lParam) 
{
	return false;
}

HCURSOR GetHardwareCursor ()
{
	return NULL;
}
#endif

// needed for osx linkage with hardware cursors disabled
#if UNITY_OSX
void ResetCursor ()
{}
    
NSCursor* GetCurrentCursor () 
{
	return NULL;
}
#endif

#endif //!PLATFORM_SUPPORTS_HARDWARE_CURSORS

}; //namespace

