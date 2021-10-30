#include "UnityPrefix.h"
#include "ParticleSystemGradients.h"
#include "Runtime/BaseClasses/ObjectDefines.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"

MinMaxGradient::MinMaxGradient()
:	minMaxState (kMMGColor), minColor (255,255,255,255), maxColor (255,255,255,255)
{
}

void MinMaxGradient::InitializeOptimized(OptimizedMinMaxGradient& g)
{
	maxGradient.InitializeOptimized(g.max);
	if(minMaxState == kMMGRandomBetweenTwoGradients)
		minGradient.InitializeOptimized(g.min);
}

template<class TransferFunction>
void MinMaxGradient::Transfer (TransferFunction& transfer)
{
	TRANSFER (maxGradient);
	TRANSFER (minGradient);
	TRANSFER (minColor);
	TRANSFER (maxColor);
	TRANSFER (minMaxState); transfer.Align ();
}	
INSTANTIATE_TEMPLATE_TRANSFER(MinMaxGradient)
