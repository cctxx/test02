#include "UnityPrefix.h"
#include "ProceduralTexture.h"
#include "SubstanceArchive.h"
#include "ProceduralMaterial.h"
#include "Image.h"
#include "SubstanceSystem.h"
#include "Texture2D.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Math/Color.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "Runtime/Serialize/PersistentManager.h"
#include "Runtime/Graphics/S3Decompression.h"
#include "Runtime/File/ApplicationSpecificPersistentDataPath.h"
#include "External/shaderlab/Library/shaderlab.h"
#include "External/shaderlab/Library/properties.h"

using namespace std;

ProceduralTexture::TextureParameters::TextureParameters() :
	width (0),
	height(0),
	mipLevels(0),
	textureFormat(kTexFormatTotalCount)
{
}

ProceduralTexture::TextureParameters::TextureParameters(int inWidth, int inHeight, int inMipLevels, TextureFormat inFormat) :
	width (inWidth),
	height(inHeight),
	mipLevels(inMipLevels),
	textureFormat(inFormat)
{
}

IMPLEMENT_CLASS( ProceduralTexture )
IMPLEMENT_OBJECT_SERIALIZE( ProceduralTexture )

ProceduralTexture::ProceduralTexture( MemLabelId label, ObjectCreationMode mode ) :
	Super( label, mode ),
	m_SubstanceTextureUID( 0 ),
	m_Type(Substance_OType_Unknown),
	m_AlphaSource(Substance_OType_Unknown),
	m_Flags(0),
	m_UploadState(UploadState_None),
	m_Format(Substance_OFormat_Compressed),
	m_PingedMaterial(NULL)
{
}

ProceduralTexture::~ProceduralTexture()
{
	// Update pointer
	if (m_PingedMaterial==NULL)
		m_PingedMaterial = dynamic_pptr_cast<ProceduralMaterial*>(Object::IDToPointer(m_SubstanceMaterial.GetInstanceID()));

#if ENABLE_SUBSTANCE
	/////@TODO: This is an incredible hack. Waiting for another process to complete in the destructor is a big no no, especially when it involves a Thread::Sleep ()...
	UnlockObjectCreation();
    GetSubstanceSystem ().NotifyTextureDestruction(this);
	LockObjectCreation();
#endif
	
	RemoveTexture ();
}

#if ENABLE_SUBSTANCE
ProceduralTexture* ProceduralTexture::Clone(ProceduralMaterial* owner)
{
	ProceduralTexture* clone = CreateObjectFromCode<ProceduralTexture>();
	clone->m_SubstanceMaterial = owner;
	clone->m_PingedMaterial = owner;
	clone->m_SubstanceTextureUID = m_SubstanceTextureUID;
	clone->m_Type = m_Type;
	clone->m_AlphaSource = m_AlphaSource;
	clone->m_Format = m_Format;
	clone->m_SubstanceFormat = m_SubstanceFormat;
	clone->m_ClonedID = GetTextureID();
	clone->SetName(GetName());
	clone->EnableFlag(Flag_AwakeClone, true);
	clone->SetUsageMode(GetUsageMode());
	return clone;
}

void ProceduralTexture::AwakeClone()
{
	// Update material texenvs
    const ShaderLab::PropertySheet::TexEnvs& textureProperties = GetSubstanceMaterial()->GetProperties().GetTexEnvsMap();
    for (ShaderLab::PropertySheet::TexEnvs::const_iterator i=textureProperties.begin();i!=textureProperties.end();++i)
    {
        if (i->second.texEnv->GetAssignedTextureID()==m_ClonedID)
        {
		    GetSubstanceMaterial()->SetTexture(i->first, this);
        }
    }
    EnableFlag(Flag_AwakeClone, false);
}
#endif

#if UNITY_EDITOR
void ProceduralTexture::Init( ProceduralMaterial& _Parent, int substanceTextureUID, ProceduralOutputType type, SubstanceOutputFormat format, ProceduralOutputType alphaSource, bool requireCompressed )
{
	m_SubstanceMaterial = &_Parent;
	m_SubstanceTextureUID = ((UInt64)substanceTextureUID)<<32;
	m_Type = type;
	m_Format = format;
	m_TextureParameters.textureFormat = GetSubstanceTextureFormat(format, requireCompressed);
	m_AlphaSource = alphaSource;
	m_Flags = 0;
	
	AwakeFromLoad(kDefaultAwakeFromLoad);
}
#endif

bool ProceduralTexture::IsBaked() const
{
	return m_BakedParameters.IsValid() && m_BakedData.size()>0;
}

bool ProceduralTexture::GetPixels32(int x, int y, int width, int height, ColorRGBA32* data)
{
	if (m_Format != Substance_OFormat_Raw)
	{
		WarningStringMsg("Substance %s should be set to RAW in order to use GetPixels32 on its texture outputs.", m_PingedMaterial->GetName());
		return false;
	}
	if (! m_PingedMaterial->IsFlagEnabled(ProceduralMaterial::Flag_Readable))
	{
		WarningStringMsg("The isReadable property of Substance %s should be set to true in order to use GetPixels32 on its texture outputs.", m_PingedMaterial->GetName());
		return false;
	}
	const ProceduralTexture::TextureParameters& parameters = GetBakedParameters();
	if (GetBakedData().size()==0 || (parameters.textureFormat!=kTexFormatRGBA32 && parameters.textureFormat!=kTexFormatARGB32))
		return false;
	ImageReference rawTexture(parameters.width, parameters.height, GetRowBytesFromWidthAndFormat(parameters.width, parameters.textureFormat), parameters.textureFormat, &GetBakedData()[0]);
	ImageReference resultImage(parameters.width, parameters.height, GetRowBytesFromWidthAndFormat(parameters.width, kTexFormatRGBA32), kTexFormatRGBA32, data);
	resultImage.BlitImage(rawTexture, ImageReference::BLIT_COPY);
	return true;
}

void ProceduralTexture::Invalidate()
{
	m_TextureParameters.Invalidate();
}

void ProceduralTexture::UnloadFromGfxDevice (bool forceUnloadAll)
{
	RemoveTexture();
}

void ProceduralTexture::UploadToGfxDevice ()
{
	if (!m_BakedParameters.IsValid())
		return;
	if (m_BakedData.size()==0)
	{
		GetPersistentManager().ReloadFromDisk(this);
	}
	else
	{
		UploadBakedTexture();
	}
}

void ProceduralTexture::RemoveTexture ()
{
	if (IsFlagEnabled (Flag_Uploaded))
	{
		GetGfxDevice().DeleteTexture (GetTextureID());
		EnableFlag(Flag_Uploaded, false);
		m_UploadState = UploadState_None;
	}
}

void ProceduralTexture::UploadWaitingTexture ()
{
	RemoveTexture ();
	// Upload blue texture to show the substance is waiting generation
	UInt8 bluePixel[] = { 255, 0, 0, 255 };
	GetGfxDevice().UploadTexture2D( GetTextureID(), GetDimension(), bluePixel, sizeof(bluePixel),
			1, 1, kTexFormatARGB32,	1, true, 0 /* \note We upload only one level so can't skip any */,
			GetUsageMode(), kTexColorSpaceLinear);
	Texture::s_TextureIDMap.insert (std::make_pair(GetTextureID(),this));
	EnableFlag(Flag_Uploaded, true);
	m_UploadState = UploadState_Waiting;
	m_TextureSettings.Apply( GetTextureID(), GetDimension(), false, kTexColorSpaceLinear );
}

void ProceduralTexture::UploadBakedTexture ()
{
	RemoveTexture ();
	Assert(m_BakedData.size()>0);
	GetGfxDevice().UploadTexture2D( GetTextureID(), GetDimension(), &m_BakedData[0], m_BakedData.size(),
		m_BakedParameters.width, m_BakedParameters.height, m_BakedParameters.textureFormat,
		m_BakedParameters.mipLevels, true, 0 /* \note We upload only one level so can't skip any */,
		GetUsageMode(), GetActiveTextureColorSpace());
	Texture::s_TextureIDMap.insert (std::make_pair(GetTextureID(),this));
	EnableFlag(Flag_Uploaded, true);
	m_UploadState = UploadState_Baked;
	m_TextureSettings.Apply( GetTextureID(), GetDimension(), m_BakedParameters.mipLevels != 1, GetActiveTextureColorSpace() );
	m_TextureParameters = m_BakedParameters;

#if !UNITY_EDITOR
	m_BakedData.clear();
#endif
}

void ProceduralTexture::SetSubstanceShuffledUID(unsigned int textureUID)
{
	m_SubstanceTextureUID &= ((UInt64)0xffffffff) << 32;
	m_SubstanceTextureUID |= (UInt64)textureUID;
}

void ProceduralTexture::AwakeFromLoadThreaded()
{
    Super::AwakeFromLoadThreaded();

    // Update format before it gets packed & generated
    m_SubstanceFormat = GetSubstanceTextureFormat(m_Format);

#if UNITY_ANDROID || UNITY_IPHONE
	// It is more practical to do this check at runtime since for Substances we control
	// the way we generate the normal maps. Doing this here instead of in the editor avoids
	// the reimports that would be associated with platform switches (non-mobile <-> mobile).
	if (m_UsageMode == kTexUsageNormalmapDXT5nm)
	{
		// Override normal format
		m_UsageMode = kTexUsageNormalmapPlain;
	}
#endif
}

void ProceduralTexture::AwakeFromLoad( AwakeFromLoadMode awakeMode )
{
	Super::AwakeFromLoad( awakeMode );
	
    // Force loading of material
    if ((awakeMode & kDidLoadThreaded) == 0)
	{
		ProceduralMaterial* material = m_SubstanceMaterial;
		if (material==NULL)
		{
			// This happens
		}
	}

	if (IsBaked())
	{
		if (m_UploadState<UploadState_Valid)
			UploadBakedTexture();
	}
	else
	{
		if (m_UploadState==UploadState_None)
			UploadWaitingTexture();
	}

	m_SubstanceFormat = GetSubstanceTextureFormat(m_Format);

#if UNITY_ANDROID || UNITY_IPHONE
	// It is more practical to do this check at runtime since for Substances we control
	// the way we generate the normal maps. Doing this here instead of in the editor avoids
	// the reimports that would be associated with platform switches (non-mobile <-> mobile).
	if (m_UsageMode == kTexUsageNormalmapDXT5nm)
	{
		// Override normal format
		m_UsageMode = kTexUsageNormalmapPlain;
	}
#endif
}

void ProceduralTexture::UploadSubstanceTexture(SubstanceTexture& outputTexture)
{
#if ENABLE_SUBSTANCE
	// Check if it's full pyramid
	if (outputTexture.mipmapCount==0)
	{
		outputTexture.mipmapCount = CalculateMipMapCount3D(outputTexture.level0Width, outputTexture.level0Height, 1);
	}

	// Check if we can replace the existing texture data
	TextureParameters state (outputTexture.level0Width, outputTexture.level0Height, outputTexture.mipmapCount, m_SubstanceFormat);
	
	bool reuseTextureMemory = (state==m_TextureParameters);
	if (!reuseTextureMemory)
		RemoveTexture ();

	int bufferSize = CalculateMipMapOffset(state.width, state.height, state.textureFormat, state.mipLevels+1);
	GetGfxDevice().UploadTexture2D( GetTextureID(), GetDimension(), reinterpret_cast<UInt8*> (outputTexture.buffer), bufferSize,
		state.width, state.height, state.textureFormat, state.mipLevels, !reuseTextureMemory, min(state.mipLevels-1, Texture::GetMasterTextureLimit()),
		GetUsageMode(), GetActiveTextureColorSpace());
	Texture::s_TextureIDMap.insert (std::make_pair(GetTextureID(),this));
	EnableFlag(Flag_Uploaded, true);	
	m_TextureParameters = state;

	// Handle readable flag
	if (GetSubstanceMaterial()!=NULL && GetSubstanceMaterial()->IsFlagEnabled(ProceduralMaterial::Flag_Readable)
		&& (state.textureFormat==kTexFormatRGBA32 || state.textureFormat==kTexFormatARGB32))
	{
		size_t size = state.width*state.height*4;
		m_BakedData.resize(size);
		memcpy(&m_BakedData[0], outputTexture.buffer, size);
		m_BakedParameters = state;
	}

	if (IsFlagEnabled(Flag_AwakeClone))
		AwakeClone();
	
	m_TextureSettings.Apply( GetTextureID(), GetDimension(), outputTexture.mipmapCount != 1, GetActiveTextureColorSpace() );
	m_UploadState = UploadState_Generated;
#endif
}

void ProceduralTexture::SetOwner(ProceduralMaterial* material)
{
    Assert (material->IsFlagEnabled(ProceduralMaterial::Flag_Clone)
        || m_PingedMaterial==NULL || m_PingedMaterial==material);
    if (m_PingedMaterial==NULL)
	    m_PingedMaterial = material;
}

template<class TransferFunction>
void ProceduralTexture::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);

	TRANSFER( m_SubstanceMaterial );
	TRANSFER( m_SubstanceTextureUID );
	transfer.Transfer(reinterpret_cast<int&> (m_Type), "Type");
	transfer.Transfer(reinterpret_cast<int&> (m_AlphaSource), "AlphaSource");
	transfer.Transfer(reinterpret_cast<int&> (m_Format), "Format");

	if (m_Format<0 || m_Format>1)
		m_Format = Substance_OFormat_Compressed;

	TRANSFER( m_TextureSettings );

	if (transfer.IsBuildingTargetPlatform(kBuildXBOX360))
	{
		const size_t size = m_BakedData.size();
		std::vector<UInt8> swappedData;
		swappedData.resize(size);
		if (m_Format == Substance_OFormat_Compressed)
		{
			// Compressed Substance textures are DXT : 16b byte-swap needed
			for (int i=0 ; i<size/2 ; ++i)
			{
				swappedData[2*i+0] = m_BakedData[2*i+1];
				swappedData[2*i+1] = m_BakedData[2*i+0];
			}
		}
		else
		{
			// RAW Substance textures = 4Bpp : 32b byte-swap needed
			for (int i=0 ; i<size/4 ; ++i)
			{
				swappedData[4*i+0] = m_BakedData[4*i+3];
				swappedData[4*i+1] = m_BakedData[4*i+2];
				swappedData[4*i+2] = m_BakedData[4*i+1];
				swappedData[4*i+3] = m_BakedData[4*i+0];
			}
		}
		TRANSFER( swappedData );
	}
	else
	{
		TRANSFER( m_BakedData );
	}

	TRANSFER( m_BakedParameters );
	transfer.Transfer( m_UsageMode, "m_LightmapFormat");
	transfer.Transfer( m_ColorSpace, "m_ColorSpace");
}
