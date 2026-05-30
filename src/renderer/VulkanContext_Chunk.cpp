#include "VulkanContext.h"
#include "renderer/Types.h"
#include "world/World.h"

#include <cstring>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

// ============================================================
//  Chunk voxel mesh builder
// ============================================================
void VulkanContext::buildChunkBuffer(const glm::ivec2& coord, Chunk& chunk) {
    // 6 face local vertex offsets, normals, neighbor offsets, top-face flag
    struct FaceDef {
        glm::vec3  verts[4];
        glm::vec3  normal;
        glm::ivec3 neighborOff;
        bool       isTop;
    };
    static const FaceDef kFaces[6] = {
        // Top (+Z)
        {{{-0.5f,-0.5f,0.5f},{0.5f,-0.5f,0.5f},{0.5f,0.5f,0.5f},{-0.5f,0.5f,0.5f}}, {0,0,1},  {0,0,1},  true  },
        // Bottom (-Z)
        {{{-0.5f,0.5f,-0.5f},{0.5f,0.5f,-0.5f},{0.5f,-0.5f,-0.5f},{-0.5f,-0.5f,-0.5f}}, {0,0,-1}, {0,0,-1}, false },
        // Front (+Y)
        {{{0.5f,0.5f,-0.5f},{-0.5f,0.5f,-0.5f},{-0.5f,0.5f,0.5f},{0.5f,0.5f,0.5f}},  {0,1,0},  {0,1,0},  false },
        // Back (-Y)
        {{{-0.5f,-0.5f,-0.5f},{0.5f,-0.5f,-0.5f},{0.5f,-0.5f,0.5f},{-0.5f,-0.5f,0.5f}}, {0,-1,0}, {0,-1,0}, false },
        // Right (+X)
        {{{0.5f,-0.5f,-0.5f},{0.5f,0.5f,-0.5f},{0.5f,0.5f,0.5f},{0.5f,-0.5f,0.5f}},  {1,0,0},  {1,0,0},  false },
        // Left (-X)
        {{{-0.5f,0.5f,-0.5f},{-0.5f,-0.5f,-0.5f},{-0.5f,-0.5f,0.5f},{-0.5f,0.5f,0.5f}}, {-1,0,0}, {-1,0,0}, false },
    };

    const int baseX = coord.x * CHUNK_SIZE;
    const int baseY = coord.y * CHUNK_SIZE;

    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t>    indices;

    // Padded neighborhood copy (chunk + 1-tile border) so face-cull and AO
    // sampling use array indexing instead of per-vertex hashmap lookups.
    // Interior comes from chunk.tiles directly; only the border ring hits getTile.
    static TileType N[CHUNK_DEPTH + 2][CHUNK_SIZE + 2][CHUNK_SIZE + 2];
    for (int lz = -1; lz <= CHUNK_DEPTH; lz++)
    for (int ly = -1; ly <= CHUNK_SIZE; ly++)
    for (int lx = -1; lx <= CHUNK_SIZE; lx++) {
        bool inside = lx >= 0 && lx < CHUNK_SIZE && ly >= 0 && ly < CHUNK_SIZE && lz >= 0 && lz < CHUNK_DEPTH;
        N[lz + 1][ly + 1][lx + 1] = inside
            ? chunk.tiles[lz][ly][lx]
            : m_world.getTile(baseX + lx, baseY + ly, lz);
    }
    auto Nat = [&](int lx, int ly, int lz) -> TileType {
        return N[lz + 1][ly + 1][lx + 1];
    };

    // Per-vertex ambient occlusion, sampling the padded buffer.
    auto vertexAO = [&Nat](int tlx, int tly, int tz, const glm::vec3& n, const glm::vec3& lp) -> float {
        glm::ivec3 ni((int)n.x, (int)n.y, (int)n.z);
        int axes[2], na = 0;
        for (int a = 0; a < 3; a++) if (ni[a] == 0) axes[na++] = a;
        glm::ivec3 d0(0), d1(0);
        d0[axes[0]] = (lp[axes[0]] > 0.0f) ? 1 : -1;
        d1[axes[1]] = (lp[axes[1]] > 0.0f) ? 1 : -1;
        glm::ivec3 b(tlx + ni.x, tly + ni.y, tz + ni.z);
        auto sol = [&](glm::ivec3 p) {
            TileType t = Nat(p.x, p.y, p.z);
            return (t != TileType::AIR && t != TileType::WATER) ? 1 : 0;
        };
        int s0 = sol(b + d0), s1 = sol(b + d1), c = sol(b + d0 + d1);
        int ao = (s0 && s1) ? 0 : (3 - (s0 + s1 + c));
        static const float levels[4] = { 0.5f, 0.7f, 0.85f, 1.0f };
        return levels[ao];
    };

    for (int z  = 0; z  < CHUNK_DEPTH; z++)
    for (int ly = 0; ly < CHUNK_SIZE;  ly++)
    for (int lx = 0; lx < CHUNK_SIZE;  lx++) {
        TileType t = chunk.tiles[z][ly][lx];
        if (t == TileType::AIR) continue;

        const int wx = baseX + lx;
        const int wy = baseY + ly;
        const uint8_t   growthStage = chunk.states[z][ly][lx].growthStage;
        const glm::vec3 topColor    = World::tileColor(t, growthStage);
        const glm::vec3 sideColor   = World::tileSideColor(t);
        const glm::vec3 center      = { (float)wx, (float)wy, (float)z };

        for (const auto& face : kFaces) {
            // Skip if neighbor tile is opaque
            TileType neighbor = Nat(lx + face.neighborOff.x,
                                    ly + face.neighborOff.y,
                                    z  + face.neighborOff.z);
            if (neighbor != TileType::AIR) continue;

            const glm::vec3 color = face.isTop ? topColor : sideColor;
            const uint32_t  base  = (uint32_t)vertices.size();

            for (int i = 0; i < 4; i++) {
                float ao = vertexAO(lx, ly, z, face.normal, face.verts[i]);
                vertices.push_back({ center + face.verts[i], face.normal, color * ao });
            }

            indices.insert(indices.end(), {
                base+0, base+1, base+2,
                base+0, base+2, base+3
            });
        }
    }

    auto& data = m_chunkBuffers[coord];

    // Defer destruction of old buffers — GPU may still be reading them
    deferDestroy(data.vertexBuffer, data.vertexMemory);
    data.vertexBuffer = VK_NULL_HANDLE;
    data.vertexMemory = VK_NULL_HANDLE;
    deferDestroy(data.indexBuffer, data.indexMemory);
    data.indexBuffer = VK_NULL_HANDLE;
    data.indexMemory = VK_NULL_HANDLE;

    data.indexCount = (uint32_t)indices.size();
    if (data.indexCount == 0) return;

    VkDeviceSize vSize = sizeof(ChunkVertex) * vertices.size();
    createBuffer(vSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        data.vertexBuffer, data.vertexMemory);
    void* vMapped;
    vkMapMemory(m_device, data.vertexMemory, 0, vSize, 0, &vMapped);
    memcpy(vMapped, vertices.data(), vSize);
    vkUnmapMemory(m_device, data.vertexMemory);

    VkDeviceSize iSize = sizeof(uint32_t) * indices.size();
    createBuffer(iSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        data.indexBuffer, data.indexMemory);
    void* iMapped;
    vkMapMemory(m_device, data.indexMemory, 0, iSize, 0, &iMapped);
    memcpy(iMapped, indices.data(), iSize);
    vkUnmapMemory(m_device, data.indexMemory);

    buildChunkObjectBuffer(coord, chunk);
}

// ============================================================
//  Per-chunk object (tree) instance buffer
// ============================================================
void VulkanContext::buildChunkObjectBuffer(const glm::ivec2& coord, Chunk& chunk) {
    auto& data = m_chunkBuffers[coord];

    if (data.objInstBuffer != VK_NULL_HANDLE) {
        deferDestroy(data.objInstBuffer, data.objInstMemory);
        data.objInstBuffer = VK_NULL_HANDLE;
        data.objInstMemory = VK_NULL_HANDLE;
    }

    data.objInstCount = (uint32_t)chunk.objects.size();
    if (data.objInstCount == 0) return;

    std::vector<ObjectInstance> insts;
    insts.reserve(chunk.objects.size());
    for (const auto& o : chunk.objects)
        insts.push_back({ o.pos, o.scale, o.rot });

    VkDeviceSize oSize = sizeof(ObjectInstance) * insts.size();
    createBuffer(oSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        data.objInstBuffer, data.objInstMemory);
    void* oMapped;
    vkMapMemory(m_device, data.objInstMemory, 0, oSize, 0, &oMapped);
    memcpy(oMapped, insts.data(), oSize);
    vkUnmapMemory(m_device, data.objInstMemory);
}

// ============================================================
//  Dirty chunk rebuild (called every frame)
// ============================================================
void VulkanContext::rebuildDirtyChunks() {
    // Free GPU buffers for chunks no longer in the world (deferred)
    for (auto it = m_chunkBuffers.begin(); it != m_chunkBuffers.end(); ) {
        if (m_world.chunks().find(it->first) == m_world.chunks().end()) {
            auto& d = it->second;
            deferDestroy(d.vertexBuffer,  d.vertexMemory);
            deferDestroy(d.indexBuffer,   d.indexMemory);
            deferDestroy(d.objInstBuffer, d.objInstMemory);
            it = m_chunkBuffers.erase(it);
        } else {
            ++it;
        }
    }

    // Rebuild dirty chunks
    for (auto& [coord, chunk] : m_world.chunks()) {
        if (!chunk.dirty) continue;
        buildChunkBuffer(coord, chunk);
        chunk.dirty = false;
    }
}
