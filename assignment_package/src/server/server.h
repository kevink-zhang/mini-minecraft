#pragma once

#include <string>
#include <QThreadPool>
#include "scene/terrain.h"
#include "scene/entity.h"
#include "scene/player.h"
#include "server/packet.h"

#include "port.h"
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

struct PlayerState {
    float phi, theta;
    glm::vec3 pos;
    QString name;
    PlayerState(glm::vec3 p, float t, float ph, QString n)
        : phi(ph), theta(t), pos(p), name(n){};
    PlayerState(){};
};

class Server{
private:
    int server_fd;
    std::mutex client_fds_mutex;
    std::vector<int> client_fds;

    void process_packet(Packet*, int);
    void broadcast_packet(Packet*, int);
    void target_packet(Packet*, int);

    //actions to do when a client joins
    //send over world seed, entity and player list, etc
    void initClient(int);
    //generates terrain around a player
    void generateTerrain(int x, int z);

    Terrain m_terrain;
    std::mutex m_players_mutex;
    std::map<int, PlayerState> m_players;
    std::mutex m_entities_mutex;
    std::vector<Entity> m_entities;

    QThreadPool m_clients;

    int seed;
public:
    Server(int);

    //starts the server
    int start();

    //client handler
    void handle_client(int);

    //determines if we can accept clients yet
    bool setup, open;

    //shuts down the server, doesn't quite work yet
    void shutdown();

    //server tick
    void tick();
};

