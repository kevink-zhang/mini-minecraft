#include "player.h"
#include <QString>

Player::Player(glm::vec3 pos, const Terrain &terrain)
    : Entity(pos), m_velocity(0,0,0), m_acceleration(0,0,0),
      m_camera(pos + glm::vec3(0, 1.5f, 0)), mcr_terrain(terrain),
      m_flightMode(true), mcr_camera(m_camera)
{}

Player::~Player()
{}

void Player::tick(float dT, InputBundle &input) {
    processInputs(input);
    computePhysics(dT, mcr_terrain);
}

void Player::processInputs(InputBundle &inputs) {
    // TODO: Update the Player's velocity and acceleration based on the
    // state of the inputs.

    float SPEED = 0.1f;
    if (inputs.fPressed) {
        m_flightMode = !m_flightMode;
        inputs.fPressed = false;
    }

    if (inputs.wPressed) {
        //action
        if (m_flightMode) {
            m_acceleration += m_forward * SPEED;
        } else {
            m_acceleration += glm::normalize(
                                glm::vec3(m_forward.x, 0, m_forward.z)) * SPEED;
        }
        inputs.wPressed = false;
    }

    if (inputs.sPressed) {
        //action
        if (m_flightMode) {
            m_acceleration -= m_forward * SPEED;
        } else {
            m_acceleration -= glm::normalize(
                                glm::vec3(m_forward.x, 0, m_forward.z)) * SPEED;
        }
        inputs.sPressed = false;
    }

    if (inputs.dPressed) {
        //action
        if (m_flightMode) {
            m_acceleration += m_right * SPEED;
        } else {
            m_acceleration += glm::normalize(
                                glm::vec3(m_right.x, 0, m_right.z)) * SPEED;
        }
        inputs.dPressed = false;
    }

    if (inputs.aPressed) {
        //action
        if (m_flightMode) {
            m_acceleration -= m_right * SPEED;
        } else {
            m_acceleration -= glm::normalize(
                                glm::vec3(m_right.x, 0, m_right.z)) * SPEED;
        }
        inputs.aPressed = false;
    }

    if (inputs.ePressed) {
        //action
        if (m_flightMode) {
            m_acceleration += m_up * SPEED;
        } else {
            m_acceleration += glm::normalize(
                                glm::vec3(m_up.x, 0, m_up.z)) * SPEED;
        }
        inputs.ePressed = false;
    }

    if (inputs.qPressed) {
        //action
        if (m_flightMode) {
            m_acceleration -= m_up * SPEED;
        } else {
            m_acceleration -= glm::normalize(
                                glm::vec3(m_up.x, 0, m_up.z)) * SPEED;
        }
        inputs.qPressed = false;
    }

    if (inputs.spacePressed) {
        //action
        m_acceleration += m_up * SPEED * 2.f;
        inputs.spacePressed = false;
    }
}

void Player::computePhysics(float dT, const Terrain &terrain) {
    // TODO: Update the Player's position based on its acceleration
    // and velocity, and also perform collision detection.

    m_velocity = m_velocity * 0.95f;

    if (m_flightMode) {
        m_velocity += m_acceleration * glm::max(dT, 1.f);
    } else {
        glm::vec3 gravity(0, -0.005f, 0);
        m_velocity += gravity;
        m_velocity += m_acceleration * glm::max(dT, 1.f);
    }

    moveAlongVector(m_velocity);

    m_acceleration = glm::vec3(0);
}

void Player::setCameraWidthHeight(unsigned int w, unsigned int h) {
    m_camera.setWidthHeight(w, h);
}

void Player::moveAlongVector(glm::vec3 dir) {
    Entity::moveAlongVector(dir);
    m_camera.moveAlongVector(dir);
}
void Player::moveForwardLocal(float amount) {
    Entity::moveForwardLocal(amount);
    m_camera.moveForwardLocal(amount);
}
void Player::moveRightLocal(float amount) {
    Entity::moveRightLocal(amount);
    m_camera.moveRightLocal(amount);
}
void Player::moveUpLocal(float amount) {
    Entity::moveUpLocal(amount);
    m_camera.moveUpLocal(amount);
}
void Player::moveForwardGlobal(float amount) {
    Entity::moveForwardGlobal(amount);
    m_camera.moveForwardGlobal(amount);
}
void Player::moveRightGlobal(float amount) {
    Entity::moveRightGlobal(amount);
    m_camera.moveRightGlobal(amount);
}
void Player::moveUpGlobal(float amount) {
    Entity::moveUpGlobal(amount);
    m_camera.moveUpGlobal(amount);
}
void Player::rotateOnForwardLocal(float degrees) {
    Entity::rotateOnForwardLocal(degrees);
    m_camera.rotateOnForwardLocal(degrees);
}
void Player::rotateOnRightLocal(float degrees) {
    Entity::rotateOnRightLocal(degrees);
    m_camera.rotateOnRightLocal(degrees);
}
void Player::rotateOnUpLocal(float degrees) {
    Entity::rotateOnUpLocal(degrees);
    m_camera.rotateOnUpLocal(degrees);
}
void Player::rotateOnForwardGlobal(float degrees) {
    Entity::rotateOnForwardGlobal(degrees);
    m_camera.rotateOnForwardGlobal(degrees);
}
void Player::rotateOnRightGlobal(float degrees) {
    Entity::rotateOnRightGlobal(degrees);
    m_camera.rotateOnRightGlobal(degrees);
}
void Player::rotateOnUpGlobal(float degrees) {
    Entity::rotateOnUpGlobal(degrees);
    m_camera.rotateOnUpGlobal(degrees);
}

QString Player::posAsQString() const {
    std::string str("( " + std::to_string(m_position.x) + ", " + std::to_string(m_position.y) + ", " + std::to_string(m_position.z) + ")");
    return QString::fromStdString(str);
}
QString Player::velAsQString() const {
    std::string str("( " + std::to_string(m_velocity.x) + ", " + std::to_string(m_velocity.y) + ", " + std::to_string(m_velocity.z) + ")");
    return QString::fromStdString(str);
}
QString Player::accAsQString() const {
    std::string str("( " + std::to_string(m_acceleration.x) + ", " + std::to_string(m_acceleration.y) + ", " + std::to_string(m_acceleration.z) + ")");
    return QString::fromStdString(str);
}
QString Player::lookAsQString() const {
    std::string str("( " + std::to_string(m_forward.x) + ", " + std::to_string(m_forward.y) + ", " + std::to_string(m_forward.z) + ")");
    return QString::fromStdString(str);
}
