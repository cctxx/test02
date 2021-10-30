// Fast DXT compression code adapted from
// stb_dxt by Sean Barrett: http://nothings.org/stb/stb_dxt.h
// which is in turn adapted from code by
// Fabian "ryg" Giesen: http://www.farbrausch.de/~fg/code/dxt/

#ifndef __DXT_COMPRESSION__
#define __DXT_COMPRESSION__

// src: 32 bit/pixel width*height image, R,G,B,A bytes in pixel.
// dest must be (width+3)/4*(height+3)/4 bytes if dxt5=true, and half of that if dxt5=false (dxt1 is used then).
void FastCompressImage (int width, int height, const UInt8* src, UInt8* dest, bool dxt5, bool dither );

#endif
