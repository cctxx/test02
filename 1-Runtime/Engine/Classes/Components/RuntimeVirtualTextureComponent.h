// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneComponent.h"
#include "RuntimeVirtualTextureComponent.generated.h"

class URuntimeVirtualTexture;
class UTexture2D;
class UVirtualTextureBuilder;

/** Component used to place a URuntimeVirtualTexture in the world. */
UCLASS(Blueprintable, ClassGroup = Rendering, HideCategories = (Activation, Collision, Cooking, Mobility, LOD, Object, Physics, Rendering))
class ENGINE_API URuntimeVirtualTextureComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

protected:
	/** Actor to align rotation to. If set this actor is always included in the bounds calculation. */
	UPROPERTY(EditAnywhere, Category = TransformFromBounds)
	TSoftObjectPtr<AActor> BoundsAlignActor = nullptr;

	/** Placeholder for details customization button. */
	UPROPERTY(VisibleAnywhere, Transient, Category = TransformFromBounds)
	bool bSetBoundsButton;

	/** If the Bounds Align Actor is a Landscape then this will snap the bounds so that virtual texture texels align with landscape vertex positions. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = TransformFromBounds, meta = (DisplayName = "Snap To Landscape"))
	bool bSnapBoundsToLandscape;

	/** The virtual texture object to use. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, NonPIEDuplicateTransient, Category = VirtualTexture)
	URuntimeVirtualTexture* VirtualTexture = nullptr;

	/** Set to true to enable scalability settings for the virtual texture. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = VirtualTexture, meta = (InlineEditConditionToggle))
	bool bEnableScalability = false;

	/** Group index of the scalability settings to use for the virtual texture. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = VirtualTexture, meta = (UIMin = "0", UIMax = "2", EditCondition = bEnableScalability))
	uint32 ScalabilityGroup = 0;

	/** Hide primitives in the main pass. Hidden primitives will be those that draw to this virtual texture with 'Draw in Main Pass' set to 'From Virtual Texture'. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = VirtualTexture)
	bool bHidePrimitives = false;

	/** Texture object containing streamed low mips. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, NonPIEDuplicateTransient, Category = VirtualTextureBuild)
	UVirtualTextureBuilder* StreamingTexture = nullptr;

	/** Number of low mips to serialize and stream for the virtual texture. This can reduce rendering update cost. */
	UPROPERTY(EditAnywhere, Category = VirtualTextureBuild, meta = (UIMin = "0", UIMax = "12", DisplayName = "Streaming Levels"))
	int32 StreamLowMips = 0;

	/** Placeholder for details customization button. */
	UPROPERTY(VisibleAnywhere, Transient, Category = VirtualTextureBuild)
	bool bBuildStreamingMipsButton;

	/** Enable Crunch texture compression for the streaming low mips. Generic ZLib compression is used when Crunch is disabled. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = VirtualTextureBuild, meta = (DisplayName = "Enable Crunch"))
	bool bEnableCompressCrunch = false;

	/** Use any streaming low mips when rendering in editor. Set true to view and debug the baked streaming low mips. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = VirtualTextureBuild, meta = (DisplayName = "View Streaming Mips in Editor"))
	bool bUseStreamingLowMipsInEditor = false;

	/** Build the streaming low mips using debug coloring. This can help show where streaming mips are being used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Transient, AdvancedDisplay, Category = VirtualTextureBuild, meta = (DisplayName = "Build Debug"))
	bool bBuildDebugStreamingMips = false;

#if WITH_EDITOR
	/** Delegate handle for our function called on PIE end. */
	FDelegateHandle PieEndDelegateHandle;
#endif

	/** Delegate that this virtual texture will call to evaluated the full HidePrimitives state. */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FGetHidePrimitivesDelegate, bool&, bool&);
	FGetHidePrimitivesDelegate HidePrimitivesDelegate;

public:
	/**
	 * This function marks an area of the runtime virtual texture as dirty.
	 * @param WorldBounds : The world space bounds of the pages to invalidate.
	 */
	UFUNCTION(BlueprintCallable, Category = "VirtualTexture")
	void Invalidate(FBoxSphereBounds const& WorldBounds);

	/** Get the runtime virtual texture object on this component. */
	URuntimeVirtualTexture* GetVirtualTexture() const { return VirtualTexture; }

	/** Get if scalability settings are enabled. */
	bool IsScalable() const { return bEnableScalability; }

	/** Get group index of the scalability settings. */
	uint32 GetScalabilityGroup() const { return ScalabilityGroup; }

	/** Get the delegate used to extend the calculation of the HidePrimitives state. */
	FGetHidePrimitivesDelegate& GetHidePrimitivesDelegate() { return HidePrimitivesDelegate; }

	/** Get the full hide primitive state including the evaluating the GetHidePrimitivesDelegate delegate. */
	void GetHidePrimitiveSettings(bool& OutHidePrimitiveEditor, bool& OutHidePrimitiveGame) const;

	/** Get the streaming virtual texture object on this component. */
	UVirtualTextureBuilder* GetStreamingTexture() const { return StreamingTexture; }

	/** Public getter for virtual texture streaming low mips */
	int32 NumStreamingMips() const { return FMath::Clamp(StreamLowMips, 0, 12); }

	/** Get if we want to use any streaming low mips on this component. */
	bool IsStreamingLowMips() const;

	/** Public getter for crunch compression flag. */
	bool IsCrunchCompressed() const { return bEnableCompressCrunch; }

	/** Public getter for debug streaming mips flag. */
	bool IsBuildDebugStreamingMips() { return bBuildDebugStreamingMips; }

	/** Returns true if the StreamingTexure contents are valid for use. */
	bool IsStreamingTextureValid() const;

#if WITH_EDITOR
	/** Set a new asset to hold the low mip streaming texture. This should only be called directly before setting data to the new asset. */
	void SetStreamingTexture(UVirtualTextureBuilder* InTexture) { StreamingTexture = InTexture; }
	/** Initialize the low mip streaming texture with the passed in size and data. */
	void InitializeStreamingTexture(uint32 InSizeX, uint32 InSizeY, uint8* InData);
#endif

#if WITH_EDITOR
	/** Get the BoundsAlignActor on this component. */
	TSoftObjectPtr<AActor>& GetBoundsAlignActor() { return BoundsAlignActor; }
	/** Get if SnapBoundsToLandscape is set on this component. */
	bool GetSnapBoundsToLandscape() const { return bSnapBoundsToLandscape; }
#endif
	/** Get a translation to account for any vertex sample offset from the use of bSnapBoundsToLandscape. */
	FTransform GetTexelSnapTransform() const;

protected:
	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif
	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) override;
	//~ End UObject Interface

	//~ Begin UActorComponent Interface
	virtual void CreateRenderState_Concurrent(FRegisterComponentContext* Context) override;
	virtual void SendRenderTransform_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;
#if WITH_EDITOR
	virtual void CheckForErrors() override;
#endif
	//~ End UActorComponent Interface

	//~ Begin USceneComponent Interface
#if WITH_EDITOR
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
#endif
	virtual bool IsVisible() const override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End USceneComponent Interface

protected:
	/** Calculate a hash used to determine if the StreamingTexture contents are valid for use. The hash doesn't include whether the contents are up to date. */
	uint64 CalculateStreamingTextureSettingsHash() const;

public:
	/** Scene proxy object. Managed by the scene but stored here. */
	class FRuntimeVirtualTextureSceneProxy* SceneProxy;
};
