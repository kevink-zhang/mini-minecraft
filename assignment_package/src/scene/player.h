#pragma once
#include "entity.h"
#include "prism.h"
#include "camera.h"
#include "scene/inventory.h"
#include "scene/cubedisplay.h"
#include "scene/handitem.h"
#include "terrain.h"

class Player : public Entity {
private:
    glm::vec3 m_velocity, m_acceleration;
    Camera m_camera;
    const Terrain &mcr_terrain;
    float theta, phi; //horiz, vert
    float airtime, maxair;
    bool in_liquid, bott_in_liquid, shift;
    BlockType camera_block;

    void processInputs(InputBundle &inputs);
    bool checkAirborne();
    void computePhysics(float dT);
    void checkCollision();

public:
    // Readonly public reference to our camera
    // for easy access from MyGL
    const Camera& mcr_camera;
    Prism head, torso, right_arm, right_leg, left_arm, left_leg;
    CubeDisplay display;
    HandItem hand_item;
    float start_swing, swing_dir;
    bool swinging, stopped, created;
    float hit;

    Player(glm::vec3 pos, const Terrain &terrain, OpenGLContext* context, QString n);
    
    virtual ~Player() override;
    Inventory m_inventory;
    int health;
    bool m_flightMode; //creative mode

    int armor;

    void setCameraWidthHeight(unsigned int w, unsigned int h);

    void tick(float dT, InputBundle &input) override;
    void orientCamera();


    // Player overrides all of Entity's movement
    // functions so that it transforms its camera
    // by the same amount as it transforms itself.
    void moveAlongVector(glm::vec3 dir) override;
    void moveForwardLocal(float amount) override;
    void moveRightLocal(float amount) override;
    void moveUpLocal(float amount) override;
    void moveForwardGlobal(float amount) override;
    void moveRightGlobal(float amount) override;
    void moveUpGlobal(float amount) override;
    void rotateOnForwardLocal(float degrees) override;
    void rotateOnRightLocal(float degrees) override;
    void rotateOnUpLocal(float degrees) override;
    void rotateOnForwardGlobal(float degrees) override;
    void rotateOnRightGlobal(float degrees) override;
    void rotateOnUpGlobal(float degrees) override;

    // For sending the Player's data to the GUI
    // for display
    QString posAsQString() const;
    QString velAsQString() const;
    QString accAsQString() const;
    QString lookAsQString() const;

    glm::vec3 getLook();
    glm::vec3 getVelocity();
    void setType(BlockType bt);
    int getType();

    float getTheta();
    float getPhi();
    void createVBOdata();
    void draw(ShaderProgram* m_prog, Texture& skin, float tick);
    void drawArm(ShaderProgram* m_prog, Texture& skin);
    void drawCubeDisplay(ShaderProgram* m_prog);
    void drawHandItem(ShaderProgram* m_prog, Texture& tex);

    ItemType inHand;
    QString name;

    void setState(glm::vec3, glm::vec3, float, float, ItemType); //use this to set the state of other players from server packet
    void knockback(glm::vec3); //apply knockback
};
