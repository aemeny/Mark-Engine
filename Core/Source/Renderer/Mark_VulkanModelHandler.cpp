#include "Mark_VulkanModelHandler.h"
#include "Renderer/Mark_VulkanCore.h"
#include "Renderer/Mark_VulkanVertexBuffer.h"
#include "Utils/Mark_Utils.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <unordered_map>

#ifndef TINYOBJLOADER_IMPLEMENTATION
    #define TINYOBJLOADER_IMPLEMENTATION
#endif
#include "Include/objloader/tiny_obj_loader.h"

// Turns single Vertex into a hash at size_t in order to use it in unordered_map
namespace std
{
    template<>struct hash<Mark::RendererVK::VertexData>
    {
        size_t operator()(Mark::RendererVK::VertexData const& _vertex) const
        {
            size_t seed = 0;
            Mark::Utils::hashCombine(seed, _vertex.m_position, _vertex.m_colour, _vertex.m_normal, _vertex.m_uv);
            return seed;
        }
    };
}

namespace Mark::RendererVK
{
    MeshHandler::MeshHandler(std::weak_ptr<VulkanCore> _vulkanCore, VulkanCommandBuffers& _commandBuffersRef) :
        m_vulkanCore(_vulkanCore)
    {
        const auto assetPath = _vulkanCore.lock()->assetPath("Textures/Curuthers.png"); // Test cat texture
        m_texture = new TextureHandler(_vulkanCore, &_commandBuffersRef, assetPath.string().c_str());
    }

    MeshHandler::~MeshHandler()
    {
        auto VkCore = m_vulkanCore.lock();
        if (!VkCore) {
            MARK_ERROR("VulkanCore is null for mesh destruction");
        }

        destroyGPUBuffer(VkCore->device());

        if (m_texture) {
            m_texture->destroyTextureHandler(VkCore->device());
            delete m_texture;
            m_texture = nullptr;
        }
    }

    void MeshHandler::uploadToGPU()
    {
        auto VkCore = m_vulkanCore.lock();
        if (!VkCore) {
            MARK_ERROR("VulkanCore is null for mesh upload");
        }
        if (m_vertices.empty() && !m_usingFallBack) {
            MARK_WARN_C(Utils::Category::Vulkan, "uploadToGPU called with empty vertex list");
        }

        const VkDeviceSize vertexSize = static_cast<VkDeviceSize>(vertexBufferSize());
        const VkDeviceSize indexSize = static_cast<VkDeviceSize>(indexBufferSize());

        // Device local buffer creation from CPU data
        m_vertexBuffer = VkCore->vertexUploader().createDeviceLocalFromCPU(
            VkCore,
            m_vertices.data(),
            vertexSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT // STORAGE_BUFFER for programmable vertex pulling
        );

        if (indexSize > 0)
        {
            m_indexBuffer = VkCore->vertexUploader().createDeviceLocalFromCPU(
                VkCore,
                m_indices.data(),
                indexSize,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            );
        }

        MARK_INFO_C(Utils::Category::Vulkan, "Mesh uploaded: %u vertices, %u indices",
            vertexCount(), indexCount());
    }

    void MeshHandler::loadFromOBJ(const char* _meshPath, bool _flipV)
    {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, _meshPath))
        {
            // Strip trailing newlines for cleaner logging
            auto StripTrailingNewlines = [](std::string& _s)
            {
                while (!_s.empty() && (_s.back() == '\n' || _s.back() == '\r')) {
                    _s.pop_back();
                }
            };
            StripTrailingNewlines(warn);
            StripTrailingNewlines(err);

            const auto level = Utils::Level::Error;
            const auto category = Utils::Category::System;

            MARK_SCOPE_C_L(category, level, "Failed to load model from:");
            MARK_IN_SCOPE(category, level, "%s", Utils::ShortPathForLog(_meshPath).c_str());
            MARK_IN_SCOPE(category, level, MARK_COL_LABEL "Severity: " MARK_COL_RESET "%s", "Error");
            MARK_IN_SCOPE(category, level, MARK_COL_LABEL "Warning: " MARK_COL_RESET "%s", (warn.empty()) ? "<none>": warn.c_str());
            MARK_IN_SCOPE(category, level, MARK_COL_LABEL "Error: " MARK_COL_RESET "%s", (err.empty()) ? "<none>" : err.c_str());
            MARK_IN_SCOPE(category, level, "Defaulting to " MARK_COL_LABEL2 "[MARK_FALLBACK_MODEL]" MARK_COL_RESET);

            const char* fallbackMeshPath = MARK_FALLBACK_MODEL;
            m_usingFallBack = true;
            if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, fallbackMeshPath))
            {
                MARK_ERROR("Failed to load fallback model from: %s", Utils::ShortPathForLog(fallbackMeshPath).c_str());
                return;
            }
        }

        m_vertices.clear();
        m_indices.clear();

        std::unordered_map<VertexData, uint32_t> uniqueVertices;
        for (const auto& shape : shapes)
        {
            for (const auto& index : shape.mesh.indices)
            {
                VertexData vertex;

                if (index.vertex_index >= 0)
                {
                    vertex.m_position = {
                        attrib.vertices[3 * index.vertex_index + 0],
                        attrib.vertices[3 * index.vertex_index + 1],
                        attrib.vertices[3 * index.vertex_index + 2]
                    };

                    vertex.m_colour = {
                        attrib.colors[3 * index.vertex_index + 0],
                        attrib.colors[3 * index.vertex_index + 1],
                        attrib.colors[3 * index.vertex_index + 2]
                    };
                }

                if (index.normal_index >= 0)
                {
                    vertex.m_normal = {
                        attrib.normals[3 * index.normal_index + 0],
                        attrib.normals[3 * index.normal_index + 1],
                        attrib.normals[3 * index.normal_index + 2]
                    };
                }

                if (index.texcoord_index >= 0)
                {
                    float u = attrib.texcoords[2 * index.texcoord_index + 0];
                    float v = attrib.texcoords[2 * index.texcoord_index + 1];

                    if (_flipV) {
                        v = 1.0f - v;
                    }

                    vertex.m_uv = { u, v };
                }

                // If new Vertex add it to the uniqueVertices map
                if (uniqueVertices.count(vertex) == 0)
                {
                    uniqueVertices[vertex] = static_cast<uint32_t>(m_vertices.size());
                    m_vertices.push_back(vertex);
                }

                m_indices.push_back(uniqueVertices[vertex]);
            }
        }

        MARK_INFO_C(Utils::Category::Vulkan, "Loaded OBJ Mesh From: %s", Utils::ShortPathForLog(_meshPath).c_str());
    }

    void MeshHandler::destroyGPUBuffer(VkDevice _device)
    {
        if (hasVertexBuffer()) {
            m_vertexBuffer.destroy(_device);
        }
        if (hasIndexBuffer()) {
            m_indexBuffer.destroy(_device);
        }
        MARK_INFO_C(Utils::Category::Vulkan, "Mesh buffers destroyed");
    }
} // namespace Mark::RendererVK