#pragma once

#include "Runtime/Graphics/Image.h"

class MonoScript;
class Texture2D;
class Texture;
class Object;

enum { kShowRGB = 1 << 0, kShowAlpha = 1 << 1 };

// Creates a shared 2D texture by loading a texture named 'name'
// from inside the Resources folder.
// Returns NULL if no texture could be created.
Texture2D* Texture2DNamed (const std::string& name);

Texture2D* TextureForScript (MonoScript* script);

ImageReference ImageForClass (int classID);
Texture2D* TextureForClass (int classID);

Image ImageForIconAtPath (const std::string& path);
Texture* TextureForObject (Object* obj);

Image GetImageNamed (std::string name);

Image ImageForTexture (int instanceID, int widthHint, int heightHint, int& inoutFlags);
Texture2D* FileTextureAtAbsolutePath (const std::string& path);

// Called when the skin changes so we reload all images on next repaint
void FlushCachedObjectImages ();
