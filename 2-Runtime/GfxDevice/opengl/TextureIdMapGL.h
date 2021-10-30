#pragma once

#include "Configuration/UnityConfigure.h"
#include "Runtime/GfxDevice/TextureIdMap.h"

inline GLuint TextureIdMapGL_QueryOrCreate(TextureID texid)
{
    GLuint ret = (GLuint)TextureIdMap::QueryNativeTexture(texid);
    if(ret == 0)
    {
        glGenTextures(1, &ret);
        TextureIdMap::UpdateTexture(texid, ret);
    }

    return ret;
}
