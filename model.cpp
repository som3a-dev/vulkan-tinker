#include "model.hpp"

void Model::load_mesh(const tinyobj::shape_t& shape)
{
    // TODO(omar): the unique vertex check is O(N)
    std::unordered_map<vertex_t, uint32_t> uniqueVertices{};
    std::cout << shape.name << std::endl;
    for (const tinyobj::index_t& index : shape.mesh.indices)
    {
        vertex_t vertex{};

        vertex.pos.x = attrib.vertices[3 * index.vertex_index + 0];
        vertex.pos.y = attrib.vertices[3 * index.vertex_index + 1];
        vertex.pos.z = attrib.vertices[3 * index.vertex_index + 2];

        vertex.tex_coord.x = attrib.texcoords[2 * index.texcoord_index + 0];
        vertex.tex_coord.y = 1.0f - attrib.texcoords[2 * index.texcoord_index + 1];

        vertex.color = {1.0f, 1.0f, 1.0f};

        if (uniqueVertices.count(vertex) == 0)
        {
            uniqueVertices[vertex] = (uint32_t)(vertices.size());
            vertices.push_back(vertex);
        }

        indices.push_back(uniqueVertices[vertex]);
    }
}

void Model::load_mat(const tinyobj::material_t& mat)
{
    // std::string path = "assets/workshop/" + mat.diffuse_texname;

    // app.create_texture(path.c_str(), texture_image, texture_image_view,
    // texture_image_memory);
}