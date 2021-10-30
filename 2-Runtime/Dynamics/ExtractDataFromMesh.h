#pragma once

#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Math/Vector3.h"

class Mesh;

bool ExtractDataFromMesh (Mesh& mesh, dynamic_array<Vector3f>& vertices, dynamic_array<UInt16>& triangles, dynamic_array<UInt16>& remap);
