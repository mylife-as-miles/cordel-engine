// Copyright (c) 2026 CORDEL contributors. MIT.
#pragma once
#include "cordel/logic.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace cordel {
struct Vertex { float position[3],normal[3]; };
struct Mesh {
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<unsigned> indices;
    Mat4 model{identity};
    std::array<float,4> color{1,1,1,1};
};
struct SceneData {
    std::vector<Mesh> meshes;
    std::size_t triangles() const;
};
SceneData load_scene(const std::filesystem::path& path);
}
