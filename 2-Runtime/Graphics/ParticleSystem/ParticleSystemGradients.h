#ifndef SHURIKENGRADIENTS_H
#define SHURIKENGRADIENTS_H

#include "Runtime/Math/Gradient.h"

enum MinMaxGradientEvalMode
{
	kGEMGradient,
	kGEMGradientMinMax,
	kGEMSlow,
};

enum MinMaxGradientState
{
	kMMGColor = 0, 
	kMMGGradient = 1, 
	kMMGRandomBetweenTwoColors = 2,  
	kMMGRandomBetweenTwoGradients = 3
};

struct OptimizedMinMaxGradient
{
	OptimizedGradient max;
	OptimizedGradient min;
};

inline ColorRGBA32 EvaluateGradient (const OptimizedMinMaxGradient& g, float t)
{
	return g.max.Evaluate(t);
}

inline ColorRGBA32 EvaluateRandomGradient (const OptimizedMinMaxGradient& g, float t, UInt32 factor)
{ 
	return Lerp (g.min.Evaluate(t), g.max.Evaluate(t), factor);
}

struct MinMaxGradient
{
	GradientNEW maxGradient;
	GradientNEW minGradient;
	ColorRGBA32 minColor;	// we have the colors separate to prevent destroying the gradients
	ColorRGBA32 maxColor;
	short minMaxState;		// see enum State

	MinMaxGradient();

	DEFINE_GET_TYPESTRING (MinMaxGradient)

	void InitializeOptimized(OptimizedMinMaxGradient& g);

	template<class TransferFunction>
	void Transfer (TransferFunction& transfer);
};

inline ColorRGBA32 EvaluateColor (const MinMaxGradient& gradient)
{
	return gradient.maxColor;
}

inline ColorRGBA32 EvaluateGradient (const MinMaxGradient& gradient, float t)
{
	return gradient.maxGradient.Evaluate(t);
}

inline ColorRGBA32 EvaluateRandomColor (const MinMaxGradient& gradient, UInt32 factor)
{
	return Lerp (gradient.minColor, gradient.maxColor, factor);
}

inline ColorRGBA32 EvaluateRandomGradient (const MinMaxGradient& gradient, float t, UInt32 factor)
{ 
	return Lerp (gradient.minGradient.Evaluate(t), gradient.maxGradient.Evaluate(t), factor);
}

inline ColorRGBA32 Evaluate (const MinMaxGradient& gradient, float t, UInt32 factor = 0xff)
{
	if (gradient.minMaxState == kMMGColor)
		return EvaluateColor(gradient);
	else if (gradient.minMaxState == kMMGGradient)
		return EvaluateGradient(gradient, t);
	else if (gradient.minMaxState == kMMGRandomBetweenTwoColors)
		return EvaluateRandomColor(gradient, factor);
	else // gradient.minMaxState == kMMGRandomBetweenTwoGradients 
		return EvaluateRandomGradient(gradient, t, factor);
}

#endif // SHURIKENGRADIENTS_H
