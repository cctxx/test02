#pragma once

class LODGroup;

void CalculateLODGroupBoundingBox (LODGroup& group);

void ForceLODLevel (const LODGroup& group, int index);
