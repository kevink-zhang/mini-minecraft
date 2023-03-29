#include "terrain.h"
#include "algo/worley.h"
#include "scene/biome.h"
#include "scene/structure.h"
#include <stdexcept>
#include <iostream>
#include <QDebug>
#include <thread>
#include <queue>
#include "algo/noise.h"

#define TEST_RADIUS 256

#define ocean_level 0.53
#define OCEAN_LEVEL 64
#define BEDROCK_LEVEL 64
#define beach_level 0.1

Terrain::Terrain(OpenGLContext *context)
    : m_chunks(), mp_context(context), m_generatedTerrain(),
      activeGroundThreads(300)
{

}

Terrain::~Terrain() {

}

// Combine two 32-bit ints into one 64-bit int
// where the upper 32 bits are X and the lower 32 bits are Z
int64_t toKey(int x, int z) {
    int64_t xz = 0xffffffffffffffff;
    int64_t x64 = x;
    int64_t z64 = z;

    // Set all lower 32 bits to 1 so we can & with Z later
    xz = (xz & (x64 << 32)) | 0x00000000ffffffff;

    // Set all upper 32 bits to 1 so we can & with XZ
    z64 = z64 | 0xffffffff00000000;

    // Combine
    xz = xz & z64;
    return xz;
}

glm::ivec2 toCoords(int64_t k) {
    // Z is lower 32 bits
    int64_t z = k & 0x00000000ffffffff;
    // If the most significant bit of Z is 1, then it's a negative number
    // so we have to set all the upper 32 bits to 1.
    // Note the 8    V
    if(z & 0x0000000080000000) {
        z = z | 0xffffffff00000000;
    }
    int64_t x = (k >> 32);

    return glm::ivec2(x, z);
}


// Surround calls to this with try-catch if you don't know whether
// the coordinates at x, y, z have a corresponding Chunk
BlockType Terrain::getBlockAt(int x, int y, int z)
{
    if(hasChunkAt(x, z)) {
        // Just disallow action below or above min/max height,
        // but don't crash the game over it.
        if(y < 0 || y >= 256) {
            return EMPTY;
        }
        const uPtr<Chunk> &c = getChunkAt(x, z);
        glm::vec2 chunkOrigin = glm::vec2(floor(x / 16.f) * 16, floor(z / 16.f) * 16);
        return c->getBlockAt(static_cast<unsigned int>(x - chunkOrigin.x),
                             static_cast<unsigned int>(y),
                             static_cast<unsigned int>(z - chunkOrigin.y));
    }
    else {
        throw std::out_of_range("Coordinates " + std::to_string(x) +
                                " " + std::to_string(y) + " " +
                                std::to_string(z) + " have no Chunk!");
    }
}

BlockType Terrain::getBlockAt(glm::vec3 p)  {
    return getBlockAt(p.x, p.y, p.z);
}

bool Terrain::hasChunkAt(int x, int z) {
    // Map x and z to their nearest Chunk corner
    // By flooring x and z, then multiplying by 16,
    // we clamp (x, z) to its nearest Chunk-space corner,
    // then scale back to a world-space location.
    // Note that floor() lets us handle negative numbers
    // correctly, as floor(-1 / 16.f) gives us -1, as
    // opposed to (int)(-1 / 16.f) giving us 0 (incorrect!).
    int xFloor = static_cast<int>(glm::floor(x / 16.f));
    int zFloor = static_cast<int>(glm::floor(z / 16.f));
    m_chunks_mutex.lock();
    bool b= m_chunks.find(toKey(16 * xFloor, 16 * zFloor)) != m_chunks.end();
    //qDebug() << x << z << b;
    m_chunks_mutex.unlock();
    return b;
}


uPtr<Chunk>& Terrain::getChunkAt(int x, int z) {
    int xFloor = static_cast<int>(glm::floor(x / 16.f));
    int zFloor = static_cast<int>(glm::floor(z / 16.f));
    m_chunks_mutex.lock();
    uPtr<Chunk>& c = m_chunks[toKey(16 * xFloor, 16 * zFloor)];
    m_chunks_mutex.unlock();
    return c;
}


const uPtr<Chunk>& Terrain::getChunkAt(int x, int z) const {
    int xFloor = static_cast<int>(glm::floor(x / 16.f));
    int zFloor = static_cast<int>(glm::floor(z / 16.f));
    return m_chunks.at(toKey(16 * xFloor, 16 * zFloor));
}

void Terrain::setBlockAt(int x, int y, int z, BlockType t)
{
    if(hasChunkAt(x, z)) {
        uPtr<Chunk> &c = getChunkAt(x, z);
        c->setBlockAt(static_cast<unsigned int>(x - c->pos.x),
                      static_cast<unsigned int>(y),
                      static_cast<unsigned int>(z - c->pos.z),
                      t);
    }
    else {
        int xFloor = static_cast<int>(glm::floor(x / 16.f));
        int zFloor = static_cast<int>(glm::floor(z / 16.f));
        int64_t key = toKey(16 * xFloor, 16 * zFloor);
        metaData_mutex.lock();
        if(metaData.find(key) == metaData.end()){
            metaData[key] = std::vector<metadata>();
        }
        metaData[key].emplace_back(t, glm::vec3(x-16*xFloor, y, z-16*zFloor));
        metaData_mutex.unlock();
//        throw std::out_of_range("Coordinates " + std::to_string(x) +
//                                " " + std::to_string(y) + " " +
//                                std::to_string(z) + " have no Chunk!");
    }
}


Chunk* Terrain::instantiateChunkAt(int x, int z) {
    //semaphore blocking to limit thread count
    activeGroundThreads.acquire();

    x = floor(x/16.f)*16;
    z = floor(z/16.f)*16;

    int64_t key = toKey(x, z);

    uPtr<Chunk> chunk = mkU<Chunk>(mp_context);
    Chunk *cPtr = chunk.get();

    cPtr->setPos(x, z);

    //biome info to generate with blocktype later
    BiomeType biomeMap[16][16];

    //terrain initialization
    for(int xx = x; xx < x+16; xx++) {
        for(int zz = z; zz < z+16; zz++) {
            float bedrock = generateBedrock(glm::vec2(xx,zz));
            float beachhead = beach_level*generateBeach(glm::vec2(xx,zz));
            std::pair<float, BiomeType> groundInfo = generateGround(glm::vec2(xx,zz));

            //deep ocean
            if(bedrock < ocean_level/2) {
                //use center of chunk as the biome of the chunk
                if(xx == x+8 && zz == z+8) {
                    cPtr->biome = OCEAN;
                }
                //height
                cPtr->heightMap[xx-x][zz-z] = OCEAN_LEVEL;
                biomeMap[xx-x][zz-z] = OCEAN;
            }
            //shallow ocean
            else if(bedrock < ocean_level) {
                //use center of chunk as the biome of the chunk
                if(xx == x+8 && zz == z+8) {
                    cPtr->biome = OCEAN;
                }
                //height
                cPtr->heightMap[xx-x][zz-z] = OCEAN_LEVEL;
                biomeMap[xx-x][zz-z] = OCEAN;
            }
            //beach
            else if(bedrock < ocean_level+beachhead) {
                //use center of chunk as the biome of the chunk
                if(xx == x+8 && zz == z+8) {
                    cPtr->biome = BEACH;
                }
                //height
                //float erosion = generateErosion(vec2(xx,zz));
                //shoreline
                float height = glm::clamp((int)(OCEAN_LEVEL + pow((bedrock-ocean_level)/beachhead,2)*(groundInfo.first+(bedrock-ocean_level)*BEDROCK_LEVEL)),
                                     0, 256);
                cPtr->heightMap[xx-x][zz-z] = height;
                if(height <= OCEAN_LEVEL+5 && groundInfo.second != RIVER){
                    biomeMap[xx-x][zz-z] = BEACH;
                }
                else {
                    biomeMap[xx-x][zz-z] = groundInfo.second;
                }
            }
            //land
            else {
                //use center of chunk as the biome of the chunk
                if(xx == x+8 && zz == z+8) {
                    cPtr->biome = groundInfo.second;
                }
                //height
                float height = glm::clamp((int)(OCEAN_LEVEL + groundInfo.first +(bedrock-ocean_level)*BEDROCK_LEVEL), 0, 256);
                cPtr->heightMap[xx-x][zz-z] = height;
                biomeMap[xx-x][zz-z] = groundInfo.second;
            }
        }
    }

    //using height and biome map, generate chunk
    for(int xx = 0; xx < 16; xx++) {
        for(int zz = 0; zz < 16; zz++) {
            for(int y = 0; y < cPtr->heightMap[xx][zz]; y++) {
                switch(biomeMap[xx][zz]) {
                    case TUNDRA:
                        cPtr->setBlockAt(xx, y, zz, SNOW);
                        break;
                    case PLAINS:
                        cPtr->setBlockAt(xx, y, zz, GRASS);
                        break;
                    case DESERT:
                        cPtr->setBlockAt(xx, y, zz, SAND);
                        break;
                    case TAIGA:
                        cPtr->setBlockAt(xx, y, zz, GRASS);
                        break;
                    case FOREST:
                        cPtr->setBlockAt(xx, y, zz, STONE);
                        break;
                    case BEACH:
                        cPtr->setBlockAt(xx, y, zz, SAND);
                        break;
                    case OCEAN:
                        cPtr->setBlockAt(xx, y, zz, WATER);
                        break;
                    case RIVER:
                        cPtr->setBlockAt(xx, y, zz, WATER);
                        break;
                    default:
                        cPtr->setBlockAt(xx, y, zz, GRASS);
                        break;
                }
            }
        }
    }

    m_chunks_mutex.lock();
    m_chunks[toKey(x, z)] = move(chunk);
    m_chunks_mutex.unlock();

    std::vector<Structure> chunkStructures = getStructureZones(cPtr);
    //checks and generates metaStructures, if needed

    for(std::pair<glm::vec2, StructureType> metaS: getMetaStructures(glm::vec2(x, z))){
        int64_t key = toKey(metaS.first.x, metaS.first.y);
        metaStructures_mutex.lock();
        if(metaStructures.find(key) == metaStructures.end()){
            metaStructures[key] = metaS.second;
            //unlock the map so other threads can use it after marking this one as generating
            metaStructures_mutex.unlock();
            std::vector<Structure> new_structures;
            switch(metaS.second){
            case VILLAGE_CENTER:
                new_structures = generateVillage(glm::vec2(x, z));
                chunkStructures.insert(chunkStructures.end(), new_structures.begin(), new_structures.end());
                break;
            default:
                break;
            }
        }
        else{
            metaStructures_mutex.unlock();
        }
    }

    //generates structures
    for(const Structure &s: chunkStructures){
        int xx = s.pos.x;
        int zz = s.pos.y;
        Chunk* c = cPtr;
        switch(s.type){
        case OAK_TREE: {
            //how tall the tree is off the ground
            //TO DO: replace the noise seed with a seed based vec3
            //TO DO: replace GRASS with LEAVES block once implemented
            //TO DO: replace DIRT with WOOD block once implemented
            int ymax = 6+3.f*noise1D(glm::vec2(xx, zz), glm::vec3(3,2,1));
            //find base of tree
            int ymin = c->heightMap[xx-x][zz-z];
            for(int dy = 0; dy < 4; dy++) {
                int yat = ymin+ymax-dy;
                switch(dy) {
                    case 0:
                        setBlockAt(xx, yat, zz, GRASS);
                        setBlockAt(xx-1, yat, zz, GRASS);
                        setBlockAt(xx+1, yat, zz, GRASS);
                        setBlockAt(xx, yat, zz-1, GRASS);
                        setBlockAt(xx, yat, zz+1, GRASS);
                        break;
                    case 1:
                        setBlockAt(xx, yat, zz, GRASS);
                        setBlockAt(xx-1, yat, zz, GRASS);
                        setBlockAt(xx+1, yat, zz, GRASS);
                        setBlockAt(xx, yat, zz-1, GRASS);
                        setBlockAt(xx, yat, zz+1, GRASS);
                        if(noise1D(glm::vec3(xx+1, yat, zz+1), glm::vec4(4,3,2,1)) > 0.5) {
                            setBlockAt(xx+1, yat, zz+1, GRASS);
                        }
                        if(noise1D(glm::vec3(xx+1, yat, zz-1), glm::vec4(4,3,2,1)) > 0.5) {
                            setBlockAt(xx+1, yat, zz-1, GRASS);
                        }
                        if(noise1D(glm::vec3(xx-1, yat, zz+1), glm::vec4(4,3,2,1)) > 0.5) {
                            setBlockAt(xx-1, yat, zz+1, GRASS);
                        }
                        if(noise1D(glm::vec3(xx-1, yat, zz-1), glm::vec4(4,3,2,1)) > 0.5) {
                            setBlockAt(xx-1, yat, zz-1, GRASS);
                        }
                        break;
                    default: //2, 3
                        for(int dx = xx-2; dx <= xx+2; dx++) {
                            for(int dz = zz-1; dz <= zz+1; dz++) {
                                if(dz != xx || dz != zz) {
                                    setBlockAt(dx, yat, dz, GRASS);
                                }
                            }
                        }
                        setBlockAt(xx-1, yat, zz+2, GRASS);
                        setBlockAt(xx, yat, zz+2, GRASS);
                        setBlockAt(xx+1, yat, zz+2, GRASS);
                        setBlockAt(xx-1, yat, zz-2, GRASS);
                        setBlockAt(xx, yat, zz-2, GRASS);
                        setBlockAt(xx+1, yat, zz-2, GRASS);
                        if(noise1D(glm::vec3(xx+2, yat, zz+2), glm::vec4(4,3,2,1)) > 0.5) {
                            setBlockAt(xx+2, yat, zz+2, GRASS);
                        }
                        if(noise1D(glm::vec3(xx+2, yat, zz-2), glm::vec4(4,3,2,1)) > 0.5) {
                            setBlockAt(xx+2, yat, zz-2, GRASS);
                        }
                        if(noise1D(glm::vec3(xx-2, yat, zz+2), glm::vec4(4,3,2,1)) > 0.5) {
                            setBlockAt(xx-2, yat, zz+2, GRASS);
                        }
                        if(noise1D(glm::vec3(xx-2, yat, zz-2), glm::vec4(4,3,2,1)) > 0.5) {
                            setBlockAt(xx-2, yat, zz-2, GRASS);
                        }
                        break;
                }
            }
            for(int y = ymin; y < ymin+ymax; y++){
                setBlockAt(xx, y, zz, DIRT);
            }
            break;
        }
        case FANCY_OAK_TREE:{
            int ymin = c->heightMap[x-(int)c->pos.x][z-(int)c->pos.z];
            break;
        }
            //creates a spruce tree
            //TO DO: replace the seed with an actual seed-dependent seed
            //TO DO: replace block types with appropriate leaves and wood
        case SPRUCE_TREE:{
            int ymin = c->heightMap[x-(int)c->pos.x][z-(int)c->pos.z];
            int ymax = 5+7*noise1D(glm::vec2(xx,zz), glm::vec3(3,2,1));
            float leaves = 1;
            setBlockAt(xx, ymax, zz, DIRT);
            for(int y = ymax+ymin-1; y > ymax; y--) {
                float transition = noise1D(glm::vec3(xx, y, zz), glm::vec4(32,24,10, 12));
                if(leaves == 0){
                    leaves++;
                }
                else if(leaves == 1) { //radius 1
                    setBlockAt(xx-1, y, zz, GRASS);
                    setBlockAt(xx+1, y, zz, GRASS);
                    setBlockAt(xx, y, zz-1, GRASS);
                    setBlockAt(xx, y, zz+1, GRASS);
                    if(transition < 0.3) leaves--;
                    else leaves++;
                }
                else if(leaves == 2) { //radius 2
                    for(int xxx = xx-2; xxx <= xx+2; xxx++) {
                        for(int zzz = zz-2; zzz <= zz+2; zzz++) {
                            if(abs(xxx-xx)+abs(zzz-zz) != 4)
                                setBlockAt(xxx, y, zzz, GRASS);
                        }
                    }
                    if(transition<0.7) leaves--;
                    else leaves++;
                }
                else{ //radius 3
                    for(int xxx = xx-3; xxx <= xx+3; xxx++) {
                        for(int zzz = zz-3; zzz <= zz+3; zzz++) {
                            if(abs(xxx-xx)+abs(zzz-zz) != 6)
                                setBlockAt(xxx, y, zzz, GRASS);
                        }
                    }
                    leaves--;
                }
                setBlockAt(xx, y, zz, DIRT);
            }
            setBlockAt(xx, ymax+ymin, zz, GRASS);
            break;
        }
        case VILLAGE_CENTER:
            setBlockAt(xx, 1000, zz, STONE);
            setBlockAt(xx, 1001, zz, STONE);
            setBlockAt(xx, 1002, zz, STONE);
            break;
        case VILLAGE_ROAD:
            setBlockAt(xx, 1000-1, zz, DIRT);
            break;
        case VILLAGE_HOUSE_1:
            //floor
            for(int i = -1; i <= 1; i++) {
                for(int j = -1; j <= 1; j++) {
                    //TO DO: replace dirt with planks
                    setBlockAt(xx+i, 1000-1, zz+j, DIRT);
                }
            }
            //pillars
            for(int i = -2; i <= 2; i+= 4){
                for(int j = -2; j <= 2; j+= 4){
                    for(int y = -1; y < -1+4; y++) {
                        setBlockAt(xx+i, 1000+y, zz+j, GRASS);
                    }
                }
            }
            //walls
            for(int i = -1; i <= 1; i++) {
                for(int y = -1; y < -1+4; y++) {
                    setBlockAt(xx+i, 1000+y, zz+2, STONE);
                    setBlockAt(xx+i, 1000+y, zz-2, STONE);
                    setBlockAt(xx+2, 1000+y, zz+i, STONE);
                    setBlockAt(xx-2, 1000+y, zz+i, STONE);
                }
            }
            //carve out windows+door
            setBlockAt(xx+2, 1000+1, zz, EMPTY);
            setBlockAt(xx-2, 1000+1, zz, EMPTY);
            setBlockAt(xx, 1000+1, zz+2, EMPTY);
            setBlockAt(xx, 1000+1, zz-1, EMPTY);
            setBlockAt(xx+2.f*dirToVec(s.orient).x, 1000, zz+2.f*dirToVec(s.orient).z, EMPTY);

            //roof
            for(int i = -3; i <= 3; i++) {
                setBlockAt(xx+i, 1000+3, zz+3, DIRT);
                setBlockAt(xx+i, 1000+3, zz-3, DIRT);
                setBlockAt(xx+3, 1000+3, zz+i, DIRT);
                setBlockAt(xx-3, 1000+3, zz+i, DIRT);
            }
            for(int i = -2; i <= 2; i++) {
                for(int j = 0; j < 2; j++){
                    setBlockAt(xx+i, 1000+3+j, zz+2, DIRT);
                    setBlockAt(xx+i, 1000+3+j, zz-2, DIRT);
                    setBlockAt(xx+2, 1000+3+j, zz+i, DIRT);
                    setBlockAt(xx-2, 1000+3+j, zz+i, DIRT);
                }
            }
            for(int i = -1; i <= 1; i++) {
                for(int j = 0; j < 2; j++){
                    setBlockAt(xx+i, 1000+4+j, zz+1, DIRT);
                    setBlockAt(xx+i, 1000+4+j, zz-1, DIRT);
                    setBlockAt(xx+1, 1000+4+j, zz+i, DIRT);
                    setBlockAt(xx-1, 1000+4+j, zz+i, DIRT);
                }
            }
            setBlockAt(xx, 1000+5, zz, DIRT);
            break;
        default:
            break;
        }
    }

    //checks for meta data
    metaData_mutex.lock();
    if(metaData.find(key) != metaData.end()) {
        for(metadata md: metaData[key]){
            cPtr->setBlockAt(md.pos.x, md.pos.y, md.pos.z, md.type);
        }
    }
    metaData.erase(key);
    metaData_mutex.unlock();


    createVBOThread(cPtr);
    // Set the neighbor pointers of itself and its neighbors
    if(hasChunkAt(x, z + 16)) {
        auto &chunkNorth = m_chunks[toKey(x, z + 16)];
        cPtr->linkNeighbor(chunkNorth, ZPOS);
    }
    if(hasChunkAt(x, z - 16)) {
        auto &chunkSouth = m_chunks[toKey(x, z - 16)];
        cPtr->linkNeighbor(chunkSouth, ZNEG);
    }
    if(hasChunkAt(x + 16, z)) {
        auto &chunkEast = m_chunks[toKey(x + 16, z)];
        cPtr->linkNeighbor(chunkEast, XPOS);
    }
    if(hasChunkAt(x - 16, z)) {
        auto &chunkWest = m_chunks[toKey(x - 16, z)];
        cPtr->linkNeighbor(chunkWest, XNEG);
    }

    //decrement the active ground counter
    activeGroundThreads.release();

    return cPtr;
}

// TODO: When you make Chunk inherit from Drawable, change this code so
// it draws each Chunk with the given ShaderProgram, remembering to set the
// model matrix to the proper X and Z translation!
void Terrain::draw(int minX, int maxX, int minZ, int maxZ, ShaderProgram *shaderProgram) {
    for(int x = minX; x < maxX; x += 16) {
        for(int z = minZ; z < maxZ; z += 16) {
            if(hasChunkAt(x, z)){
                uPtr<Chunk> &chunk = getChunkAt(x, z);
                //only renders chunks with generated terrain data
                if(chunk->dataGen){
                    //since only main thread can add
                    if(!chunk->dataBound){
                        chunk->bindVBOdata();
                    }
                    shaderProgram->draw(*chunk.get());
                }
            }
            else {
                //qDebug() << "missing chunk at " << x << z;
            }
        }
    }
    //check if we should clear unloaded chunk vbos
    for(int x = minX - 32; x < maxX + 32; x+= 16) {
        if(hasChunkAt(x, minZ - 32)) {
            uPtr<Chunk> &chunk = getChunkAt(x, minZ - 32);
            if(chunk->dataGen && chunk->dataBound) {
                chunk->unbindVBOdata();
            }
        }
        if(hasChunkAt(x, maxZ+16)) { //not that we don't actually hit maxZ in the drawloop
            uPtr<Chunk> &chunk = getChunkAt(x, maxZ+16);
            if(chunk->dataGen && chunk->dataBound) {
                chunk->unbindVBOdata();
            }
        }
    }
    //dont need to recheck corners, lower bounds by 1 chunk
    for(int z = minZ - 16; z < maxZ+16; z+= 16) {
        if(hasChunkAt(minX-32, z)) {
            uPtr<Chunk> &chunk = getChunkAt(minX-32, z);
            if(chunk->dataGen && chunk->dataBound) {
                chunk->unbindVBOdata();
            }
        }
        if(hasChunkAt(maxX+16, z)) { //not that we don't actually hit maxZ in the drawloop
            uPtr<Chunk> &chunk = getChunkAt(maxX+16, z);
            if(chunk->dataGen && chunk->dataBound) {
                chunk->unbindVBOdata();
            }
        }
    }
}

void Terrain::createInitScene()
{
    // mark one semaphor immediately to signal start
    activeGroundThreads.acquire();
    // Tell our existing terrain set that
    // the "generated terrain zone" at (0,0)
    // now exists.
    // We'll do this part somewhat synchronously since doing it async will result in 500+ threads and crash

    for(int dx = -256; dx <= 256; dx+=64) {
        for(int dy = -256; dy <= 256; dy+=64) {
            m_generatedTerrain.insert(toKey(dx, dy));
            for(int ddx = dx; ddx < dx + 64; ddx+=16) {
                for(int ddy = dy; ddy < dy + 64; ddy+=16) {
                    createGroundThread(glm::vec2(ddx, ddy));
                }
            }
        }
    }
    activeGroundThreads.release();
}

void Terrain::createGroundThread(glm::vec2 p) {
    if(hasChunkAt(p.x, p.y)) return;
    //mutex to prevent concurrent modification of threads vector
    groundGen_mutex.lock();
    groundGenThreads.push_back(std::thread(&Terrain::instantiateChunkAt, this, p.x, p.y));
    groundGen_mutex.unlock();
    //qDebug() << "chunk generators: " << groundGenThreads.size();
}

void Terrain::createVBOThread(Chunk* c) {
    vboGen_mutex.lock();
    vboGenThreads.emplace_back(std::thread(&Chunk::createVBOdata, c));
    vboGen_mutex.unlock();
}


