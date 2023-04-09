#include "mygl.h"
#include <glm_includes.h>

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <QApplication>
#include <QKeyEvent>

#include "algo/perlin.h"
#include "scene/biome.h"
#include "scene/font.h"
#include "scene/inventory.h"
#include "scene/runnables.h"
#include "scene/structure.h"
#include "server/getip.h"
#include <QDateTime>

MyGL::MyGL(QWidget *parent)
    : OpenGLContext(parent),
      m_worldAxes(this),
      m_progLambert(this), m_progFlat(this), m_progOverlay(this), m_progInstanced(this),
      m_terrain(this), m_player(glm::vec3(48.f, 129.f, 48.f), m_terrain, this),
      m_time(0), m_block_texture(this), m_font_texture(this), m_inventory_texture(this), m_currentMSecsSinceEpoch(QDateTime::currentMSecsSinceEpoch()),
      ip("localhost"), m_crosshair(this), mouseMove(true)
      
{
    // Connect the timer to a function so that when the timer ticks the function is executed
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
}

//move stuff from constructor to here so that it can be called upon entering game scene
void MyGL::start(bool joinServer) {
    setFocusPolicy(Qt::ClickFocus);

    setMouseTracking(true); // MyGL will track the mouse's movements even if a mouse button is not pressed
    setCursor(Qt::BlankCursor); // Make the cursor invisible

    //set up overlays
    overlayTransform = glm::scale(glm::mat4(1), glm::vec3(1.f/width(), 1.f/height(), 1.f));
    m_crosshair.createVBOdata();

    //noise function distribution tests
    //distTest();
    //biomeDist();

    //check if we need to host a server
    if(!joinServer) {
        SERVER = mkU<Server>(1);
        while(!SERVER->setup);
        ip = getIP().data();
    }

    verified_server = false;
    init_client();

    //block until we get world spawn info
    while(!verified_server);
    m_player.setState(m_terrain.worldSpawn, 0, 0);

    m_player.m_inventory.createVBOdata();
    m_player.m_inventory.hotbar.createVBOdata();
    Item a = Item(this, DIAMOND_HOE, 1);
    Item b = Item(this, DIAMOND_LEGGINGS, 1);
    Item c = Item(this, GOLD_NUGGET, 64);
    Item d = Item(this, IRON_NUGGET, 8);
    Item e = Item(this, IRON_BOOTS, 1);
    Item f = Item(this, STONE_SWORD, 1);
    Item g = Item(this, DIAMOND_SWORD, 1);
    Item h = Item(this, IRON_CHESTPLATE, 1);
    Item i = Item(this, STRING, 1);
    Item j = Item(this, IRON_HELMET, 1);
    Item k = Item(this, IRON_INGOT, 9);
    m_player.m_inventory.addItem(a);
    m_player.m_inventory.addItem(b);
    m_player.m_inventory.addItem(c);
    m_player.m_inventory.addItem(d);
    m_player.m_inventory.addItem(e);
    m_player.m_inventory.addItem(f);
    m_player.m_inventory.addItem(g);
    m_player.m_inventory.addItem(h);
    m_player.m_inventory.addItem(i);
    m_player.m_inventory.addItem(j);
    m_player.m_inventory.addItem(k);
    m_player.m_inventory.addItem(j);
    m_player.m_inventory.addItem(j);
    m_player.m_inventory.addItem(k, 26);
    m_player.m_inventory.addItem(j);
    m_player.m_inventory.addItem(j);
    m_player.m_inventory.addItem(j);



    // Tell the timer to redraw 60 times per second
    m_timer.start(16);
}

MyGL::~MyGL() {
    makeCurrent();
    glDeleteVertexArrays(1, &vao);
    close_client();
    SERVER->shutdown();
}

void MyGL::moveMouseToCenter() {
    QCursor::setPos(this->mapToGlobal(QPoint(width() / 2, height() / 2)));
}

void MyGL::initializeGL()
{
    // Create an OpenGL context using Qt's QOpenGLFunctions_3_2_Core class
    // If you were programming in a non-Qt context you might use GLEW (GL Extension Wrangler)instead
    initializeOpenGLFunctions();
    // Print out some information about the current OpenGL context
    debugContextVersion();

    // Set a few settings/modes in OpenGL rendering
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);

    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // Set the color with which the screen is filled at the start of each render call.
    glClearColor(0.37f, 0.74f, 1.0f, 1);

    printGLErrorLog();

    // Create a Vertex Attribute Object
    glGenVertexArrays(1, &vao);

    //load texture into memory and store on gpu
    m_block_texture.create(":/textures/block_item_textures.png");
    m_block_texture.load(0);

    m_font_texture.create(":/textures/ascii.png");
    m_font_texture.load(1);

    m_inventory_texture.create(":/textures/inventory.png");
    m_inventory_texture.load(2);

    overlayTransform = glm::scale(glm::mat4(1), glm::vec3(1.f/width(), 1.f/height(), 1.f));

    //Create the instance of the world axes
    m_worldAxes.createVBOdata();

    // Create and set up the diffuse shader
    m_progLambert.create(":/glsl/lambert.vert.glsl", ":/glsl/lambert.frag.glsl");
    // Create and set up the flat lighting shader
    m_progFlat.create(":/glsl/flat.vert.glsl", ":/glsl/flat.frag.glsl");
    m_progOverlay.create(":/glsl/overlay.vert.glsl", ":/glsl/overlay.frag.glsl");
    m_progInstanced.create(":/glsl/instanced.vert.glsl", ":/glsl/instanced.frag.glsl");

    // Set a color with which to draw geometry.
    // This will ultimately not be used when you change
    // your program to render Chunks with vertex colors
    // and UV coordinates
    //m_progLambert.setGeometryColor(glm::vec4(0,1,0,1));


    // We have to have a VAO bound in OpenGL 3.2 Core. But if we're not
    // using multiple VAOs, we can just bind one once.
    glBindVertexArray(vao);
}

void MyGL::resizeGL(int w, int h) {
    //This code sets the concatenated view and perspective projection matrices used for
    //our scene's camera view.
    m_player.setCameraWidthHeight(static_cast<unsigned int>(w), static_cast<unsigned int>(h));
    glm::mat4 viewproj = m_player.mcr_camera.getViewProj();
    overlayTransform = glm::scale(glm::mat4(1), glm::vec3(1.f/w, 1.f/h, 1.f));

    // Upload the view-projection matrix to our shaders (i.e. onto the graphics card)

    m_progLambert.setViewProjMatrix(viewproj);
    m_progFlat.setViewProjMatrix(viewproj);
    m_progOverlay.setViewProjMatrix(viewproj);

    overlayTransform = glm::scale(glm::mat4(1), glm::vec3(1.f/w, 1.f/h, 1.f));

    m_block_texture.bind(0);
    //m_font_texture.bind(1);

    printGLErrorLog();
}


// MyGL's constructor links tick() to a timer that fires 60 times per second.
// We're treating MyGL as our game engine class, so we're going to perform
// all per-frame actions here, such as performing physics updates on all
// entities in the scene.

void MyGL::tick() {
    m_time++;
    long long ct = QDateTime::currentMSecsSinceEpoch();
    float dt = 0.001 * (ct - m_currentMSecsSinceEpoch);
    m_currentMSecsSinceEpoch = ct;

    if (mouseMove) updateMouse();
    m_player.tick(dt, m_inputs);
    setupTerrainThreads();

    update(); // Calls paintGL() as part of a larger QOpenGLWidget pipeline

    PlayerStatePacket pp = PlayerStatePacket(m_player.getPos(), m_player.getTheta(), m_player.getPhi());
    send_packet(&pp);

    sendPlayerDataToGUI(); // Updates the info in the secondary window displaying player data
    //generates chunks based on player position
    //zone player is in

}

void MyGL::setupTerrainThreads() {
    //does rendering stuff
    int minx = floor(m_player.mcr_position.x/64)*64;
    int miny = floor(m_player.mcr_position.z/64)*64;
    for(int dx = minx-192; dx <= minx+192; dx+=64) {
        for(int dy = miny-192; dy <= miny+192; dy+=64) {
            if(m_terrain.m_generatedTerrain.find(toKey(dx, dy)) == m_terrain.m_generatedTerrain.end()){
                m_terrain.m_generatedTerrain.insert(toKey(dx, dy));
                for(int ddx = dx; ddx < dx + 64; ddx+=16) {
                    for(int ddy = dy; ddy < dy + 64; ddy+=16) {
                        //qDebug() << "creating ground for " << ddx << ddy;
                        m_terrain.createGroundThread(glm::vec2(ddx, ddy));
                    }
                }
            }
        }
    }

    //checks for additional structures for rendering, but not as often since structure threads can finish at staggered times
    if(m_time%30 == 0) {
        for(int dx = minx-192; dx <= minx+192; dx+=64) {
            for(int dy = miny-192; dy <= miny+192; dy+=64) {
                for(int ddx = dx; ddx < dx + 64; ddx+=16) {
                    for(int ddy = dy; ddy < dy + 64; ddy+=16) {
                        if(m_terrain.hasChunkAt(ddx, ddy)){
                            Chunk* c = m_terrain.getChunkAt(ddx, ddy).get();
                            if(c->dataGen && c->blocksChanged){
                                c->blocksChanged = false;
                                m_terrain.createVBOThread(c);
                            }
                        }
                    }
                }

            }
        }
        m_time = 0;
    }
}

void MyGL::sendPlayerDataToGUI() const{
    emit sig_sendServerIP(QString::fromStdString(ip));
    emit sig_sendPlayerPos(m_player.posAsQString());
    emit sig_sendPlayerVel(m_player.velAsQString());
    emit sig_sendPlayerAcc(m_player.accAsQString());
    emit sig_sendPlayerLook(m_player.lookAsQString());
    glm::vec2 pPos(m_player.mcr_position.x, m_player.mcr_position.z);
    glm::ivec2 chunk(16 * glm::ivec2(glm::floor(pPos / 16.f)));
    glm::ivec2 zone(64 * glm::ivec2(glm::floor(pPos / 64.f)));
    emit sig_sendPlayerChunk(QString::fromStdString("( " + std::to_string(chunk.x) + ", " + std::to_string(chunk.y) + " )"));
    emit sig_sendPlayerTerrainZone(QString::fromStdString("( " + std::to_string(zone.x) + ", " + std::to_string(zone.y) + " )"));
}

// This function is called whenever update() is called.
// MyGL's constructor links update() to a timer that fires 60 times per second,
// so paintGL() called at a rate of 60 frames per second.
void MyGL::paintGL() {
    // Clear the screen so that we only see newly drawn images
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_progLambert.setTime(m_time);
    m_progFlat.setViewProjMatrix(m_player.mcr_camera.getViewProj());
    m_progLambert.setViewProjMatrix(m_player.mcr_camera.getViewProj());
    m_progInstanced.setViewProjMatrix(m_player.mcr_camera.getViewProj());
    m_block_texture.bind(0);
    renderTerrain();
    //m_player.createVBOdata();
    //m_progLambert.setModelMatrix(glm::translate(glm::mat4(1.f), glm::vec3(m_player.mcr_position)));
    //m_progLambert.drawInterleaved(m_player);
    m_multiplayers_mutex.lock();
    for(std::map<int, uPtr<Player>>::iterator it = m_multiplayers.begin(); it != m_multiplayers.end(); it++) {
        it->second->createVBOdata();
        m_progLambert.setModelMatrix(glm::translate(glm::mat4(1.f), glm::vec3(it->second->m_position)));
        m_progLambert.drawInterleaved(*(it->second));
    }
    m_multiplayers_mutex.unlock();

    glDisable(GL_DEPTH_TEST);
    
    m_progFlat.setModelMatrix(glm::mat4());
    m_progFlat.setViewProjMatrix(overlayTransform);
    m_progFlat.draw(m_crosshair);

    m_progOverlay.setModelMatrix(glm::mat4());
    m_progOverlay.setViewProjMatrix(overlayTransform);
    m_font_texture.bind(0);

    //inventory gui
    m_inventory_texture.bind(0);
    m_progOverlay.setModelMatrix(glm::translate(glm::mat4(1), glm::vec3(0, -height()+10, 0)));
    m_progOverlay.draw(m_player.m_inventory.hotbar);

    if(m_player.m_inventory.showInventory) {
        m_progOverlay.setModelMatrix(glm::mat4(1));
        m_progOverlay.draw(m_player.m_inventory);
    }

    //inventory items
    m_block_texture.bind(0);
    if(m_player.m_inventory.showInventory){
        for(int i = 0; i < m_player.m_inventory.items.size(); i++) {
            std::optional<Item>& item = m_player.m_inventory.items[i];
            if(item) {
                item->draw(&m_progOverlay, m_block_texture, m_font_texture,
                           60, 30, glm::vec3(-550.f*158.f/256.f+36.f/256.f*550.f*(i%9), -550.f*32.f/256.f-(i/9)*36.f/256.f*550.f, 0), glm::vec3(-550.f*158.f/256.f+36.f/256.f*550.f*(i%9)+65, -5-550.f*32.f/256.f-(i/9)*36.f/256.f*550.f, 0));
            }
        }
        for(int i = 0; i < m_player.m_inventory.hotbar.items.size(); i++){
            std::optional<Item>& item = m_player.m_inventory.hotbar.items[i];
            if(item) {
                item->draw(&m_progOverlay, m_block_texture, m_font_texture,
                            60, 30,
                           glm::vec3(-550.f*158.f/256.f+36.f/256.f*550.f*(i%9), -550.f*148.f/256.f-(i/9)*36.f/256.f*550.f, 0),
                           glm::vec3(-550.f*158.f/256.f+36.f/256.f*550.f*(i%9)+65, -5-550.f*148.f/256.f-(i/9)*36.f/256.f*550.f, 0));
            }
        }
    }

    for(int i = 0; i < m_player.m_inventory.hotbar.items.size(); i++){
        std::optional<Item>& item = m_player.m_inventory.hotbar.items[i];
        if(item) {
            item->draw(&m_progOverlay, m_block_texture, m_font_texture,
                       60, 35, glm::vec3(-450+i*100 + 20, 30-height(), 0), glm::vec3(-450+i*100 + 96, 10-height(), 0));
        }
    }

    if(m_player.m_inventory.showInventory) {
        QPoint cur = mapFromGlobal(QCursor::pos());
        if(m_cursor_item) {
            m_cursor_item->draw(&m_progOverlay, m_block_texture, m_font_texture,
                                60, 30, glm::vec3(2*cur.x()-width()+31, -2*cur.y()+height()-31, 0), glm::vec3(2*cur.x()-width()+96, -5-2*cur.y()+height()-31, 0));
        }
        m_progFlat.setModelMatrix(glm::translate(glm::mat4(1), glm::vec3(2*cur.x()-width()+65, -2*cur.y()+height(), 0)));
        m_progFlat.draw(m_crosshair);
    }
    
    glEnable(GL_DEPTH_TEST);
}

// TODO: Change this so it renders the nine zones of generated
// terrain that surround the player (refer to Terrain::m_generatedTerrain
// for more info)

void MyGL::renderTerrain() {
    //chunk player is in
    int renderDist = 512;
    float x = floor(m_player.mcr_position.x/16.f)*16;
    float y = floor(m_player.mcr_position.z/16.f)*16;

    m_terrain.draw(x-renderDist, x+renderDist, y-renderDist, y+renderDist, &m_progLambert);
    //m_terrain.draw(0, 1024, 0, 1024, &m_progInstanced);
}


void MyGL::keyPressEvent(QKeyEvent *e) {
    // http://doc.qt.io/qt-5/qt.html#Key-enum
    // This could all be much more efficient if a switch
    // statement were used, but I really dislike their
    // syntax so I chose to be lazy and use a long
    // chain of if statements instead
    if (e->key() == Qt::Key_Escape) {
        QApplication::quit();
    } else if (e->key() == Qt::Key_W) {
        m_inputs.wPressed = true;
    } else if (e->key() == Qt::Key_S) {
        m_inputs.sPressed = true;
    } else if (e->key() == Qt::Key_D) {
        m_inputs.dPressed = true;
    } else if (e->key() == Qt::Key_A) {
        m_inputs.aPressed = true;
    } else if (e->key() == Qt::Key_Shift) {
        m_inputs.lshiftPressed = true;
    } else if (e->key() == Qt::Key_F) {
        m_inputs.fPressed = true;
    } else if (e->key() == Qt::Key_Space) {
        m_inputs.spacePressed = true;
    } else if (e->key() == Qt::Key_Right) {
        m_inputs.mouseX = 5.f;
    } else if (e->key() == Qt::Key_Left) {
        m_inputs.mouseX = -5.f;
    } else if (e->key() == Qt::Key_Up) {
        m_inputs.mouseY = -5.f;
    } else if (e->key() == Qt::Key_Down) {
        m_inputs.mouseY = 5.f;
    }     //inventory hotkeys
    else if (e->key() == Qt::Key_E) {
        m_player.m_inventory.showInventory = !m_player.m_inventory.showInventory;
        mouseMove = !m_player.m_inventory.showInventory;
    } else if (e->key() == Qt::Key_Q) {
        m_player.m_inventory.showInventory = !m_player.m_inventory.showInventory;
    } else if (e->key() == Qt::Key_1) {
        m_player.m_inventory.hotbar.selected = 0;
        m_player.m_inventory.hotbar.createVBOdata();
    } else if (e->key() == Qt::Key_2) {
        m_player.m_inventory.hotbar.selected = 1;
        m_player.m_inventory.hotbar.createVBOdata();
    } else if (e->key() == Qt::Key_3) {
        m_player.m_inventory.hotbar.selected = 2;
        m_player.m_inventory.hotbar.createVBOdata();
    } else if (e->key() == Qt::Key_4) {
        m_player.m_inventory.hotbar.selected = 3;
        m_player.m_inventory.hotbar.createVBOdata();
    } else if (e->key() == Qt::Key_5) {
        m_player.m_inventory.hotbar.selected = 4;
        m_player.m_inventory.hotbar.createVBOdata();
    } else if (e->key() == Qt::Key_6) {
        m_player.m_inventory.hotbar.selected = 5;
        m_player.m_inventory.hotbar.createVBOdata();
    } else if (e->key() == Qt::Key_7) {
        m_player.m_inventory.hotbar.selected = 6;
        m_player.m_inventory.hotbar.createVBOdata();
    } else if (e->key() == Qt::Key_8) {
        m_player.m_inventory.hotbar.selected = 7;
        m_player.m_inventory.hotbar.createVBOdata();
    } else if (e->key() == Qt::Key_9) {
        m_player.m_inventory.hotbar.selected = 8;
        m_player.m_inventory.hotbar.createVBOdata();
    }
}

void MyGL::keyReleaseEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_W) {
        m_inputs.wPressed = false;
    } else if (e->key() == Qt::Key_S) {
        m_inputs.sPressed = false;
    } else if (e->key() == Qt::Key_D) {
        m_inputs.dPressed = false;
    } else if (e->key() == Qt::Key_A) {
        m_inputs.aPressed = false;
    } else if (e->key() == Qt::Key_Shift) {
        m_inputs.lshiftPressed = false;
    } else if (e->key() == Qt::Key_F) {
        m_inputs.fPressed = false;
    } else if (e->key() == Qt::Key_Space) {
        m_inputs.spacePressed = false;
    }
}

void MyGL::updateMouse() {
    float sens = 0.15;
    QPoint cur = QWidget::mapFromGlobal(QCursor::pos());
    m_inputs.mouseX = sens * (cur.x() - width() / 2);
    m_inputs.mouseY = sens * (cur.y() - height() / 2);
    moveMouseToCenter();
}

void MyGL::mousePressEvent(QMouseEvent *e) {
    if(m_player.m_inventory.showInventory) {
        QPoint cur = 2*mapFromGlobal(QCursor::pos());
        cur = QPoint(cur.x()-width(), height()-cur.y());
        for(int i = 0; i < m_player.m_inventory.items.size(); i++) {
            float dx = -550.f*158.f/256.f+36.f/256.f*550.f*(i%9);
            float dy = -550.f*32.f/256.f-(i/9)*36.f/256.f*550.f;
            if(cur.x() >= dx-36.f/256.f*550.f && cur.x() <= dx &&
                    cur.y() <= dy+36.f/256.f*550.f && cur.y() >= dy) {
                std::optional<Item> foo = m_player.m_inventory.items[i];
                if(e->buttons() & Qt::LeftButton) {
                    if(foo && m_cursor_item) {
                        m_cursor_item->merge(foo.value());
                        if(foo->item_count == 0){
                            foo.reset();
                        }
                    }
                    m_player.m_inventory.items[i] = m_cursor_item;
                    m_cursor_item = foo;
                }
                else if (e->buttons() & Qt::RightButton) {
                    if(m_cursor_item.has_value()) {
                        if(!m_player.m_inventory.items[i].has_value()) {
                            m_player.m_inventory.items[i] = Item(this, m_cursor_item->type, 1);
                            m_cursor_item-> item_count--;

                            if(m_cursor_item->item_count == 0) m_cursor_item.reset();
                            else m_cursor_item->count_text.setText(std::to_string(m_cursor_item->item_count));
                        }
                        else if(m_player.m_inventory.items[i]->type == m_cursor_item->type && m_player.m_inventory.items[i] ->item_count < m_player.m_inventory.items[i] -> max_count) {
                            m_cursor_item->item_count --;
                            if(m_cursor_item->item_count == 0) m_cursor_item.reset();
                            else m_cursor_item->count_text.setText(std::to_string(m_cursor_item->item_count));

                            m_player.m_inventory.items[i]->item_count ++;
                            m_player.m_inventory.items[i]->count_text.setText(std::to_string(m_player.m_inventory.items[i]->item_count));
                        }
                    }
                    else {
                        if(m_player.m_inventory.items[i].has_value()) {
                            int toMerge = (m_player.m_inventory.items[i]->item_count+1)/2;
                            m_cursor_item = Item(this, m_player.m_inventory.items[i]->type, toMerge);

                            m_player.m_inventory.items[i]->item_count -= toMerge;
                            if(m_player.m_inventory.items[i]->item_count == 0) m_player.m_inventory.items[i].reset();
                            else m_player.m_inventory.items[i]->count_text.setText(std::to_string(m_player.m_inventory.items[i]->item_count));

                        }
                    }
                }
                break;
            }
        }
        for(int i = 0; i < m_player.m_inventory.hotbar.items.size(); i++){
            float dx = -550.f*158.f/256.f+36.f/256.f*550.f*(i%9);
            float dy = -550.f*148.f/256.f-(i/9)*36.f/256.f*550.f;
            if(cur.x() >= dx-36.f/256.f*550.f && cur.x() <= dx  &&
                    cur.y() <= dy+36.f/256.f*550.f && cur.y() >= dy) {
                std::optional<Item> foo = m_player.m_inventory.hotbar.items[i];
                if(e->buttons() & Qt::LeftButton) {
                    if(foo && m_cursor_item) {
                        m_cursor_item->merge(foo.value());
                        if(foo->item_count == 0) {
                            foo.reset();
                        }
                    }
                    m_player.m_inventory.hotbar.items[i] = m_cursor_item;
                    m_cursor_item = foo;
                }
                else if (e->buttons() & Qt::RightButton) {
                    if(m_cursor_item.has_value()) {
                        if(!m_player.m_inventory.hotbar.items[i].has_value()) {
                            m_player.m_inventory.hotbar.items[i] = Item(this, m_cursor_item->type, 1);
                            m_cursor_item-> item_count--;

                            if(m_cursor_item->item_count == 0) m_cursor_item.reset();
                            else m_cursor_item->count_text.setText(std::to_string(m_cursor_item->item_count));
                        }
                        else if(m_player.m_inventory.hotbar.items[i]->type == m_cursor_item->type && m_player.m_inventory.hotbar.items[i] ->item_count < m_player.m_inventory.hotbar.items[i] -> max_count) {
                            m_cursor_item->item_count --;
                            if(m_cursor_item->item_count == 0) m_cursor_item.reset();
                            else m_cursor_item->count_text.setText(std::to_string(m_cursor_item->item_count));

                            m_player.m_inventory.hotbar.items[i]->item_count ++;
                            m_player.m_inventory.hotbar.items[i]->count_text.setText(std::to_string(m_player.m_inventory.hotbar.items[i]->item_count));
                        }
                    }
                    else {
                        if(m_player.m_inventory.hotbar.items[i].has_value()) {
                            int toMerge = (m_player.m_inventory.hotbar.items[i]->item_count+1)/2;
                            m_cursor_item = Item(this, m_player.m_inventory.hotbar.items[i]->type, toMerge);

                            m_player.m_inventory.hotbar.items[i]->item_count -= toMerge;

                            if(m_player.m_inventory.hotbar.items[i]->item_count == 0) m_player.m_inventory.hotbar.items[i].reset();
                            else m_player.m_inventory.hotbar.items[i]->count_text.setText(std::to_string(m_player.m_inventory.hotbar.items[i]->item_count));
                        }
                    }
                }
                break;
            }
        }
    }
    else {
        if(e->buttons() & Qt::LeftButton)
        {
            glm::vec3 cam_pos = m_player.mcr_camera.mcr_position;
            glm::vec3 ray_dir = m_player.getLook() * 3.f;

            float dist;
            glm::ivec3 block_pos;
            if (m_terrain.gridMarch(cam_pos, ray_dir, &dist, &block_pos, false)) {
                qDebug() << block_pos.x << " " << block_pos.y << " " << block_pos.z;
                m_terrain.setBlockAt(block_pos.x, block_pos.y, block_pos.z, EMPTY);
                Chunk* c = m_terrain.getChunkAt(block_pos.x, block_pos.z).get();
                if (block_pos.x % 16 == 0) c->getNeighborChunk(XNEG)->blocksChanged = true;
                if (block_pos.x % 16 == 15) c->getNeighborChunk(XPOS)->blocksChanged = true;
                if (block_pos.z % 16 == 0) c->getNeighborChunk(ZNEG)->blocksChanged = true;
                if (block_pos.z % 16 == 15) c->getNeighborChunk(ZPOS)->blocksChanged = true;
                qDebug() << "blocks changed = " << m_terrain.getChunkAt(32, 32).get()->blocksChanged;
            }
        } else if (e->buttons() & Qt::RightButton) {
            float bound = 3.f;
            glm::vec3 cam_pos = m_player.mcr_camera.mcr_position;
            glm::vec3 ray_dir = glm::normalize(m_player.getLook()) * bound;

            float dist;
            glm::ivec3 block_pos;
            bool found = m_terrain.gridMarch(cam_pos, ray_dir, &dist, &block_pos, false);
            if (found) {
                BlockType type = m_terrain.getBlockAt(block_pos.x, block_pos.y, block_pos.z);
                glm::vec3 backward = -glm::normalize(ray_dir);
                glm::vec3 restart = glm::vec3(cam_pos + glm::normalize(ray_dir) * (dist + 0.2f));
                qDebug() << block_pos.x << " " << block_pos.y << " " << block_pos.z;
                qDebug() << QString::fromStdString(glm::to_string(restart));
                qDebug() << dist;
                glm::ivec3 bpos;
                if (m_terrain.getBlockAt(restart.x, restart.y, restart.z) == EMPTY) {
                    m_terrain.setBlockAt(restart.x, restart.y, restart.z, type);
                } else if (m_terrain.gridMarch(restart, backward, &dist, &bpos, true)) {
                    m_terrain.setBlockAt(bpos.x, bpos.y, bpos.z, type);
                }
            }
        }
    }
}

void MyGL::run_client() {
    QByteArray buffer;
    qDebug() << "client listening";
    buffer.resize(BUFFER_SIZE);
    while (open)
    {
        int bytes_received = recv(client_fd, buffer.data(), buffer.size(), 0);
        if (bytes_received < 0)
        {
            qDebug() << "Failed to receive message";
            break;
        }
        else if (bytes_received == 0)
        {
            qDebug() << "Connection closed by server";
            break;
        }
        else
        {
            buffer.resize(bytes_received);
            Packet* pp = bufferToPacket(buffer);
            packet_processer(pp);
            delete(pp);
            buffer.resize(BUFFER_SIZE);
        }
    }
}

void MyGL::init_client() {
    // create client socket
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        qDebug() << "Failed to create client socket";
        return;
    }

    // connect to server
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    if (inet_pton(AF_INET, &ip[0], &server_address.sin_addr) <= 0)
    {
        qDebug() << "Invalid server address";
        return;
    }
    if (::connect(client_fd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0)
    {
        qDebug() << "Failed to connect to server";
        return;
    }

    // create thread to receive messages
    open = true;
    ClientWorker* cw = new ClientWorker(this);
    QThreadPool::globalInstance()->start(cw);
}

void MyGL::send_packet(Packet* packet) {
    QByteArray buffer;
        switch(packet->type) {
        case PLAYER_STATE:{
            buffer = (dynamic_cast<PlayerStatePacket*>(packet))->packetToBuffer();
            break;
        }
        default:
            break;
        }
        int bytes_sent = send(client_fd, buffer.data(), buffer.size(), 0);
        //qDebug() << bytes_sent;
//        return (bytes_sent >= 0);
}

void MyGL::close_client() {
    open = false;
}

void MyGL::packet_processer(Packet* packet) {
    switch(packet->type) {
    case PLAYER_STATE:{
        PlayerStatePacket* thispack = dynamic_cast<PlayerStatePacket*>(packet);
        m_multiplayers_mutex.lock();
        if(m_multiplayers.find(thispack->player_id) == m_multiplayers.end()) {
            m_multiplayers[thispack->player_id] = mkU<Player>(Player(glm::vec3(0), nullptr, this));
        }
        m_multiplayers[thispack->player_id]->setState(thispack->player_pos, thispack->player_theta, thispack->player_phi);
        m_multiplayers_mutex.unlock();
        break;
    }
    case WORLD_INIT:{
        WorldInitPacket* thispack = dynamic_cast<WorldInitPacket*>(packet);
        //TO do: set seed somewhere
        m_terrain.worldSpawn = thispack->spawn;
        verified_server = true;
        break;
    }
    default:
        qDebug() << "unknown packet type: " << packet->type;
        break;
    }
}
