#pragma once

#include <directxtk12/SimpleMath.h>
#include <string>
#include <vector>

#include "Vertex.h"

struct MeshData {
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;
    std::string           albedoTexturePath;
};