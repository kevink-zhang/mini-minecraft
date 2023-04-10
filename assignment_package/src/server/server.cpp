#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "algo/seed.h"
#include "server.h"
#include "scene/runnables.h"
#include <QThreadPool>

using namespace std;


Server::Server(int s) : m_terrain(nullptr), seed(s), setup(false), open(true){
    m_clients.setMaxThreadCount(MAX_CLIENTS);
    ServerConnectionWorker* sw = new ServerConnectionWorker(this);
    QThreadPool::globalInstance()->start(sw);
}

void Server::handle_client(int client_fd)
{
    QByteArray buffer;
    buffer.resize(BUFFER_SIZE);
    while (open)
    {
        int valread = read(client_fd, buffer.data(), BUFFER_SIZE);
        if (valread == 0)
        {
            // client has disconnected
            cout << "Client " << client_fd << " disconnected" << endl;
            close(client_fd);
            break;
        }
        else
        {
            //qDebug() << "Client " << client_fd << " says: " << buffer;
            buffer.resize(valread);
            Packet* pp = bufferToPacket(buffer);
            process_packet(pp, client_fd);
            delete(pp);
            buffer.resize(BUFFER_SIZE);
        }
    }
}

void Server::process_packet(Packet* packet, int sender) {
    switch(packet->type) {
    case PLAYER_STATE: {
        PlayerStatePacket* thispack = dynamic_cast<PlayerStatePacket*>(packet);
        m_players_mutex.lock();
        m_players[sender].pos = thispack->player_pos;
        m_players[sender].phi = thispack->player_phi;
        m_players[sender].theta = thispack->player_theta;
        m_players_mutex.unlock();
        broadcast_packet(mkU<PlayerStatePacket>(sender, thispack->player_pos, thispack->player_theta, thispack->player_phi).get(), sender);
        break;
    }
    default:
        qDebug() << "unexpected packet type found" << packet->type;
        break;
    }
}

void Server::broadcast_packet(Packet* packet, int exclude) { //use exclude = 0 if you dont want to exclude
    client_fds_mutex.lock();
    for (int i = 0; i < client_fds.size(); i++)
    {
        if (client_fds[i] != exclude)
        {
            QByteArray buffer = packet->packetToBuffer();
            send(client_fds[i], buffer, buffer.size(), 0);
        }
    }
    client_fds_mutex.unlock();
}

void Server::target_packet(Packet* packet, int target) {
    client_fds_mutex.lock();
    QByteArray buffer = packet->packetToBuffer();
    send(target, buffer, buffer.size(), 0);
    client_fds_mutex.unlock();
}

void Server::initClient(int i) {
    client_fds_mutex.lock();
    client_fds.push_back(i);
    m_players_mutex.lock();
    m_players[i] = PlayerState(glm::vec3(0, 80, 0), 0.f, 0.f);
    m_players_mutex.unlock();
    client_fds_mutex.unlock();
    target_packet(mkU<WorldInitPacket>(seed, m_terrain.worldSpawn).get(), i);
}

int Server::start()
{
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        cout << "Failed to create server socket" << endl;
        return -1;
    }

    // bind server socket to address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if (::bind(server_fd, (struct sockaddr*)&address, addrlen) < 0)
    {
        cout << "Failed to bind server socket to address" << endl;
        return -1;
    }

    // listen for incoming connections
    if (listen(server_fd, MAX_CLIENTS) < 0)
    {
        cout << "Failed to listen for incoming connections" << endl;
        return -1;
    }

    cout << "Server started listening on port " << PORT << endl;

    //initialize spawn chunks, and select a spawn point
    m_terrain.createSpawn();

    setup = true;

    // wait for incoming connections and handle each one in a separate thread
    while (true)
    {
        int client_fd = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (client_fd < 0)
        {
            cout << "Failed to accept incoming connection" << endl;
            continue;
        }
        else
        {
            // do initial actions
            initClient(client_fd);
            // create a new thread to handle the client
            ServerThreadWorker* stw = new ServerThreadWorker(this, client_fd);
            m_clients.start(stw);
            cout << "New client connected: " << inet_ntoa(address.sin_addr) << endl;
        }
    }

    return 0;
}

void Server::shutdown() {
    open = false;
}

