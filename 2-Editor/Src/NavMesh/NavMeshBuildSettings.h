#pragma once
#ifndef NAVMESHBUILDSETTINGS_H
#define NAVMESHBUILDSETTINGS_H

struct NavMeshBuildSettings
{
	NavMeshBuildSettings ()
	{
		agentRadius = 0.5f;
		agentHeight = 2.0f;

		agentSlope = 45.0F;
		agentClimb = 0.4f;

		ledgeDropHeight = 0.0f;
		maxJumpAcrossDistance = 0.0f;

		accuratePlacement = false;

		// Advanced
		minRegionArea = 2.0f;
		widthInaccuracy = 100.0f/6.0f;
		heightInaccuracy = 10.0f;
	}

	float agentRadius;
	float agentHeight;

	float agentSlope;
	float agentClimb; //////@TODO: rename to stepSize??
	float ledgeDropHeight;
	float maxJumpAcrossDistance;

	bool accuratePlacement;

	// Advanced
	float minRegionArea;
	float widthInaccuracy;
	float heightInaccuracy;


	DECLARE_SERIALIZE (NavMeshBuildSettings)
};


template<class TransferFunc>
inline void NavMeshBuildSettings::Transfer (TransferFunc& transfer)
{
	TRANSFER (agentRadius);
	TRANSFER (agentHeight);
	TRANSFER (agentSlope);
	TRANSFER (agentClimb);
	TRANSFER (ledgeDropHeight);
	TRANSFER (maxJumpAcrossDistance);

	TRANSFER (accuratePlacement);
	transfer.Align ();

	TRANSFER (minRegionArea);
	TRANSFER (widthInaccuracy);
	TRANSFER (heightInaccuracy);
}

#endif

