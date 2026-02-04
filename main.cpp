/**
 * PROYECTO: ASTEROIDS QUADTREE + BOT SNIPER + UI
 * AUTOR: Mateo Llallire Meza
 * FECHA: 03/02/2026
 */

#include "raylib.h"
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <ctime>

// ==========================================
// PARTE 1: ESTRUCTURAS DE DATOS (SIN STL)
// ==========================================

// Reemplazo manual de std::vector
template <typename T>
class MiVector {
private:
    T* data;
    size_t capacity;
    size_t count;

    void resize(size_t new_capacity) {
        T* new_data = new T[new_capacity];
        for (size_t i = 0; i < count; i++) {
            new_data[i] = data[i];
        }
        delete[] data;
        data = new_data;
        capacity = new_capacity;
    }

public:
    MiVector() {
        capacity = 10;
        count = 0;
        data = new T[capacity];
    }

    ~MiVector() {
        if (data) {
            delete[] data;
            data = nullptr;
        }
    }

    void push_back(const T& element) {
        if (count >= capacity) {
            resize(capacity * 2);
        }
        data[count] = element;
        count++;
    }

    void clear() {
        count = 0;
    }

    T& operator[](size_t index) { return data[index]; }
    const T& operator[](size_t index) const { return data[index]; }

    size_t size() const { return count; }
    bool empty() const { return count == 0; }

    void erase(size_t index) {
        if (index >= count) return;
        for (size_t i = index; i < count - 1; i++) {
            data[i] = data[i + 1];
        }
        count--;
    }
};

// ==========================================
// PARTE 2: GEOMETRÍA Y ENTIDADES BASE
// ==========================================

struct Rect {
    float x, y;
    float width, height;

    bool intersects(const Rect& other) const {
        return (x < other.x + other.width &&
                x + width > other.x &&
                y < other.y + other.height &&
                y + height > other.y);
    }
};

class Entity {
public:
    float x, y;
    float width, height;
    bool active;

    Entity(float _x, float _y, float _w, float _h)
        : x(_x), y(_y), width(_w), height(_h), active(true) {}

    virtual ~Entity() = default;

    Rect getBounds() const {
        return { x, y, width, height };
    }

    virtual void update() = 0;
    virtual void draw() = 0;
};

// ==========================================
// PARTE 3: QUADTREE (OPTIMIZACIÓN)
// ==========================================

class Quadtree {
private:
    int MAX_OBJECTS = 4;
    int MAX_LEVELS = 5;

    int level;
    Rect bounds;
    MiVector<Entity*> objects;
    Quadtree* nodes[4];

public:
    Quadtree(int pLevel, Rect pBounds) : level(pLevel), bounds(pBounds) {
        for (int i = 0; i < 4; i++) nodes[i] = nullptr;
    }

    ~Quadtree() {
        clear();
    }

    void clear() {
        objects.clear();
        for (int i = 0; i < 4; i++) {
            if (nodes[i] != nullptr) {
                delete nodes[i];
                nodes[i] = nullptr;
            }
        }
    }

    void split() {
        float subWidth = bounds.width / 2;
        float subHeight = bounds.height / 2;
        float x = bounds.x;
        float y = bounds.y;

        nodes[0] = new Quadtree(level + 1, { x + subWidth, y, subWidth, subHeight });  // NE
        nodes[1] = new Quadtree(level + 1, { x, y, subWidth, subHeight });             // NW
        nodes[2] = new Quadtree(level + 1, { x, y + subHeight, subWidth, subHeight }); // SW
        nodes[3] = new Quadtree(level + 1, { x + subWidth, y + subHeight, subWidth, subHeight }); // SE
    }

    int getIndex(Entity* pRect) {
        int index = -1;
        double verticalMidpoint = bounds.x + (bounds.width / 2);
        double horizontalMidpoint = bounds.y + (bounds.height / 2);

        Rect r = pRect->getBounds();

        bool topQuadrant = (r.y < horizontalMidpoint && r.y + r.height < horizontalMidpoint);
        bool bottomQuadrant = (r.y > horizontalMidpoint);

        if (r.x < verticalMidpoint && r.x + r.width < verticalMidpoint) {
            if (topQuadrant) index = 1;      // NW
            else if (bottomQuadrant) index = 2; // SW
        }
        else if (r.x > verticalMidpoint) {
            if (topQuadrant) index = 0;      // NE
            else if (bottomQuadrant) index = 3; // SE
        }

        return index;
    }

    void insert(Entity* pRect) {
        if (nodes[0] != nullptr) {
            int index = getIndex(pRect);
            if (index != -1) {
                nodes[index]->insert(pRect);
                return;
            }
        }

        objects.push_back(pRect);

        if (objects.size() > MAX_OBJECTS && level < MAX_LEVELS) {
            if (nodes[0] == nullptr) split();

            int i = 0;
            while (i < objects.size()) {
                int index = getIndex(objects[i]);
                if (index != -1) {
                    Entity* obj = objects[i];
                    nodes[index]->insert(obj);
                    objects.erase(i);
                } else {
                    i++;
                }
            }
        }
    }

    void retrieve(MiVector<Entity*>& returnObjects, Entity* pRect) {
        int index = getIndex(pRect);
        if (index != -1 && nodes[0] != nullptr) {
            nodes[index]->retrieve(returnObjects, pRect);
        }

        for (size_t i = 0; i < objects.size(); i++) {
            returnObjects.push_back(objects[i]);
        }

        if (index == -1 && nodes[0] != nullptr) {
            for(int i=0; i<4; i++) {
                nodes[i]->retrieve(returnObjects, pRect);
            }
        }
    }

    void getAllBounds(MiVector<Rect>& list) {
        list.push_back(bounds);
        if (nodes[0] != nullptr) {
            for (int i = 0; i < 4; i++) {
                nodes[i]->getAllBounds(list);
            }
        }
    }
};

// ==========================================
// PARTE 4: CLASES DE JUEGO (GAMEPLAY)
// ==========================================

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 500;

struct Vec2 { float x, y; };

class Bullet : public Entity {
public:
    Vec2 velocity;
    float lifeTime;

    Bullet(float _x, float _y, float angle)
        : Entity(_x, _y, 5, 5) {
        float speed = 1100.0f;
        velocity.x = cos(angle * DEG2RAD) * speed;
        velocity.y = sin(angle * DEG2RAD) * speed;
        lifeTime = 0.4f;
    }

    void update() override {
        float dt = GetFrameTime();
        x += velocity.x * dt;
        y += velocity.y * dt;
        lifeTime -= dt;

        if (lifeTime <= 0) active = false;

        if (x > SCREEN_WIDTH) {x = 0;}
        if (x < 0) {x = SCREEN_WIDTH;}
        if (y > SCREEN_HEIGHT) {y = 0;}
        if (y < 0) {y = SCREEN_HEIGHT;}
    }

    void draw() override {
        DrawRectangle((int)x, (int)y, (int)width, (int)height, YELLOW);
    }
};

class Asteroid : public Entity {
public:
    Vec2 velocity;
    int sizeLevel;
    Vector2 shapePoints[12];
    int totalPoints;

    Asteroid(float _x, float _y, int _size)
        : Entity(_x, _y, 0, 0), sizeLevel(_size) {

        float baseRadius = 0;
        if (sizeLevel == 3) { width = height = 60; baseRadius = 30; }
        else if (sizeLevel == 2) { width = height = 40; baseRadius = 20; }
        else { width = height = 20; baseRadius = 10; }

        float speed;
        if (sizeLevel == 3) {speed = 100;}
        else if (sizeLevel == 2) {speed = 150;}
        else {speed = 250;}

        float moveAngle = (float)(rand() % 360);
        velocity.x = cos(moveAngle * DEG2RAD) * speed;
        velocity.y = sin(moveAngle * DEG2RAD) * speed;

        totalPoints = 8 + (rand() % 5);
        for(int i = 0; i < totalPoints; i++) {
            float angle = (360.0f / totalPoints) * i;
            float variation = ((rand() % 60) - 30) / 100.0f;
            float r = baseRadius * (1.0f + variation);
            shapePoints[i].x = cos(angle * DEG2RAD) * r;
            shapePoints[i].y = sin(angle * DEG2RAD) * r;
        }
    }

    void update() override {
        float dt = GetFrameTime();
        x += velocity.x * dt;
        y += velocity.y * dt;

        if (x > SCREEN_WIDTH) x = -width;
        if (x < -width) x = SCREEN_WIDTH;
        if (y > SCREEN_HEIGHT) y = -height;
        if (y < -height) y = SCREEN_HEIGHT;
    }

    void draw() override {
        float centerX = x + width / 2;
        float centerY = y + height / 2;

        for (int i = 0; i < totalPoints; i++) {
            float startX = centerX + shapePoints[i].x;
            float startY = centerY + shapePoints[i].y;
            int nextIndex = (i + 1) % totalPoints;
            float endX = centerX + shapePoints[nextIndex].x;
            float endY = centerY + shapePoints[nextIndex].y;
            DrawLine((int)startX, (int)startY, (int)endX, (int)endY, WHITE);
        }
    }
};

class Player : public Entity {
public:
    Vec2 velocity;
    float rotation;
    float acceleration;
    float friction;
    MiVector<Entity*>* gameEntitiesRef;

    Player(MiVector<Entity*>* entitiesPtr)
        : Entity(SCREEN_WIDTH/2, SCREEN_HEIGHT/2, 20, 20), gameEntitiesRef(entitiesPtr) {
        velocity = {0, 0};
        rotation = 0;
        acceleration = 900.0f;
        friction = 0.98f;
    }

    virtual void update() override {
        float dt = GetFrameTime();

        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) {rotation -= 270.0f * dt;}
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) {rotation += 270.0f * dt;}

        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) {
            velocity.x += cos(rotation * DEG2RAD) * acceleration * dt;
            velocity.y += sin(rotation * DEG2RAD) * acceleration * dt;
        }

        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) {
            velocity.x -= cos(rotation * DEG2RAD) * acceleration * dt;
            velocity.y -= sin(rotation * DEG2RAD) * acceleration * dt;
        }

        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_KP_0)) {
            shoot();
        }

        x += velocity.x * dt;
        y += velocity.y * dt;
        velocity.x *= friction;
        velocity.y *= friction;

        if (x > SCREEN_WIDTH) x = 0;
        if (x < 0) x = SCREEN_WIDTH;
        if (y > SCREEN_HEIGHT) y = 0;
        if (y < 0) y = SCREEN_HEIGHT;
    }

    void shoot() {
        float centerX = x + width/2;
        float centerY = y + height/2;
        Bullet* b = new Bullet(centerX, centerY, rotation);
        gameEntitiesRef->push_back(b);
    }

    virtual void draw() override {
        DrawTriangleLines(
            (Vector2){x + width/2 + (float)cos(rotation*DEG2RAD)*15, y + height/2 + (float)sin(rotation*DEG2RAD)*15},
            (Vector2){x + width/2 + (float)cos((rotation+140)*DEG2RAD)*10, y + height/2 + (float)sin((rotation+140)*DEG2RAD)*10},
            (Vector2){x + width/2 + (float)cos((rotation-140)*DEG2RAD)*10, y + height/2 + (float)sin((rotation-140)*DEG2RAD)*10},
            GREEN
        );
    }
};

// ==========================================
// AQUI INICIA LA IA (BOT PLAYER)
// ==========================================
class BotPlayer : public Player {
public:
    BotPlayer(MiVector<Entity*>* entitiesPtr)
        : Player(entitiesPtr) {
        acceleration = 1800.0f;
        friction = 0.94f;
    }

    void update() override {
        float dt = GetFrameTime();

        Entity* target = nullptr;
        float minDistance = 100000.0f;

        // 1. ESCANEAR
        for (size_t i = 0; i < gameEntitiesRef->size(); i++) {
            Entity* e = (*gameEntitiesRef)[i];

            if (e == this) continue;
            if (!e->active) continue;

            Asteroid* ast = dynamic_cast<Asteroid*>(e);

            if (ast != nullptr) {
                float dx = e->x - x;
                float dy = e->y - y;
                float dist = sqrt(dx*dx + dy*dy);

                if (dist < minDistance) {
                    minDistance = dist;
                    target = e;
                }
            }
        }

        // 2. LÓGICA DE COMBATE PREDICTIVA
        if (target != nullptr) {
            Asteroid* astTarget = dynamic_cast<Asteroid*>(target);
            float timeToHit = minDistance / 1100.0f; 

            // Predecir futuro
            float futureX = target->x + (astTarget->velocity.x * timeToHit);
            float futureY = target->y + (astTarget->velocity.y * timeToHit);

            float aimX = futureX + target->width/2;
            float aimY = futureY + target->height/2;
            float myX = x + width/2;
            float myY = y + height/2;

            float dx = aimX - myX;
            float dy = aimY - myY;
            float desiredAngle = atan2(dy, dx) * RAD2DEG;

            if (desiredAngle < 0) desiredAngle += 360;

            float diff = desiredAngle - rotation;
            while (diff > 180) diff -= 360;
            while (diff < -180) diff += 360;

            if (diff > 0) rotation += 400.0f * dt;
            else rotation -= 400.0f * dt;

            if (abs(diff) < 10.0f) {
                if ((rand() % 100) < 15) shoot();
            }

            if (minDistance < 180.0f) {
                velocity.x -= cos(desiredAngle * DEG2RAD) * acceleration * dt;
                velocity.y -= sin(desiredAngle * DEG2RAD) * acceleration * dt;
            } else {
                velocity.x *= friction;
                velocity.y *= friction;
            }
        }

        x += velocity.x * dt;
        y += velocity.y * dt;

        if (x > SCREEN_WIDTH) x = 0;
        if (x < 0) x = SCREEN_WIDTH;
        if (y > SCREEN_HEIGHT) y = 0;
        if (y < 0) y = SCREEN_HEIGHT;
    }

    void draw() override {
        Player::draw();
        DrawLine(x+width/2, y+height/2,
                 x+width/2 + cos(rotation*DEG2RAD)*800,
                 y+height/2 + sin(rotation*DEG2RAD)*800,
                 Fade(PURPLE, 0.5f));
    }
};

// ==========================================
// FUNCIONES DE AYUDA (UI Y ESTADO)
// ==========================================

void ReiniciarJuego(MiVector<Entity*>& entities, bool modoBot) {
    // 1. Limpiar memoria antigua
    for (size_t i = 0; i < entities.size(); i++) {
        delete entities[i];
    }
    entities.clear();

    // 2. Crear Jugador (Bot o Humano)
    if (modoBot) {
        BotPlayer* bot = new BotPlayer(&entities);
        entities.push_back(bot);
    } else {
        Player* player = new Player(&entities);
        entities.push_back(player);
    }

    // 3. Crear Asteroides iniciales
    for (int i = 0; i < 8; i++) {
        float x = (rand() % SCREEN_WIDTH);
        float y = (rand() % SCREEN_HEIGHT);
        // Evitar que aparezcan encima del jugador (centro)
        if (abs(x - SCREEN_WIDTH/2) < 100) x += 200;
        entities.push_back(new Asteroid(x, y, 3));
    }
}

// Botón simple con efecto hover
bool DibujarBoton(const char* texto, float x, float y, float w, float h) {
    Vector2 mouse = GetMousePosition();
    Rectangle btnRect = { x, y, w, h };
    bool hover = CheckCollisionPointRec(mouse, btnRect);

    // Fondo
    DrawRectangleRec(btnRect, hover ? DARKGREEN : BLACK);
    // Borde
    DrawRectangleLinesEx(btnRect, 2, hover ? GREEN : DARKGREEN);

    // Texto Centrado
    int fontSize = 20;
    int textWidth = MeasureText(texto, fontSize);
    DrawText(texto, x + (w - textWidth)/2, y + (h - fontSize)/2, fontSize, hover ? WHITE : GREEN);

    return hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

// ==========================================
// MAIN (GESTIÓN DEL JUEGO + ESTADOS)
// ==========================================

// Definimos los estados del juego
enum EstadoJuego {
    MENU_PRINCIPAL,
    SELECCION_MODO,
    JUGANDO
};

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "UTEC - Asteroids Quadtree");
    SetTargetFPS(60);

    // Variables del sistema
    EstadoJuego estadoActual = MENU_PRINCIPAL;
    MiVector<Entity*> entities;
    Rect screenRect = { 0, 0, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT };
    Quadtree* quadtree = new Quadtree(0, screenRect);
    bool showDebug = true;

    while (!WindowShouldClose()) {
        
        // ==========================================================
        // LÓGICA DE ACTUALIZACIÓN SEGÚN EL ESTADO
        // ==========================================================
        
        if (estadoActual == JUGANDO) {
            
            // Regresar al menú con ESC
            if (IsKeyPressed(KEY_ESCAPE)) {
                estadoActual = MENU_PRINCIPAL;
            }

            // 1. UPDATE ENTIDADES
            for (size_t i = 0; i < entities.size(); i++) {
                entities[i]->update();
            }

            // 2. CONSTRUIR QUADTREE
            quadtree->clear();
            for (size_t i = 0; i < entities.size(); i++) {
                if (entities[i]->active) {
                    quadtree->insert(entities[i]);
                }
            }

            // 3. COLISIONES
            MiVector<Entity*> returnObjects;
            for (size_t i = 0; i < entities.size(); i++) {
                Entity* entityA = entities[i];
                if (!entityA->active) continue;

                returnObjects.clear();
                quadtree->retrieve(returnObjects, entityA);

                for (size_t j = 0; j < returnObjects.size(); j++) {
                    Entity* entityB = returnObjects[j];
                    if (entityA == entityB) continue;

                    Bullet* bullet = dynamic_cast<Bullet*>(entityA);
                    Asteroid* asteroid = dynamic_cast<Asteroid*>(entityB);

                    if (bullet && asteroid) {
                        if (bullet->getBounds().intersects(asteroid->getBounds())) {
                            bullet->active = false;
                            asteroid->active = false;
                            if (asteroid->sizeLevel > 1) {
                                int newSize = asteroid->sizeLevel - 1;
                                entities.push_back(new Asteroid(asteroid->x, asteroid->y, newSize));
                                entities.push_back(new Asteroid(asteroid->x, asteroid->y, newSize));
                            }
                        }
                    }
                }
            }

            // 4. LIMPIEZA
            for (int i = entities.size() - 1; i >= 0; i--) {
                if (!entities[i]->active) {
                    delete entities[i];
                    entities.erase(i);
                }
            }
            if (IsKeyPressed(KEY_P)) showDebug = !showDebug;
        }

        // ==========================================================
        // DIBUJADO (RENDER)
        // ==========================================================
        
        BeginDrawing();
        ClearBackground(BLACK);

        if (estadoActual == MENU_PRINCIPAL) {
            // --- PANTALLA DE TITULO ---
            DrawText("ASTEROIDS", SCREEN_WIDTH/2 - MeasureText("ASTEROIDS", 60)/2, 100, 60, GREEN);
            DrawText("QUADTREE EDITION", SCREEN_WIDTH/2 - MeasureText("QUADTREE EDITION", 20)/2, 160, 20, GRAY);
            
            // Boton PLAY
            if (DibujarBoton("PLAY", SCREEN_WIDTH/2 - 100, 250, 200, 50)) {
                estadoActual = SELECCION_MODO;
            }
            
            DrawText("Mateo Llallire Meza - UTEC", 10, SCREEN_HEIGHT - 30, 15, DARKGRAY);

        } 
        else if (estadoActual == SELECCION_MODO) {
            // --- PANTALLA DE SELECCION ---
            DrawText("SELECCIONA MODO", SCREEN_WIDTH/2 - MeasureText("SELECCIONA MODO", 30)/2, 100, 30, GREEN);
            
            // Opcion 1: Solo
            if (DibujarBoton("JUGAR SOLO", SCREEN_WIDTH/2 - 100, 200, 200, 50)) {
                ReiniciarJuego(entities, false); // false = Humano
                estadoActual = JUGANDO;
            }

            // Opcion 2: Bot
            if (DibujarBoton("JUGAR CON BOT", SCREEN_WIDTH/2 - 100, 280, 200, 50)) {
                ReiniciarJuego(entities, true); // true = Bot
                estadoActual = JUGANDO;
            }
            
            DrawText("(Presiona ESC para volver)", 10, 10, 10, GRAY);
        } 
        else if (estadoActual == JUGANDO) {
            // --- JUEGO ---
            for (size_t i = 0; i < entities.size(); i++) {
                entities[i]->draw();
            }

            if (showDebug) {
                MiVector<Rect> allNodes;
                quadtree->getAllBounds(allNodes);
                for (size_t i = 0; i < allNodes.size(); i++) {
                    DrawRectangleLines((int)allNodes[i].x, (int)allNodes[i].y,
                                       (int)allNodes[i].width, (int)allNodes[i].height, Fade(GRAY, 0.4f));
                }
            }
            
            DrawText("Presiona ESC para Menu", 10, SCREEN_HEIGHT - 20, 10, GRAY);
        }

        EndDrawing();
    }

    delete quadtree;
    for (size_t i = 0; i < entities.size(); i++) delete entities[i];

    CloseWindow();
    return 0;
}
