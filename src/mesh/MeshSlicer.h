#pragma once 
#include "Mesh.h"
#include "../Plane.h"

bool SliceMesh(const Mesh& src, const Plane& plane, Mesh& outPos, Mesh& outNeg);
