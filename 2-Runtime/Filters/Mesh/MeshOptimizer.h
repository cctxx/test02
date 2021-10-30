#pragma once

#ifndef __importmeshoptimizer_h_included__
#define __importmeshoptimizer_h_included__

#include "Runtime/Filters/Mesh/LodMesh.h"

void DeOptimizeIndexBuffers (Mesh& mesh);
void OptimizeIndexBuffers (Mesh& mesh);
void OptimizeReorderVertexBuffer (Mesh& mesh);


#endif	//__importmeshoptimizer_h_included__
