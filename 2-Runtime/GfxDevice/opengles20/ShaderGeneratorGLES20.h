#ifndef SHADERGENERATOR_GLES20_H
#define SHADERGENERATOR_GLES20_H

#include <string>

class FixedFunctionStateGLES20;

std::string BuildVertexShaderSourceGLES20 (const FixedFunctionStateGLES20& state);
std::string BuildFragmentShaderSourceGLES20 (const FixedFunctionStateGLES20& state);

struct FullStateCompareGLES20
{
	bool operator() (FixedFunctionStateGLES20 const& a, FixedFunctionStateGLES20 const& b) const;
};
struct VertexStateCompareGLES20
{
	bool operator() (FixedFunctionStateGLES20 const& a, FixedFunctionStateGLES20 const& b) const;
};
struct FragmentStateCompareGLES20
{
	bool operator() (FixedFunctionStateGLES20 const& a, FixedFunctionStateGLES20 const& b) const;
};

#endif /* SHADERGENERATOR_GLES20_H */
