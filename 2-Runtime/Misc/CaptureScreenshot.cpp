#include "UnityPrefix.h"
#include "CaptureScreenshot.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/IMGUI/GUIManager.h"
#include "Runtime/Network/PlayerCommunicator/PlayerConnection.h"
#if ENABLE_RETAINEDGUI
#include "Runtime/ExGUI/GUITracker.h"
#endif

#if UNITY_WII
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "External/stb/stb_image_write.h"
#include "PlatformDependent/Wii/WiiHio2.h"
#endif

#if UNITY_FLASH
#include "PlatformDependent/FlashSupport/cpp/FlashUtils.h"
#include "Runtime/GfxDevice/molehill/MolehillUtils.h"
#endif

#if UNITY_WP8
#include "Runtime/Graphics/ScreenManager.h"
#endif


#if UNITY_PS3
#include <cell/codec.h>

	bool ConvertImageToPNGFilePS3 (const ImageReference& inputImage, const char* path)
	{
		int ret;
		static bool hasCreatedEncoder = false;
		static CellPngEncHandle encoderHandle;

		const UInt32 w = inputImage.GetWidth();
		const UInt32 h = inputImage.GetHeight();
		const UInt32 p = inputImage.GetRowBytes();
		const UInt32 bpp = p / w;

		if (!hasCreatedEncoder)
		{
			CELLCALL(cellSysmoduleLoadModule(CELL_SYSMODULE_PNGENC));

			CellPngEncConfig config;
			CellPngEncAttr attr;
			CellPngEncResource resource;

			config.maxWidth = 1920;
			config.maxHeight = 1080;
			config.maxBitDepth = 8;
			config.addMemSize = 0;
			config.enableSpu = true;
			config.exParamList = NULL;
			config.exParamNum = 0;

			CELLCALL(cellPngEncQueryAttr(&config, &attr));

			resource.memAddr = malloc(attr.memSize);
			resource.memSize = attr.memSize;
			resource.ppuThreadPriority = 1000;
			resource.spuThreadPriority = 200;

			CELLCALL(cellPngEncOpen(&config, &resource, &encoderHandle));
			
			hasCreatedEncoder = true;
		}

		CellPngEncPicture picture;
		CellPngEncEncodeParam encodeParam;
		CellPngEncOutputParam outputParam;
		CellPngEncStreamInfo streamInfo;

		picture.width = w;
		picture.height = h;
		picture.pitchWidth = bpp * w;
		picture.colorSpace = bpp == 4 ? CELL_PNGENC_COLOR_SPACE_ARGB : CELL_PNGENC_COLOR_SPACE_RGB;
		picture.bitDepth = 8;
		picture.packedPixel = false;
		picture.pictureAddr = inputImage.GetImageData();

		encodeParam.enableSpu = true;
		encodeParam.encodeColorSpace = CELL_PNGENC_COLOR_SPACE_RGB;
		encodeParam.compressionLevel = CELL_PNGENC_COMPR_LEVEL_1;
		encodeParam.filterType = CELL_PNGENC_FILTER_TYPE_SUB;
		encodeParam.ancillaryChunkList = NULL;
		encodeParam.ancillaryChunkNum = 0;

		outputParam.location = CELL_PNGENC_LOCATION_BUFFER;
		outputParam.streamFileName = NULL;
		outputParam.limitSize = w * h * 4;
		outputParam.streamAddr = malloc(outputParam.limitSize);

		CELLCALL(cellPngEncEncodePicture(encoderHandle, &picture, &encodeParam, &outputParam));

		// wait for encoding to finish

		uint32_t streamInfoNum;

		CELLCALL(cellPngEncWaitForOutput(encoderHandle, &streamInfoNum, true));
		CELLCALL(cellPngEncGetStreamInfo(encoderHandle, &streamInfo));

		string finalPath = path;
		ConvertSeparatorsToPlatform(finalPath);

		File file;
		if (!file.Open(finalPath.c_str(), File::kWritePermission))
			return false;

		file.Write(streamInfo.streamAddr, streamInfo.streamSize);
		file.Close();

#if ENABLE_PLAYERCONNECTION
		TransferFileOverPlayerConnection(finalPath, streamInfo.streamAddr, streamInfo.streamSize);
#endif
		free(outputParam.streamAddr);
		return true;
	}
#endif


#if UNITY_XENON
bool ConvertImageToPNGFileXbox360(const ImageReference& inputImage, const char* path)
{
	const UInt32 w = inputImage.GetWidth();
	const UInt32 h = inputImage.GetHeight();
	const UInt32 p = inputImage.GetRowBytes();
	const UInt32 s = h * p;
	const UInt32 bpp = p / w;
	if (bpp != 4)
	{
		UNITY_TRACE("Can only save 32bpp screenshots.\n");
		return false;
	}

	DWORD size = XGSetTextureHeader(inputImage.GetWidth(), inputImage.GetHeight(), 1, D3DUSAGE_CPU_CACHED_MEMORY, D3DFMT_LIN_A8R8G8B8, 0, 0, 0, p, NULL, NULL, NULL);
	if (size != s)
	{
		UNITY_TRACE("Invalid screenshot size.\n");
		return false;
	}
	
	bool success = true;
	
	IDirect3DTexture9* tex = new IDirect3DTexture9();
	XGSetTextureHeader(inputImage.GetWidth(), inputImage.GetHeight(), 1, D3DUSAGE_CPU_CACHED_MEMORY, D3DFMT_LIN_A8R8G8B8, 0, 0, 0, p, tex, NULL, NULL);
	XGOffsetResourceAddress(tex, inputImage.GetImageData());

	string finalPath = path;
	ConvertSeparatorsToPlatform(finalPath);

#if ENABLE_PLAYERCONNECTION
	ID3DXBuffer* buffer = 0;
	HRESULT hr = D3DXSaveTextureToFileInMemory(&buffer, D3DXIFF_PNG, tex, NULL);
	success &= SUCCEEDED(hr);

	if (buffer)
	{
		File file;
		if (file.Open(finalPath.c_str(), File::kWritePermission))
		{
			success &= file.Write(buffer->GetBufferPointer(), buffer->GetBufferSize());
			file.Close();
		}
		else
			success = false;
		
		TransferFileOverPlayerConnection(finalPath, buffer->GetBufferPointer(), buffer->GetBufferSize());
		buffer->Release();
	}
#else
	HRESULT hr = D3DXSaveTextureToFile(finalPath.c_str(), D3DXIFF_PNG, tex, NULL);
	success &= SUCCEEDED(hr);
#endif

	delete tex;

	return success;
}
#endif

#if UNITY_XENON || UNITY_PS3
//#define ENABLE_MULTITHREADED 1
	bool ConvertImageToTGAFile (const ImageReference& inputImage, const char* path)
	{
		bool success = false;
		const UInt32 w = inputImage.GetWidth();
		const UInt32 h = inputImage.GetHeight();
		const UInt32 p = inputImage.GetRowBytes();
		const UInt32 bpp = p / w;

		UInt8 tgaHeader[18] =  {
			0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
			(w>>0)&0xff, (w>>8)&0xff, 
			(h>>0)&0xff, (h>>8)&0xff, 
			bpp<<3, (bpp==4)?8:0 
		};

		string finalPath = path;
		ConvertSeparatorsToPlatform(finalPath);

		File file;
		if (!file.Open(finalPath.c_str(), File::kWritePermission))
			return false;

		success |= file.Write(tgaHeader, 18);
		success |= file.Write(inputImage.GetImageData(), h*p);

		file.Close();

#if ENABLE_PLAYERCONNECTION
		TransferFileOverPlayerConnection(finalPath, inputImage.GetImageData(), h*p, tgaHeader, sizeof(tgaHeader));
#endif

		return success;
	}
#endif

// don't even compile the code in web player
#if CAPTURE_SCREENSHOT_AVAILABLE

#include "Runtime/Graphics/Image.h"
#include "Runtime/Graphics/ImageConversion.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Misc/SystemInfo.h"

using namespace std;

static const char* gCaptureScreenshotPath = NULL;
static int s_CaptureSuperSize;

#if CAPTURE_SCREENSHOT_THREAD
Thread gCaptureScreenshotThread;
#endif

void QueueScreenshot (const string& path, int superSize)
{
#if UNITY_IPHONE || UNITY_ANDROID
	gCaptureScreenshotPath = strdup (AppendPathName (systeminfo::GetPersistentDataPath(), path).c_str());
#else
	gCaptureScreenshotPath = strdup(PathToAbsolutePath(path).c_str());
#endif
	s_CaptureSuperSize = clamp (superSize, 0, 16);
}

void FinishAllCaptureScreenshot ()
{
#if CAPTURE_SCREENSHOT_THREAD
	gCaptureScreenshotThread.WaitForExit();
#endif
}

struct WriteImageAsync
{
	std::string path;
	Image* image;
};

void* WriteImageAsyncThread (void* data)
{
	WriteImageAsync* async = static_cast<WriteImageAsync*> (data);

#if UNITY_XENON 
	async->image->ReformatImage( async->image->GetWidth(), async->image->GetHeight(), kTexFormatRGBA32 );
	if (!ConvertImageToPNGFileXbox360 (*async->image, async->path.c_str()))
	{
		ErrorString( "Failed to store screen shot" );
	}
#elif UNITY_PS3
	async->image->ReformatImage( async->image->GetWidth(), async->image->GetHeight(), kTexFormatRGBA32 );
	if (!ConvertImageToPNGFilePS3 (*async->image, async->path.c_str()))
	{
		ErrorString( "Failed to store screen shot" );
	}
#elif UNITY_WII
	async->image->ReformatImage( async->image->GetWidth(), async->image->GetHeight(), kTexFormatRGBA32 );

	int len;
	UInt8* pngData = stbi_write_png_to_mem(
	   async->image->GetImageData(),
	   async->image->GetRowBytes(), 
	   async->image->GetWidth(), 
	   async->image->GetHeight(), 
	   async->image->GetRowBytes() / async->image->GetWidth(), &len);
	bool success = pngData != NULL;
	if (success)
	{
		#if ENABLE_HIO2
			success = wii::hio2::SendFile(async->path.c_str(), pngData, len);
		#else
			success = false;
		#endif
		free(pngData); 
	}

	if (!success)
	{
		ErrorString(Format("Failed to write screenshot to path '%s', (remember this path is relative to *.mcp file)", async->path.c_str()).c_str());
	}
#elif ENABLE_PNG_JPG
	async->image->ReformatImage( async->image->GetWidth(), async->image->GetHeight(), kTexFormatRGB24 );
	if (!ConvertImageToPNGFile (*async->image, async->path))
	{
		ErrorString( "Failed to store screen shot" );
	}
#else
	ErrorString("Cannot create pngs");
#endif
	delete async->image;
	delete async;

	return NULL;
}


// --------------------------------------------------------------------------


struct SavedCameraData {
	PPtr<Camera> camera;
	Rectf viewportRect;
};
typedef dynamic_array<SavedCameraData> SavedCameras;

struct SavedTextureData {
	PPtr<Texture> texture;
	float	mipBias;
};
typedef dynamic_array<SavedTextureData> SavedTextures;


static void SaveCameraParams (RenderManager::CameraContainer& cameras, SavedCameras& outCameras)
{
	outCameras.clear();
	for (RenderManager::CameraContainer::iterator camit = cameras.begin(); camit != cameras.end(); ++camit)
	{
		Camera* cam = *camit;
		if (!cam)
			continue;
		SavedCameraData data;
		data.camera = cam;
		data.viewportRect = cam->GetNormalizedViewportRect();
		outCameras.push_back (data);
	}
}

static void RestoreCameraParams (SavedCameras& cameras)
{
	for (SavedCameras::iterator camit = cameras.begin(); camit != cameras.end(); ++camit)
	{
		SavedCameraData& data = *camit;
		Camera* cam = data.camera;
		if (!cam)
			continue;
		cam->SetNormalizedViewportRect (data.viewportRect);
		cam->ResetProjectionMatrix (); ///@TODO
	}
}


static void SaveTextureParams (float mipBias, SavedTextures& outTextures, int& minAniso, int& maxAniso)
{
	vector<Texture*> objects;
	Object::FindObjectsOfType (&objects);
	
	outTextures.clear();
	outTextures.reserve (objects.size());
	
	TextureSettings::GetAnisoLimits (minAniso, maxAniso);
	TextureSettings::SetAnisoLimits (16, 16);	
	
	for (size_t i=0;i<objects.size ();i++)
	{
		Texture* t = objects[i];		
		SavedTextureData data;
		data.texture = t;
		data.mipBias = t->GetSettings().m_MipBias;
		outTextures.push_back (data);
		
		if (t->HasMipMap())
			t->GetSettings().m_MipBias += mipBias;
		t->ApplySettings ();
	}
}

static void RestoreTextureParams (SavedTextures& textures, int minAniso, int maxAniso)
{
	TextureSettings::SetAnisoLimits (minAniso, maxAniso);
	
	for (SavedTextures::iterator it = textures.begin(); it != textures.end(); ++it)
	{
		SavedTextureData& data = *it;
		Texture* t = data.texture;
		if (!t)
			continue;
		t->GetSettings().m_MipBias = data.mipBias;
		t->ApplySettings ();
	}
}


static void ShiftedCameraProjections (SavedCameras& cameras, int superSize, int x, int y, int screenWidth, int screenHeight)
{
	for (SavedCameras::iterator camit = cameras.begin(); camit != cameras.end(); ++camit)
	{
		SavedCameraData& data = *camit;
		Camera* cam = data.camera;
		if (!cam)
			continue;
		cam->ResetProjectionMatrix();
		Rectf camRect = cam->GetScreenViewportRect();
		float dx = (float(x)/float(superSize)-0.5f) / (camRect.width*0.5f);
		float dy = (float(y)/float(superSize)-0.5f) / (camRect.height*0.5f);
		Matrix4x4f proj = cam->GetProjectionMatrix ();
		if (cam->GetOrthographic())
		{
			proj.Get(0,3) -= dx;
			proj.Get(1,3) -= dy;
		}
		else
		{
			proj.Get(0,2) += dx;
			proj.Get(1,2) += dy;
		}
		cam->SetProjectionMatrix (proj);
	}
}

static void InterleaveImage (const Image& screenImage, Image& finalImage, int xo, int yo, int superSize)
{
	Assert (screenImage.GetWidth() * superSize == finalImage.GetWidth());
	Assert (screenImage.GetHeight() * superSize == finalImage.GetHeight());
	Assert (xo >= 0 && xo < superSize);
	Assert (yo >= 0 && yo < superSize);
	
	const UInt32* srcData = reinterpret_cast<const UInt32*>(screenImage.GetImageData());
	UInt32* dstData = reinterpret_cast<UInt32*>(finalImage.GetImageData());
	const int width = screenImage.GetWidth();
	const int height = screenImage.GetHeight();
	dstData += (width*superSize) * yo + xo;
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			dstData[x*superSize] = srcData[x];
		}
		dstData += width*superSize*superSize;
		srcData += width;
	}
}


// --------------------------------------------------------------------------


static Image* DoCaptureScreenshot (int superSize)
{
	GfxDevice& device = GetGfxDevice();
	RenderManager& renderMgr = GetRenderManager();
	
#if UNITY_WP8
	/* In WP8, D3D11 seems to always be in portrait orientation, despite program orientation,
	*  so horizontal and vertical axis swap is needed */
	Rectf rc = renderMgr.GetWindowRect();
	if (GetScreenManager().GetScreenOrientation() == ScreenOrientation::kLandscapeLeft || GetScreenManager().GetScreenOrientation() == ScreenOrientation::kLandscapeRight)
	{
		std::swap(rc.x, rc.y);
		std::swap(rc.width, rc.height);
	}
#else
	const Rectf rc = renderMgr.GetWindowRect();
#endif

	const int screenWidth = int(rc.Width ());
	const int screenHeight = int(rc.Height ());

	Image* finalImage = NULL;
	
	if (superSize > 1)
	{
		finalImage = new Image (screenWidth*superSize, screenHeight*superSize, kTexFormatRGBA32);
		if (!finalImage)
			return NULL;
		
		Image screenImage (screenWidth, screenHeight, kTexFormatRGBA32);
		
		RenderManager::CameraContainer& cameras = renderMgr.GetOnscreenCameras ();
		SavedCameras cameraData;
		SaveCameraParams (cameras, cameraData);

		float mipBias = -Log2(superSize) - 1.0f;
		SavedTextures textureData;
		int savedMinAniso, savedMaxAniso;
		SaveTextureParams (mipBias, textureData, savedMinAniso, savedMaxAniso);
		
		for (int y = 0; y < superSize; ++y)
		{
			for (int x = 0; x < superSize; ++x)
			{
				ShiftedCameraProjections (cameraData, superSize, x, y, screenWidth, screenHeight);
				renderMgr.RenderCameras ();
				#if ENABLE_UNITYGUI
				GetGUIManager().Repaint ();
				#endif
				device.CaptureScreenshot (int(rc.x), int(rc.y), screenWidth, screenHeight, screenImage.GetImageData());
				InterleaveImage (screenImage, *finalImage, x, y, superSize);
			}
		}
		
		RestoreTextureParams (textureData, savedMinAniso, savedMaxAniso);		
		RestoreCameraParams (cameraData);
	}
	else
	{
		finalImage = new Image (screenWidth, screenHeight, kTexFormatRGBA32);
		if (!finalImage)
			return NULL;
		bool ok = device.CaptureScreenshot (int(rc.x), int(rc.y), screenWidth, screenHeight, finalImage->GetImageData());
		if (!ok)
		{
			delete finalImage;
			return NULL;
		}
	}
	return finalImage;
}

bool IsScreenshotQueued()
{
	return (gCaptureScreenshotPath!= NULL);
}

void CaptureScreenshotImmediate(string filePath, int x, int y, int width, int height)
{
	Rectf rc = GetRenderManager().GetWindowRect();
	Image* buffer = new Image (width, height, kTexFormatRGBA32);
	bool ok = GetGfxDevice().CaptureScreenshot( rc.x + x, rc.y + y, width, height, buffer->GetImageData() );
	if( ok )
	{
		WriteImageAsync* asyncData = new WriteImageAsync();
		asyncData->path = filePath;
		asyncData->image = buffer;
		WriteImageAsyncThread(asyncData);		
	}
	else
	{
		delete buffer;
		ErrorString( "Failed to capture screen shot" );
	}
}

void UpdateCaptureScreenshot ()
{
	if (!gCaptureScreenshotPath)
		return;
	
	#if !UNITY_FLASH
	Image* buffer = DoCaptureScreenshot (s_CaptureSuperSize);
	if (buffer)
	{
		WriteImageAsync* asyncData = new WriteImageAsync();
		asyncData->path = gCaptureScreenshotPath;
		asyncData->image = buffer;
		
		#if CAPTURE_SCREENSHOT_THREAD
		gCaptureScreenshotThread.WaitForExit();
		gCaptureScreenshotThread.Run(WriteImageAsyncThread, asyncData);
		#else
		WriteImageAsyncThread(asyncData);		
		#endif
	}
	else
	{
		delete buffer;
		ErrorString( "Failed to capture screen shot" );
	}
	
	#else
	Rectf rc = GetRenderManager().GetWindowRect();
	DBG_LOG_MOLEHILL("\nMH: capture screenshot\n\n");
	GetGfxDevice().CaptureScreenshot (0,0,0,0,NULL); // arguments don't matter here, just needs to ensure clear is done
	AS3Handle ba = Ext_Stage3D_CaptureScreenshot(rc.Width(),rc.Height());
	
	#if ENABLE_PLAYERCONNECTION
	int baLength = Ext_Flash_GetByteArrayLength(ba);
	void* buffer = malloc(baLength);
	Ext_Flash_ReadByteArray(ba,buffer,baLength);
	TransferFileOverPlayerConnection (gCaptureScreenshotPath, buffer, baLength);
	free (buffer);
	#endif
	
	//Let go of bytearray.
	//Ext_MarshalMap_Release(ba);//TODO RH : this byterarray shouldn't be in the marshalmap anyway.
	#endif
	
	free((void*)gCaptureScreenshotPath);
	gCaptureScreenshotPath = NULL;
}

#endif
