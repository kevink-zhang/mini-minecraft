#include "structure.h"
#include "algo/perlin.h"
#include "algo/worley.h"
#include "algo/noise.h"
#include "scene/terrain.h"
#include <queue>

using namespace glm;

#define mp(a, b) std::make_pair(a, b)

//TODO: MOVE SEED TO SOMEWHERE ELSE
int seed2 = 2294021;
float getSeed2(float f) {
    return seed2*f;
}

std::vector<Structure> getStructureZones(Chunk* c) {
    std::vector<Structure> ret;
    ivec2 cp = ivec2(c->pos.x, c->pos.z);
    //find trees, trees should force a 3x3 chunk generation zone
    switch(c->biome){
    case PLAINS:
        for(int i = 0; i < 16; i+=16) {
            for(int j = 0; j < 16; j+=16) {
                ivec2 pp = cp+ivec2(i,j)+ivec2(clamp(14.f*random2(cp, vec4(getSeed2(1),getSeed2(1.2),getSeed2(1.4),getSeed2(1.1))), 0.f, 15.f));
                if(c->heightMap[pp.x-cp.x][pp.y-cp.y] < 64+64 && c->getBlockAt(pp.x-cp.x, c->heightMap[pp.x-cp.x][pp.y-cp.y]-1, pp.y-cp.y) == GRASS){
                    ret.push_back(Structure(OAK_TREE, pp));
                }
            }
        }
        break;
    case OCEAN:
        break;
    case BEACH:
        break;
    default:
//        for(int i = 0; i < 16; i+=8) {
//            for(int j = 0; j < 16; j+=8) {
//                ivec2 pp = cp+ivec2(i,j)+ivec2(clamp(8.f*random2(cp, vec4(getSeed2(1),getSeed2(1.2),getSeed2(1.4),getSeed2(1.1))), 0.f, 15.f));
//                if(c->heightMap[pp.x-cp.x][pp.y-cp.y] < 64+64 && c->getBlockAt(pp.x-cp.x, c->heightMap[pp.x-cp.x][pp.y-cp.y]-1, pp.y-cp.y) == GRASS){
//                    ret.push_back(Structure(OAK_TREE, pp));
//                }
//            }
//        }
        break;
    }
    return ret;
}

//returns a quick computed list of meta structures relevant to a point
std::vector<std::pair<std::pair<int64_t, int>, StructureType>> getMetaStructures(vec2 pp) {
    std::vector<std::pair<std::pair<int64_t, int>, StructureType>> ret;
    vec2 villageCenter = vec2(0);//floor(pp*0.0011f)*900.f + 900.f*random2(floor(pp*0.0011f)*900.f, glm::vec4(125,45356,23,532));
    //using y = 1000 to indicate surface level generation
    //ret.push_back(std::make_pair(std::make_pair(toKey(villageCenter.x, villageCenter.y), 1000), VILLAGE_CENTER));

    //for demo purposes
    ret.push_back(std::make_pair(std::make_pair(toKey(150,150), 1000), VILLAGE_CENTER));

    return ret;
}

//procedurally generates a village
std::vector<Structure> generateVillage(vec2 pp) {
    //TO DO: replace this fixed noise seed with a world seed based seed
    vec3 seeds7 = vec3(589.3, 125319, 43934);
    vec3 seeds8 = vec3(583.342, 153984.5, 9385.2);
    vec3 seeds9 = vec3(1439.342, 648.5, 1940.2);
    qDebug() << "generating village";
    std::vector<Structure> ret;

    //generates village center
    ivec2 villageCenter = ivec2(pp);//floor(pp*0.0011f)*900.f + 900.f*random2(floor(pp*0.0011f)*900.f, glm::vec4(125,45356,23,532));
    ret.emplace_back(VILLAGE_CENTER, villageCenter);
    ret.emplace_back(OAK_TREE, villageCenter);

    //stores the 'hitbox' of structures for spawn condition detection
    std::vector<std::pair<vec2, vec2>> hitbox;
    hitbox.push_back(std::make_pair(villageCenter, vec2(7,7)));

    //L-system with roads
    std::queue<std::vector<int>> q;
    std::vector<vec2> xdir;
    std::vector<vec2> zdir;
    q.push({villageCenter.x+6, villageCenter.y, 1, 0, 0, 0});
    q.push({villageCenter.x-6, villageCenter.y, -1, 0, 0, 0});
    q.push({villageCenter.x, villageCenter.y+6, 0, 1, 0, 0});
    q.push({villageCenter.x, villageCenter.y-6, 0, -1, 0, 0});
    while(!q.empty()) {
        std::vector<int> v = q.front();
        ivec2 pos = ivec2(v[0], v[1]);
        ivec2 att = ivec2(v[2], v[3]);
        ivec2 len = ivec2(v[4], v[5]);
        ret.emplace_back(VILLAGE_ROAD, pos, vecToDir(vec3(att.x, 0, att.y)));
        q.pop();
        float f = noise1D(pos, seeds8);
        float rl = noise1D(pos, seeds9);
        float p_turn = 0.04;
        float p_branch = 0.04;
        float p_t = 0.01;
        float p_stop = len[0]*0.00005;
        if(len[0] < 64) p_stop = 0;
        if(len[0] < 64) p_branch = 0.06;
        if(len[1] < 16) p_turn = p_branch = p_t = 0;
        if(len[0] >= 200) p_stop = 1;

        std::vector<std::vector<int>> toAdd;
        if(f < p_turn) {
            if(rl<0.5){
                att = ivec2(-att.y, att.x);
            }
            else {
                att = ivec2(att.y, -att.x);
            }
            toAdd.push_back({pos.x, pos.y, att.x, att.y, len[0]+20, 0});
        }
        else if(f < p_turn+p_branch) {
            pos+=att;
            toAdd.push_back({pos.x, pos.y, att.x, att.y, len[0]+1, len[1]+1});
            pos-=att;
            if(rl<0.5){
                att = ivec2(-att.y, att.x);
            }
            else {
                att = ivec2(att.y, -att.x);
            }
            toAdd.push_back({pos.x, pos.y, att.x, att.y, len[0]+20, 0});
        }
        else if(f < p_turn+p_branch+p_t) {
            toAdd.push_back({pos.x, pos.y, -att.y, att.x, len[0]+20, 0});
            toAdd.push_back({pos.x, pos.y, att.y, -att.x, len[0]+20, 0});
        }
        else if(f < p_turn+p_branch+p_t+p_stop) {
            //stop
        }
        else {
            pos+=att;
            toAdd.push_back({pos.x, pos.y, att.x, att.y, len[0]+1, len[1]+1});
        }
        for(std::vector<int> vv: toAdd) {
            bool canAdd = true;
            if(vv[2] != 0) {
                for(vec2 comp: xdir) {
                    if(abs(vv[0]-comp.x) < 10 && abs(vv[1]-comp.y) < 10 && vv[1]!=comp.y) {
                        canAdd = false;
                        break;
                    }
                }
                if(canAdd) {
                    q.push(vv);
                    xdir.push_back(vec2(vv[0], vv[1]));
                }
            }
            else {
                for(vec2 comp: zdir) {
                    if(abs(vv[0]-comp.x) < 10 && abs(vv[1]-comp.y) < 10 && vv[0]!=comp.x) {
                        canAdd = false;
                        break;
                    }
                }
                if(canAdd) {
                    q.push(vv);
                    zdir.push_back(vec2(vv[0], vv[1]));
                }
            }
        }
    }

    std::vector<std::pair<vec2, Direction>> tries;
    for(vec2 vv: xdir) {
        tries.push_back(std::make_pair(vv+vec2(0, 4), ZNEG));
        tries.push_back(std::make_pair(vv-vec2(0, 4), ZPOS));
    }
    for(vec2 vv: zdir) {
        tries.push_back(std::make_pair(vv+vec2(4, 0), XNEG));
        tries.push_back(std::make_pair(vv-vec2(4, 0), XPOS));
    }
    for(std::pair<vec2, Direction> pp: tries) {
        float f = noise1D(pp.first, seeds7);
        if(f<0.4){
            int towndist = max(abs(pp.first.x-villageCenter.x), abs(pp.first.y-villageCenter.y));
            StructureType st;
            int hx, hy;
            vec2 thisCenter; //shifted center
            //get the building type
            if(towndist < 30) {
                st = VILLAGE_LIBRARY;
                if(pp.second == ZPOS || pp.second == ZNEG){
                    hx = 8+1;
                    hy = 4+1;
                }
                else {
                    hy = 8+1;
                    hx = 4+1;
                }
                thisCenter = pp.first - 5.f * vec2(dirToVec(pp.second).x, dirToVec(pp.second).z);
            }
            else {
                st = VILLAGE_HOUSE_1;
                hx = 3+1;
                hy = 3+1;
                thisCenter = pp.first - 3.f * vec2(dirToVec(pp.second).x, dirToVec(pp.second).z);
            }
            //check if the location intersects existing structures
            bool canUse = true;
            //streets running +-X
            for(vec2 vv: xdir) {
                if(abs(thisCenter.x - vv.x) <= hx && abs(thisCenter.y - vv.y) <= hy+2) {
                    //qDebug() << thisCenter.x << thisCenter.y << "intersects x street" << vv.x << vv.y;
                    canUse = false;
                    break;
                }
            }
            if(!canUse) continue;
            //streets running +-Z
            for(vec2 vv: zdir) {
                if(abs(thisCenter.x - vv.x) <= hx+2 && abs(thisCenter.y - vv.y) <= hy) {
                    canUse = false;
                    break;
                }
            }
            if(!canUse) continue;
            //other buildings
            for(std::pair<vec2, vec2> box: hitbox) {
                if(abs(thisCenter.x - box.first.x) <= hx+box.second.x && abs(thisCenter.y - box.first.y) <= hy+box.second.y) {
                    canUse = false;
                    break;
                }
            }
            if(!canUse) continue;
            //finally we can add the building
            ret.emplace_back(st, pp.first, pp.second);
            //use a slightly larger hitbox for buildings
            hitbox.push_back(std::make_pair(thisCenter, vec2(hx+2, hy+2)));
        }
    }

    //ret.emplace_back(VILLAGE_HOUSE_1, villageCenter+ivec2(-50, 50), XPOS);
    //ret.emplace_back(VILLAGE_LIBRARY, villageCenter+ivec2(50, 50), XPOS);
    qDebug() << "finished village gen";
    return ret;
}
