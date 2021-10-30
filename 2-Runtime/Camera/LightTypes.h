#pragma once

struct ActiveLight;

struct ForwardLightsBlock
{
	float sh[9][3];
	const ActiveLight* mainLight;
	int addLightCount;
	int vertexLightCount;
	float lastAddLightBlend;
	float lastVertexLightBlend;
	// followed by ActiveLight pointers; additive lights first, then vertex lights

	const ActiveLight* const* GetLights() const {
		return reinterpret_cast<const ActiveLight* const*>( reinterpret_cast<const UInt8*>(this) + sizeof(ForwardLightsBlock) );
	}
};

struct VertexLightsBlock
{
	int lightCount;
	// followed by ActiveLight pointers

	const ActiveLight* const* GetLights() const {
		return reinterpret_cast<const ActiveLight* const*>( reinterpret_cast<const UInt8*>(this) + sizeof(VertexLightsBlock) );
	}
};
