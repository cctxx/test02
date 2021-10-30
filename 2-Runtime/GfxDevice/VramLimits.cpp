#include "UnityPrefix.h"
#include "VramLimits.h"
#include "Runtime/Shaders/GraphicsCaps.h"


int ChooseSuitableFSAALevel( int width, int height, int backbufferBPP, int frontbufferBPP, int depthBPP, int fsaa )
{
	// no AA support?
	if( !gGraphicsCaps.hasMultiSample )
		return 0;

	// figure out appropriate AA level based on VRAM and screen size
	int vramKB = int(gGraphicsCaps.videoMemoryMB * 1024);

	const int vramAllowedByPortion = int(vramKB * 0.5f); // allow max. 50% of total VRAM for screen
	const int vramAllowedMax = 256 * 1024; // max 256MB for screen
	// Make sure at least this amount of free of VRAM left after the screen.
	// E.g. on a 32MB VRAM PPC MacMini, going 1280x960 2xAA still corrupts the screen (VRAM taken: 23.5MB).
	// So we need to keep somewhat more free than 8MB... 16MB works.
	const int vramAllowedWithKeepingSomeFree = int(vramKB - 16*1024);

	int vramAllowedKB = std::min(vramAllowedByPortion, std::min(vramAllowedMax,vramAllowedWithKeepingSomeFree));
	int vramNeededKB;
	do {
		vramNeededKB = width * height * (std::max(fsaa,1) * (backbufferBPP+depthBPP) + frontbufferBPP) / 1024;
		if( vramNeededKB < vramAllowedKB )
			break;

		#if !UNITY_RELEASE
		printf_console("Screen %ix%i at %ixAA won't fit, reducing AA (needed mem=%i allowedmem=%i)\n", width, height, fsaa, vramNeededKB, vramAllowedKB );
		#endif
		fsaa /= 2;
	} while( fsaa > 1 );

	if( fsaa == 1 )
		fsaa = 0;

	return fsaa;
}
