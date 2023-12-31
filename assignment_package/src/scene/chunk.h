#pragma once
#include "smartpointerhelp.h"
#include "glm_includes.h"
#include <array>
#include <unordered_map>
#include <cstddef>
#include "drawable.h"
#include "biome.h"


//using namespace std;

// C++ 11 allows us to define the size of an enum. This lets us use only one byte
// of memory to store our different block types. By default, the size of a C++ enum
// is that of an int (so, usually four bytes). This *does* limit us to only 256 different
// block types, but in the scope of this project we'll never get anywhere near that many.
enum BlockType : unsigned char
{
    EMPTY, GRASS_BLOCK, DIRT, STONE, WATER, SAND, SNOW, COBBLESTONE,
    OAK_PLANKS, SPRUCE_PLANKS, JUNGLE_PLANKS, BIRCH_PLANKS, ACACIA_PLANKS,
    OAK_LOG, SPRUCE_LOG, BIRCH_LOG, JUNGLE_LOG, ACACIA_LOG,
    OAK_LEAVES, BOOKSHELF, GLASS, PATH, SANDSTONE,
    LAVA, BEDROCK, CACTUS, ICE,
    GRASS};

// The six cardinal directions in 3D space
enum Direction : unsigned char
{
    XPOS, XNEG, YPOS, YNEG, ZPOS, ZNEG
};

glm::vec3 dirToVec(Direction);
Direction vecToDir(glm::vec3);

// Lets us use any enum class as the key of a
// std::unordered_map
struct EnumHash {
    template <typename T>
    size_t operator()(T t) const {
        return static_cast<size_t>(t);
    }
};

// for sorting ivec3
struct KeyFuncsiv3
{
    size_t operator()(const glm::ivec3& k)const
    {
        return std::hash<int>()(k.x) ^ std::hash<int>()(k.y) ^ std::hash<int>()(k.z);
    }

    bool operator()(const glm::ivec3& a, const glm::ivec3& b)const
    {
            return a.x == b.x && a.y == b.y && a.z == b.z;
    }
};

typedef std::unordered_map<glm::ivec3,BlockType,KeyFuncsiv3,KeyFuncsiv3> vec3Map;

//block uvs
std::vector<glm::vec4> getBlockUV(BlockType, int);

// One Chunk is a 16 x 256 x 16 section of the world,
// containing all the Minecraft blocks in that area.
// We divide the world into Chunks in order to make
// recomputing its VBO data faster by not having to
// render all the world at once, while also not having
// to render the world block by block.

// TODO have Chunk inherit from Drawable
class Chunk : public Drawable{
private:
    // All of the blocks contained within this Chunk
    std::array<BlockType, 65536> m_blocks;

    // This Chunk's four neighbors to the north, south, east, and west
    // The third input to this map just lets us use a Direction as
    // a key for this map.
    // These allow us to properly determine
    std::unordered_map<Direction, Chunk*, EnumHash> m_neighbors;

    //for vbo
    std::vector<glm::vec4> VBOinter;
    std::vector<int> idx;

    std::mutex setBlock_mutex;
    std::mutex createVBO_mutex;
    std::mutex neighbor_mutex;
public:
    Chunk(OpenGLContext*);
    Chunk* getNeighborChunk(Direction d);
    BlockType getBlockAt(unsigned int x, unsigned int y, unsigned int z) const;
    BlockType getBlockAt(int x, int y, int z) const;
    void setBlockAt(unsigned int x, unsigned int y, unsigned int z, BlockType t);
    void linkNeighbor(uPtr<Chunk>& neighbor, Direction dir);

    virtual void createVBOdata();
    //locks for multithreading stages
    std::atomic_bool dataBound, dataGen, surfaceGen;

    bool hasTransparent;

    //for generating structures, don't want to redraw vbo for every single one
    std::atomic_bool blocksChanged;

    void bindVBOdata();
    void unbindVBOdata();
    virtual GLenum drawMode();


    //for terrain gen
    BiomeType biome; //biome of chunk, taking 8,8
    int heightMap[16][16]; //height map of surface level ground, to generate surface structs
    // non-generated changes made to terrain
    vec3Map m_changes;
};

bool isTransparent(int x, int y, int z, Chunk* c);
bool isEmpty(int x, int y, int z, Chunk* c);
