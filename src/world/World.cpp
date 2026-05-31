#include "World.h"
#include "TerrainGen.h"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <fstream>

static const glm::vec3 kTileColors[] = {
    {0.0f,  0.0f,  0.0f },  // AIR
    {0.45f, 0.75f, 0.30f},  // GRASS
    {0.55f, 0.35f, 0.15f},  // DIRT
    {0.20f, 0.45f, 0.70f},  // WATER
    {0.55f, 0.55f, 0.55f},  // STONE
    {0.42f, 0.28f, 0.15f},  // WOOD
    {0.30f, 0.55f, 0.25f},  // LEAVES
    {0.30f, 0.18f, 0.08f},  // FARMLAND
    {0.58f, 0.75f, 0.32f},  // WHEAT (stage 0 fallback; overridden by growthStage)
};

World::World() {
    // Chunks are generated on demand via loadChunksAround()
}

glm::ivec2 World::chunkCoord(int x, int y) {
    return {
        (int)std::floor((float)x / CHUNK_SIZE),
        (int)std::floor((float)y / CHUNK_SIZE)
    };
}

glm::ivec2 World::localCoord(int x, int y) {
    return {
        ((x % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE,
        ((y % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE
    };
}

void World::generateChunk(int cx, int cy) {
    Chunk& chunk = getOrCreateChunk(cx, cy);
    TerrainGen::generate(cx, cy, chunk);
}

void World::loadChunksAround(int cx, int cy, int radius) {
    for (int dy = -radius; dy <= radius; dy++)
    for (int dx = -radius; dx <= radius; dx++) {
        glm::ivec2 coord = { cx + dx, cy + dy };
        if (m_chunks.find(coord) != m_chunks.end()) continue;

        auto it = m_modifiedUnloaded.find(coord);
        if (it != m_modifiedUnloaded.end()) {
            m_chunks[coord] = std::move(it->second);
            m_modifiedUnloaded.erase(it);
        } else {
            generateChunk(coord.x, coord.y);
        }
    }
}

void World::unloadChunksOutside(int cx, int cy, int radius) {
    for (auto it = m_chunks.begin(); it != m_chunks.end(); ) {
        if (std::abs(it->first.x - cx) > radius ||
            std::abs(it->first.y - cy) > radius) {
            if (it->second.modified) {
                it->second.dirty = true;
                m_modifiedUnloaded[it->first] = std::move(it->second);
            }
            it = m_chunks.erase(it);
        } else {
            ++it;
        }
    }
}

Chunk& World::getOrCreateChunk(int cx, int cy) {
    return m_chunks[{cx, cy}];
}

const Chunk* World::getChunk(int cx, int cy) const {
    auto it = m_chunks.find({cx, cy});
    return it != m_chunks.end() ? &it->second : nullptr;
}

TileType World::getTile(int x, int y, int z) const {
    if (z < 0 || z >= CHUNK_DEPTH) return TileType::AIR;
    auto cc = chunkCoord(x, y);
    const Chunk* chunk = getChunk(cc.x, cc.y);
    if (!chunk) return TileType::AIR;
    auto lc = localCoord(x, y);
    return chunk->tiles[z][lc.y][lc.x];
}

void World::setTile(int x, int y, int z, TileType t) {
    if (z < 0 || z >= CHUNK_DEPTH) return;
    auto cc = chunkCoord(x, y);
    Chunk& chunk = getOrCreateChunk(cc.x, cc.y);
    auto lc = localCoord(x, y);
    chunk.tiles[z][lc.y][lc.x] = t;
    chunk.dirty    = true;
    chunk.modified = true;
}

TileState World::getTileState(int x, int y, int z) const {
    if (z < 0 || z >= CHUNK_DEPTH) return {};
    auto cc = chunkCoord(x, y);
    const Chunk* chunk = getChunk(cc.x, cc.y);
    if (!chunk) return {};
    auto lc = localCoord(x, y);
    return chunk->states[z][lc.y][lc.x];
}

void World::setTileState(int x, int y, int z, const TileState& s) {
    if (z < 0 || z >= CHUNK_DEPTH) return;
    auto cc = chunkCoord(x, y);
    Chunk& chunk = getOrCreateChunk(cc.x, cc.y);
    auto lc = localCoord(x, y);
    chunk.states[z][lc.y][lc.x] = s;
    chunk.dirty    = true;   // state can affect rendering (e.g. watered farmland tint)
    chunk.modified = true;
}

// ---- Save / Load ----

static constexpr char    kMagic[5]   = "PFRM";
static constexpr uint8_t kSaveVer    = 1;

void World::save(const std::string& path, const glm::vec3& playerPos, float gameTime) const {
    std::ofstream f(path, std::ios::binary);
    if (!f) return;

    f.write(kMagic, 5);
    f.write(reinterpret_cast<const char*>(&kSaveVer), 1);
    f.write(reinterpret_cast<const char*>(&playerPos.x), 4);
    f.write(reinterpret_cast<const char*>(&playerPos.y), 4);
    f.write(reinterpret_cast<const char*>(&playerPos.z), 4);
    f.write(reinterpret_cast<const char*>(&gameTime), 4);

    int32_t count = 0;
    for (const auto& [coord, chunk] : m_chunks)
        if (chunk.modified) count++;
    count += (int32_t)m_modifiedUnloaded.size();
    f.write(reinterpret_cast<const char*>(&count), 4);

    auto writeChunk = [&](const glm::ivec2& coord, const Chunk& chunk) {
        f.write(reinterpret_cast<const char*>(&coord.x), 4);
        f.write(reinterpret_cast<const char*>(&coord.y), 4);
        f.write(reinterpret_cast<const char*>(chunk.tiles), sizeof(chunk.tiles));
        for (int z = 0; z < CHUNK_DEPTH; z++)
        for (int y = 0; y < CHUNK_SIZE;  y++)
        for (int x = 0; x < CHUNK_SIZE;  x++)
            f.write(reinterpret_cast<const char*>(&chunk.states[z][y][x].growthStage), 1);
        for (int z = 0; z < CHUNK_DEPTH; z++)
        for (int y = 0; y < CHUNK_SIZE;  y++)
        for (int x = 0; x < CHUNK_SIZE;  x++)
            f.write(reinterpret_cast<const char*>(&chunk.states[z][y][x].lastUpdatedDay), 4);
    };

    for (const auto& [coord, chunk] : m_chunks)
        if (chunk.modified) writeChunk(coord, chunk);
    for (const auto& [coord, chunk] : m_modifiedUnloaded)
        writeChunk(coord, chunk);
}

bool World::load(const std::string& path, glm::vec3& outPlayerPos, float& outGameTime) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    char magic[5];
    f.read(magic, 5);
    if (std::memcmp(magic, kMagic, 5) != 0) return false;

    uint8_t ver;
    f.read(reinterpret_cast<char*>(&ver), 1);
    if (ver != kSaveVer) return false;

    f.read(reinterpret_cast<char*>(&outPlayerPos.x), 4);
    f.read(reinterpret_cast<char*>(&outPlayerPos.y), 4);
    f.read(reinterpret_cast<char*>(&outPlayerPos.z), 4);
    f.read(reinterpret_cast<char*>(&outGameTime), 4);

    int32_t count;
    f.read(reinterpret_cast<char*>(&count), 4);

    for (int i = 0; i < count; i++) {
        int32_t cx, cy;
        f.read(reinterpret_cast<char*>(&cx), 4);
        f.read(reinterpret_cast<char*>(&cy), 4);

        Chunk chunk;
        f.read(reinterpret_cast<char*>(chunk.tiles), sizeof(chunk.tiles));
        for (int z = 0; z < CHUNK_DEPTH; z++)
        for (int y = 0; y < CHUNK_SIZE;  y++)
        for (int x = 0; x < CHUNK_SIZE;  x++)
            f.read(reinterpret_cast<char*>(&chunk.states[z][y][x].growthStage), 1);
        for (int z = 0; z < CHUNK_DEPTH; z++)
        for (int y = 0; y < CHUNK_SIZE;  y++)
        for (int x = 0; x < CHUNK_SIZE;  x++)
            f.read(reinterpret_cast<char*>(&chunk.states[z][y][x].lastUpdatedDay), 4);

        if (!f) return false;

        // Objects (trees) aren't saved — regenerate them deterministically from the
        // coords so loaded modified chunks keep their trees. Saved tiles/states are
        // preserved; only temp.objects is taken from the regenerated terrain.
        Chunk temp;
        TerrainGen::generate(cx, cy, temp);
        chunk.objects = std::move(temp.objects);

        chunk.modified = true;
        chunk.dirty    = true;
        m_modifiedUnloaded[{cx, cy}] = std::move(chunk);
    }
    return true;
}

// ---- Growth ----
// Water gates growth: wheat advances one stage per in-game day only if the
// farmland below it was watered. Farmland then dries out (must re-water daily).

void World::growthTick(int currentDay) {
    for (auto& [coord, chunk] : m_chunks) {
        bool changed = false;

        // Grow wheat sitting on watered farmland
        for (int z  = 1; z  < CHUNK_DEPTH; z++)
        for (int ly = 0; ly < CHUNK_SIZE;  ly++)
        for (int lx = 0; lx < CHUNK_SIZE;  lx++) {
            if (chunk.tiles[z][ly][lx]   != TileType::WHEAT)    continue;
            if (chunk.tiles[z-1][ly][lx] != TileType::FARMLAND) continue;
            if (!chunk.states[z-1][ly][lx].watered)             continue;
            TileState& s = chunk.states[z][ly][lx];
            if (s.growthStage < 3) {
                s.growthStage++;
                s.lastUpdatedDay = (uint32_t)currentDay;
                changed = true;
            }
        }

        // Farmland dries out each day — must be re-watered
        for (int z  = 0; z  < CHUNK_DEPTH; z++)
        for (int ly = 0; ly < CHUNK_SIZE;  ly++)
        for (int lx = 0; lx < CHUNK_SIZE;  lx++) {
            if (chunk.tiles[z][ly][lx] == TileType::FARMLAND && chunk.states[z][ly][lx].watered) {
                chunk.states[z][ly][lx].watered = false;
                changed = true; // wet -> dry visual change
            }
        }

        if (changed) chunk.dirty = true;
    }
}

bool World::inBounds(int x, int y, int z) const {
    return z >= 0 && z < CHUNK_DEPTH;
}

bool World::isWalkable(int x, int y, int z) const {
    TileType t = getTile(x, y, z);
    return t != TileType::AIR && t != TileType::WATER && t != TileType::WHEAT;
}

glm::ivec3 World::worldToTile(const glm::vec3& position) const {
    return {
        static_cast<int>(std::floor(position.x + 0.5f)),
        static_cast<int>(std::floor(position.y + 0.5f)),
        static_cast<int>(std::round(position.z)) - 1
    };
}

glm::vec3 World::tileCenter(int x, int y, int z) const {
    return { static_cast<float>(x), static_cast<float>(y), static_cast<float>(z) };
}

glm::vec3 World::tileColor(TileType type, uint8_t growthStage) {
    if (type == TileType::WHEAT) {
        static const glm::vec3 kWheatColors[4] = {
            {0.58f, 0.75f, 0.32f},  // stage 0: pale green sprout
            {0.70f, 0.78f, 0.22f},  // stage 1: yellow-green
            {0.85f, 0.78f, 0.12f},  // stage 2: yellow
            {0.95f, 0.78f, 0.05f},  // stage 3: golden
        };
        return kWheatColors[growthStage < 4 ? growthStage : 3];
    }
    return kTileColors[(int)type];
}

static const glm::vec3 kTileSideColors[] = {
    {0.0f,  0.0f,  0.0f },  // AIR
    {0.45f, 0.28f, 0.12f},  // GRASS
    {0.38f, 0.22f, 0.08f},  // DIRT
    {0.15f, 0.35f, 0.60f},  // WATER
    {0.38f, 0.38f, 0.38f},  // STONE
    {0.34f, 0.22f, 0.11f},  // WOOD
    {0.24f, 0.45f, 0.20f},  // LEAVES
    {0.22f, 0.12f, 0.05f},  // FARMLAND
    {0.50f, 0.38f, 0.15f},  // WHEAT (straw)
};

glm::vec3 World::tileSideColor(TileType type) {
    return kTileSideColors[(int)type];
}
