#pragma once
#include "glm_includes.h"
#include "drawable.h"

struct InputBundle {
    bool wPressed, aPressed, sPressed, dPressed, qPressed, ePressed, fPressed;
    bool spacePressed;
    float mouseX, mouseY;

    InputBundle()
        : wPressed(false), aPressed(false), sPressed(false),
          dPressed(false), qPressed(false), ePressed(false), fPressed(false),
          spacePressed(false), mouseX(0.f), mouseY(0.f)
    {}
};

class Entity: public Drawable {
protected:
    // The origin of our local coordinate system
    glm::vec3 m_position;

public:
    // A readonly reference to position for external use
    const glm::vec3& mcr_position;
    // Vectors that define the axes of our local coordinate system
    glm::vec3 m_forward, m_right, m_up;

    // Various constructors
    //Entity();
    Entity(glm::vec3 pos, OpenGLContext* mp_context);
    Entity(const Entity &e, OpenGLContext* mp_context);
    virtual ~Entity();

    // To be called by MyGL::tick()
    virtual void tick(float dT, InputBundle &input) = 0;

    // To be called to draw
    virtual void createVBOdata();
    virtual GLenum drawMode();

    // Translate along the given vector
    virtual void moveAlongVector(glm::vec3 dir);

    // Translate along one of our local axes
    virtual void moveForwardLocal(float amount);
    virtual void moveRightLocal(float amount);
    virtual void moveUpLocal(float amount);

    // Translate along one of the world axes
    virtual void moveForwardGlobal(float amount);
    virtual void moveRightGlobal(float amount);
    virtual void moveUpGlobal(float amount);

    // Rotate about one of our local axes
    virtual void rotateOnForwardLocal(float degrees);
    virtual void rotateOnRightLocal(float degrees);
    virtual void rotateOnUpLocal(float degrees);

    // Rotate about one of the world axes
    virtual void rotateOnForwardGlobal(float degrees);
    virtual void rotateOnRightGlobal(float degrees);
    virtual void rotateOnUpGlobal(float degrees);
};
