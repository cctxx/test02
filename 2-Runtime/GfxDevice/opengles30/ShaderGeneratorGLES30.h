#ifndef SHADERGENERATOR_GLES30_H
#define SHADERGENERATOR_GLES30_H

#include <string>

class FixedFunctionStateGLES30;

std::string BuildVertexShaderSourceGLES30 (const FixedFunctionStateGLES30& state);
std::string BuildFragmentShaderSourceGLES30 (const FixedFunctionStateGLES30& state);

struct FullStateCompareGLES30
{
	bool operator() (FixedFunctionStateGLES30 const& a, FixedFunctionStateGLES30 const& b) const;
};
struct VertexStateCompareGLES30
{
	bool operator() (FixedFunctionStateGLES30 const& a, FixedFunctionStateGLES30 const& b) const;
};
struct FragmentStateCompareGLES30
{
	bool operator() (FixedFunctionStateGLES30 const& a, FixedFunctionStateGLES30 const& b) const;
};

#endif /* SHADERGENERATOR_GLES30_H */
