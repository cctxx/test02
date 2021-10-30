#pragma once

#include "Configuration/UnityConfigure.h"
#include "Runtime/GfxDevice/TextureIdMap.h"
#include "IncludesGLES20.h"

inline GLuint TextureIdMapGLES20_QueryOrCreate(TextureID texid)
{
    GLuint ret = (GLuint)TextureIdMap::QueryNativeTexture(texid);
    if(ret == 0)
    {
        GLES_CHK(glGenTextures(1, &ret));
        TextureIdMap::UpdateTexture(texid, ret);
    }

    return ret;
}
