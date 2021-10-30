#ifndef GL_DATABUFFER_COMMON_H
#define GL_DATABUFFER_COMMON_H

#if GFX_SUPPORTS_OPENGL || GFX_SUPPORTS_OPENGLES || GFX_SUPPORTS_OPENGLES20 || GFX_SUPPORTS_OPENGLES30

#define DATA_BUFFER_ID_MASK 0xC0000000
#define MAKE_DATA_BUFFER_ID(id) (id|DATA_BUFFER_ID_MASK)

inline void glRegisterBufferData(UInt32 bufferID, GLsizeiptr size, void* related) 
{
	REGISTER_EXTERNAL_GFX_DEALLOCATION(MAKE_DATA_BUFFER_ID(bufferID) );
	REGISTER_EXTERNAL_GFX_ALLOCATION_REF(MAKE_DATA_BUFFER_ID(bufferID),size,related);
}

inline void glDeregisterBufferData(int count, GLuint* buffferIds)
{
	for (size_t q = 0; q < count; ++q)
		REGISTER_EXTERNAL_GFX_DEALLOCATION(MAKE_DATA_BUFFER_ID(buffferIds[q]) );
}

#endif 

#endif // GL_DATABUFFER_COMMON_H

