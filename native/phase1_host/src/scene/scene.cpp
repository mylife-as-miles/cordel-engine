// Copyright (c) 2026 CORDEL contributors. MIT.
#include "cordel/scene.hpp"
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include <memory>

namespace cordel {
std::size_t SceneData::triangles() const {
    std::size_t n=0;
    for(const auto& mesh:meshes) n+=mesh.indices.size()/3;
    return n;
}
SceneData load_scene(const std::filesystem::path& path) {
    cgltf_options options{};
    cgltf_data* raw{};
    auto result=cgltf_parse_file(&options,path.string().c_str(),&raw);
    if(result!=cgltf_result_success) throw std::runtime_error("glTF parse failed: "+std::to_string(result));
    std::unique_ptr<cgltf_data,decltype(&cgltf_free)> data(raw,cgltf_free);
    if(cgltf_load_buffers(&options,data.get(),path.string().c_str())!=cgltf_result_success
       ||cgltf_validate(data.get())!=cgltf_result_success) throw std::runtime_error("Invalid glTF buffers");
    if(!data->scene) throw std::runtime_error("glTF has no default scene");
    SceneData scene;
    // Fixture-only loader: unsupported content fails explicitly instead of
    // silently pretending to support an animation/material/asset pipeline.
    for(std::size_t i=0;i<data->scene->nodes_count;++i) {
        const auto& node=*data->scene->nodes[i];
        if(!node.mesh||node.children_count||node.skin||node.has_rotation||node.has_scale||node.has_matrix)
            throw std::runtime_error("Spike requires flat translation-only mesh nodes");
        cgltf_float column_major[16];
        cgltf_node_transform_world(&node,column_major);
        Mat4 model{};
        // Sole import boundary: glTF column-major storage -> CPU row-major.
        // glTF's RH/+Y-up/metre convention already matches CORDEL; no reflection.
        for(int r=0;r<4;++r) for(int c=0;c<4;++c) model[r*4+c]=column_major[c*4+r];
        for(std::size_t j=0;j<node.mesh->primitives_count;++j) {
            const auto& p=node.mesh->primitives[j];
            if(p.type!=cgltf_primitive_type_triangles||!p.indices||p.targets_count)
                throw std::runtime_error("Spike requires indexed static triangles");
            const cgltf_accessor *positions=nullptr,*normals=nullptr;
            for(std::size_t k=0;k<p.attributes_count;++k) {
                if(p.attributes[k].type==cgltf_attribute_type_position) positions=p.attributes[k].data;
                if(p.attributes[k].type==cgltf_attribute_type_normal) normals=p.attributes[k].data;
            }
            if(!positions||!normals||positions->count!=normals->count||positions->is_sparse||normals->is_sparse)
                throw std::runtime_error("Missing/unsupported positions or normals");
            Mesh mesh;
            mesh.name=node.name?node.name:"unnamed";
            mesh.model=model;
            if(!p.material||!p.material->has_pbr_metallic_roughness||p.material->pbr_metallic_roughness.base_color_texture.texture)
                throw std::runtime_error("Spike requires solid base-color materials");
            std::copy_n(p.material->pbr_metallic_roughness.base_color_factor,4,mesh.color.begin());
            mesh.vertices.resize(positions->count);
            for(std::size_t k=0;k<positions->count;++k) {
                if(!cgltf_accessor_read_float(positions,k,mesh.vertices[k].position,3)
                   ||!cgltf_accessor_read_float(normals,k,mesh.vertices[k].normal,3))
                    throw std::runtime_error("glTF vertex read failed");
            }
            if(p.indices->count%3) throw std::runtime_error("Non-triangle index count");
            for(std::size_t k=0;k<p.indices->count;++k) {
                auto index=cgltf_accessor_read_index(p.indices,k);
                if(index>=positions->count||index>std::numeric_limits<unsigned>::max())
                    throw std::runtime_error("glTF index out of range");
                mesh.indices.push_back(static_cast<unsigned>(index));
            }
            scene.meshes.push_back(std::move(mesh));
        }
    }
    return scene;
}
}
