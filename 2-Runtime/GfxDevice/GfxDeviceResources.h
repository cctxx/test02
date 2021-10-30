#pragma once

#include "GfxDeviceTypes.h"

template<typename T> 
struct memcmp_less 
{
	bool operator () (const T& lhs, const T& rhs) const 
	{
		return memcmp(&lhs, &rhs, sizeof(T)) < 0;
	}
};


struct GfxBlendState
{
	BlendMode	srcBlend;
	BlendMode	dstBlend;
	BlendMode	srcBlendAlpha;
	BlendMode	dstBlendAlpha;
	BlendOp		blendOp;
	BlendOp		blendOpAlpha;
	UInt32		renderTargetWriteMask;
	CompareFunction alphaTest;
	bool		alphaToMask;

	GfxBlendState()
	{
		memset(this, 0, sizeof(*this));
		srcBlend = kBlendOne;
		dstBlend = kBlendZero;
		srcBlendAlpha = kBlendOne;
		dstBlendAlpha = kBlendZero;
		blendOp = kBlendOpAdd;
		blendOpAlpha = kBlendOpAdd;
		renderTargetWriteMask = KColorWriteAll;
		alphaTest = kFuncDisabled;
		alphaToMask = false;
	}
};


struct GfxRasterState
{
	CullMode	cullMode;
	int			depthBias;
	float		slopeScaledDepthBias;

	GfxRasterState()
	{
		memset(this, 0, sizeof(*this));
		cullMode = kCullBack;
		depthBias = 0;
		slopeScaledDepthBias = 0.0f;
	}
};


struct GfxDepthState
{
	bool			depthWrite;
	CompareFunction depthFunc;

	GfxDepthState()
	{
		memset(this, 0, sizeof(*this));
		depthWrite = true;
		depthFunc = kFuncLess;
	}
};

struct GfxStencilState
{
	bool			stencilEnable;
	UInt8			readMask;
	UInt8			writeMask;
	CompareFunction	stencilFuncFront;
	StencilOp		stencilPassOpFront; // stencil and depth pass
	StencilOp		stencilFailOpFront; // stencil fail (depth irrelevant)
	StencilOp		stencilZFailOpFront; // stencil pass, depth fail
	CompareFunction	stencilFuncBack;
	StencilOp		stencilPassOpBack;
	StencilOp		stencilFailOpBack;
	StencilOp		stencilZFailOpBack;

	GfxStencilState()
	{
		memset(this, 0, sizeof(*this));
		stencilEnable = false;
		readMask = 0xFF;
		writeMask = 0xFF;
		stencilFuncFront = kFuncAlways;
		stencilFailOpFront = kStencilOpKeep;
		stencilZFailOpFront = kStencilOpKeep;
		stencilPassOpFront = kStencilOpKeep;
		stencilFuncBack = kFuncAlways;
		stencilFailOpBack = kStencilOpKeep;
		stencilZFailOpBack = kStencilOpKeep;
		stencilPassOpBack = kStencilOpKeep;
	}
};

struct DeviceBlendState
{
	DeviceBlendState(const GfxBlendState& src) : sourceState(src) {}
	DeviceBlendState() {}
	GfxBlendState sourceState;
};

struct DeviceDepthState
{
	DeviceDepthState(const GfxDepthState& src) : sourceState(src) {}
	DeviceDepthState() {}
	GfxDepthState sourceState;
};

struct DeviceStencilState
{
	DeviceStencilState(const GfxStencilState& src) : sourceState(src) {}
	DeviceStencilState() {}
	GfxStencilState sourceState;
};

struct DeviceRasterState
{
	DeviceRasterState(const GfxRasterState& src) : sourceState(src) {}
	DeviceRasterState() {}
	GfxRasterState sourceState;
};
