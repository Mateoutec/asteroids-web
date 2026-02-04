/**
 * PROYECTO: ASTEROIDS FINAL - CON STATS Y VICTORIA
 * AUTOR: Mateo Llallire Meza
 * FECHA: 03/02/2026
 */

#include "raylib.h"
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string> // Necesario para std::to_string si usamos formatos avanzados, pero usaremos TextFormat de Raylib

// ==========================================
// PARTE 1: ESTRUCTURAS DE DATOS
// ==========================================

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
// PARTE 2: GEOMETRÍA Y ENTIDADES
// ==========================================

struct Rect {
    float x, y, width, height;
    bool intersects(const Rect& other) const {
        return (x < other.x + other.width && x + width > other.x &&
                y < other.y + other.height && y + height > other.y);
    }
};

class Entity {
public:
    float x, y, width, height;
    bool active;

    Entity(float _x, float _y, float _w, float _h)
        : x(_x), y(_y), width(_w), height(_h), active(true) {}

    virtual ~Entity() = default;
    Rect getBounds() const { return { x, y, width, height }; }
    virtual void update() = 0;
    virtual void draw() = 0;
};

// ==========================================
// PARTE 3: QUADTREE
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

    ~Quadtree() { clear(); }

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
        nodes[0] = new Quadtree(level + 1, { x + subWidth, y, subWidth, subHeight });
        nodes[1] = new Quadtree(level + 1, { x, y, subWidth, subHeight });
        nodes[2] = new Quadtree(level + 1, { x, y + subHeight, subWidth, subHeight });
        nodes[3] = new Quadtree(level + 1, { x + subWidth, y + subHeight, subWidth, subHeight });
    }

    int getIndex(Entity* pRect) {
        int index = -1;
        double vMid = bounds.x + (bounds.width / 2);
        double hMid = bounds.y + (bounds.height / 2);
        Rect r = pRect->getBounds();
        bool top = (r.y < hMid && r.y + r.height < hMid);
        bool bot = (r.y > hMid);

        if (r.x < vMid && r.x + r.width < vMid) {
            if (top) index = 1; else if (bot) index = 2;
        } else if (r.x > vMid) {
            if (top) index = 0; else if (bot) index = 3;
        }
        return index;
    }

    void insert(Entity* pRect) {
        if (nodes[0] != nullptr) {
            int index = getIndex(pRect);
            if (index != -1) { nodes[index]->insert(pRect); return; }
        }
        objects.push_back(pRect);
        if (objects.size() > MAX_OBJECTS && level < MAX_LEVELS) {
            if (nodes[0] == nullptr) split();
            int i = 0;
            while (i < objects.size()) {
                int index = getIndex(objects[i]);
                if (index != -1) {
                    nodes[index]->insert(objects[i]);
                    objects.erase(i);
                } else { i++; }
            }
        }
    }

    void retrieve(MiVector<Entity*>& returnObjects, Entity* pRect) {
        int index = getIndex(pRect);
        if (index != -1 && nodes[0] != nullptr) nodes[index]->retrieve(returnObjects, pRect);
        for (size_t i = 0; i < objects.size(); i++) returnObjects.push_back(objects[i]);
        if (index == -1 && nodes[0] != nullptr) {
            for(int i=0; i<4; i++) nodes[i]->retrieve(returnObjects, pRect);
        }
    }

    void getAllBounds(MiVector<Rect>& list) {
        list.push_back(bounds);
        if (nodes[0] != nullptr) {
            for (int i = 0; i < 4; i++) nodes[i]->getAllBounds(list);
        }
    }
};

// ==========================================
// PARTE 4: CLASES DE JUEGO
// ==========================================

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 500;

struct Vec2 { float x, y; };

class Bullet : public Entity {
public:
    Vec2 velocity;
    float lifeTime;
    Bullet(float _x, float _y, float angle) : Entity(_x, _y, 5, 5) {
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
        if (x > SCREEN_WIDTH) {x = 0;} if (x < 0) {x = SCREEN_WIDTH;}
        if (y > SCREEN_HEIGHT) {y = 0;} if (y < 0) {y = SCREEN_HEIGHT;}
    }
    void draw() override { DrawRectangle((int)x, (int)y, (int)width, (int)height, YELLOW); }
};

class Asteroid : public Entity {
public:
    Vec2 velocity;
    int sizeLevel;
    Vector2 shapePoints[12];
    int totalPoints;

    Asteroid(float _x, float _y, int _size) : Entity(_x, _y, 0, 0), sizeLevel(_size) {
        float baseRadius = (sizeLevel == 3) ? 30 : (sizeLevel == 2 ? 20 : 10);
        width = height = baseRadius * 2;
        float speed = (sizeLevel == 3) ? 100 : (sizeLevel == 2 ? 150 : 250);
        float moveAngle = (float)(rand() % 360);
        velocity.x = cos(moveAngle * DEG2RAD) * speed;
        velocity.y = sin(moveAngle * DEG2RAD) * speed;

        totalPoints = 8 + (rand() % 5);
        for(int i = 0; i < totalPoints; i++) {
            float angle = (360.0f / totalPoints) * i;
            float r = baseRadius * (1.0f + ((rand() % 60) - 30) / 100.0f);
            shapePoints[i].x = cos(angle * DEG2RAD) * r;
            shapePoints[i].y = sin(angle * DEG2RAD) * r;
        }
    }
    void update() override {
        float dt = GetFrameTime();
        x += velocity.x * dt;
        y += velocity.y * dt;
        if (x > SCREEN_WIDTH) x = -width; if (x < -width) x = SCREEN_WIDTH;
        if (y > SCREEN_HEIGHT) y = -height; if (y < -height) y = SCREEN_HEIGHT;
    }
    void draw() override {
        float cx = x + width/2, cy = y + height/2;
        for (int i = 0; i < totalPoints; i++) {
            int next = (i + 1) % totalPoints;
            DrawLine(cx + shapePoints[i].x, cy + shapePoints[i].y,
                     cx + shapePoints[next].x, cy + shapePoints[next].y, WHITE);
        }
    }
};

class Player : public Entity {
public:
    Vec2 velocity;
    float rotation, acceleration, friction;
    MiVector<Entity*>* gameEntitiesRef;
    int* bulletsCounterRef; // Referencia al contador de balas

    Player(MiVector<Entity*>* entitiesPtr, int* bulletsRef)
        : Entity(SCREEN_WIDTH/2, SCREEN_HEIGHT/2, 20, 20), gameEntitiesRef(entitiesPtr), bulletsCounterRef(bulletsRef) {
        velocity = {0, 0}; rotation = 0; acceleration = 900.0f; friction = 0.98f;
    }

    virtual void update() override {
        float dt = GetFrameTime();
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) rotation -= 270.0f * dt;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) rotation += 270.0f * dt;
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) {
            velocity.x += cos(rotation * DEG2RAD) * acceleration * dt;
            velocity.y += sin(rotation * DEG2RAD) * acceleration * dt;
        }
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_KP_0)) shoot();

        x += velocity.x * dt; y += velocity.y * dt;
        velocity.x *= friction; velocity.y *= friction;
        if (x > SCREEN_WIDTH) x = 0; if (x < 0) x = SCREEN_WIDTH;
        if (y > SCREEN_HEIGHT) y = 0; if (y < 0) y = SCREEN_HEIGHT;
    }

    void shoot() {
        float cx = x + width/2, cy = y + height/2;
        gameEntitiesRef->push_back(new Bullet(cx, cy, rotation));
        // AUMENTAR CONTADOR DE BALAS
        if (bulletsCounterRef) (*bulletsCounterRef)++;
    }

    virtual void draw() override {
        DrawTriangleLines(
            (Vector2){x+width/2 + (float)cos(rotation*DEG2RAD)*15, y+height/2 + (float)sin(rotation*DEG2RAD)*15},
            (Vector2){x+width/2 + (float)cos((rotation+140)*DEG2RAD)*10, y+height/2 + (float)sin((rotation+140)*DEG2RAD)*10},
            (Vector2){x+width/2 + (float)cos((rotation-140)*DEG2RAD)*10, y+height/2 + (float)sin((rotation-140)*DEG2RAD)*10},
            GREEN
        );
    }
};

class BotPlayer : public Player {
public:
    BotPlayer(MiVector<Entity*>* entitiesPtr, int* bulletsRef) : Player(entitiesPtr, bulletsRef) {
        acceleration = 1800.0f; friction = 0.94f;
    }

    void update() override {
        float dt = GetFrameTime();
        Entity* target = nullptr;
        float minDistance = 100000.0f;

        for (size_t i = 0; i < gameEntitiesRef->size(); i++) {
            Entity* e = (*gameEntitiesRef)[i];
            if (e == this || !e->active) continue;
            if (dynamic_cast<Asteroid*>(e)) {
                float dx = e->x - x, dy = e->y - y;
                float dist = sqrt(dx*dx + dy*dy);
                if (dist < minDistance) { minDistance = dist; target = e; }
            }
        }

        if (target != nullptr) {
            Asteroid* ast = dynamic_cast<Asteroid*>(target);
            float timeToHit = minDistance / 1100.0f;
            float fx = target->x + (ast->velocity.x * timeToHit) + target->width/2;
            float fy = target->y + (ast->velocity.y * timeToHit) + target->height/2;
            float desiredAngle = atan2(fy - (y+height/2), fx - (x+width/2)) * RAD2DEG;
            if (desiredAngle < 0) desiredAngle += 360;

            float diff = desiredAngle - rotation;
            while (diff > 180) diff -= 360; while (diff < -180) diff += 360;

            if (diff > 0) rotation += 400.0f * dt; else rotation -= 400.0f * dt;

            if (abs(diff) < 10.0f && (rand() % 100) < 15) shoot();

            if (minDistance < 180.0f) {
                velocity.x -= cos(desiredAngle * DEG2RAD) * acceleration * dt;
                velocity.y -= sin(desiredAngle * DEG2RAD) * acceleration * dt;
            } else {
                velocity.x *= friction; velocity.y *= friction;
            }
        }
        x += velocity.x * dt; y += velocity.y * dt;
        if (x > SCREEN_WIDTH) x = 0; if (x < 0) x = SCREEN_WIDTH;
        if (y > SCREEN_HEIGHT) y = 0; if (y < 0) y = SCREEN_HEIGHT;
    }
    void draw() override {
        Player::draw();
        DrawLine(x+width/2, y+height/2, x+width/2 + cos(rotation*DEG2RAD)*800, y+height/2 + sin(rotation*DEG2RAD)*800, Fade(PURPLE, 0.5f));
    }
};

// ==========================================
// CONTROL DE ESTADO
// ==========================================

void ReiniciarJuego(MiVector<Entity*>& entities, bool modoBot, int* bulletsRef, float* timeRef) {
    for (size_t i = 0; i < entities.size(); i++) delete entities[i];
    entities.clear();

    // RESETEAR STATS
    *bulletsRef = 0;
    *timeRef = 0.0f;

    if (modoBot) entities.push_back(new BotPlayer(&entities, bulletsRef));
    else entities.push_back(new Player(&entities, bulletsRef));

    for (int i = 0; i < 8; i++) {
        float x = (rand() % SCREEN_WIDTH), y = (rand() % SCREEN_HEIGHT);
        if (abs(x - SCREEN_WIDTH/2) < 100) x += 200;
        entities.push_back(new Asteroid(x, y, 3));
    }
}

bool DibujarBoton(const char* texto, float x, float y, float w, float h) {
    Vector2 mouse = GetMousePosition();
    Rectangle r = { x, y, w, h };
    bool hover = CheckCollisionPointRec(mouse, r);
    DrawRectangleRec(r, hover ? DARKGREEN : BLACK);
    DrawRectangleLinesEx(r, 2, hover ? GREEN : DARKGREEN);
    DrawText(texto, x + (w - MeasureText(texto, 20))/2, y + (h - 20)/2, 20, hover ? WHITE : GREEN);
    return hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

enum EstadoJuego { MENU_PRINCIPAL, SELECCION_MODO, JUGANDO, RESULTADOS };

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "UTEC - Asteroids Stats");
    SetTargetFPS(60);

    EstadoJuego estadoActual = MENU_PRINCIPAL;
    MiVector<Entity*> entities;
    Rect screenRect = { 0, 0, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT };
    Quadtree* quadtree = new Quadtree(0, screenRect);
    bool showDebug = false;

    // VARIABLES DE ESTADISTICAS
    int balasDisparadas = 0;
    float tiempoJuego = 0.0f;

    while (!WindowShouldClose()) {

        if (estadoActual == JUGANDO) {
            if (IsKeyPressed(KEY_ESCAPE)) estadoActual = MENU_PRINCIPAL;
            if (IsKeyPressed(KEY_P)) showDebug = !showDebug;

            // AUMENTAR TIEMPO
            tiempoJuego += GetFrameTime();

            // UPDATE
            int asteroidesVivos = 0;
            for (size_t i = 0; i < entities.size(); i++) {
                entities[i]->update();
                if (dynamic_cast<Asteroid*>(entities[i])) asteroidesVivos++;
            }

            // CHECK VICTORIA
            if (asteroidesVivos == 0) {
                estadoActual = RESULTADOS;
            }

            // QUADTREE & COLISIONES
            quadtree->clear();
            for (size_t i = 0; i < entities.size(); i++) if (entities[i]->active) quadtree->insert(entities[i]);

            MiVector<Entity*> returnObjects;
            for (size_t i = 0; i < entities.size(); i++) {
                Entity* entityA = entities[i];
                if (!entityA->active) continue;
                returnObjects.clear();
                quadtree->retrieve(returnObjects, entityA);

                for (size_t j = 0; j < returnObjects.size(); j++) {
                    Entity* entityB = returnObjects[j];
                    if (entityA == entityB) continue;
                    Bullet* b = dynamic_cast<Bullet*>(entityA);
                    Asteroid* a = dynamic_cast<Asteroid*>(entityB);
                    if (b && a && b->getBounds().intersects(a->getBounds())) {
                        b->active = false;
                        a->active = false;
                        if (a->sizeLevel > 1) {
                            entities.push_back(new Asteroid(a->x, a->y, a->sizeLevel - 1));
                            entities.push_back(new Asteroid(a->x, a->y, a->sizeLevel - 1));
                        }
                    }
                }
            }

            // LIMPIEZA
            for (int i = entities.size() - 1; i >= 0; i--) {
                if (!entities[i]->active) {
                    delete entities[i];
                    entities.erase(i);
                }
            }
        }

        // RENDER
        BeginDrawing();
        ClearBackground(BLACK);

        if (estadoActual == MENU_PRINCIPAL) {
            DrawText("ASTEROIDS", SCREEN_WIDTH/2 - MeasureText("ASTEROIDS", 60)/2, 100, 60, GREEN);
            if (DibujarBoton("PLAY", SCREEN_WIDTH/2 - 100, 250, 200, 50)) estadoActual = SELECCION_MODO;

        } else if (estadoActual == SELECCION_MODO) {
            DrawText("SELECCIONA MODO", SCREEN_WIDTH/2 - MeasureText("SELECCIONA MODO", 30)/2, 100, 30, GREEN);
            if (DibujarBoton("SOLO", SCREEN_WIDTH/2 - 100, 200, 200, 50)) {
                ReiniciarJuego(entities, false, &balasDisparadas, &tiempoJuego);
                estadoActual = JUGANDO;
            }
            if (DibujarBoton("CON BOT", SCREEN_WIDTH/2 - 100, 280, 200, 50)) {
                ReiniciarJuego(entities, true, &balasDisparadas, &tiempoJuego);
                estadoActual = JUGANDO;
            }

        } else if (estadoActual == JUGANDO) {
            for (size_t i = 0; i < entities.size(); i++) entities[i]->draw();
            if (showDebug) {
                MiVector<Rect> nodes; quadtree->getAllBounds(nodes);
                for (size_t i = 0; i < nodes.size(); i++) DrawRectangleLines(nodes[i].x, nodes[i].y, nodes[i].width, nodes[i].height, Fade(GRAY, 0.3f));
            }
            // HUD EN TIEMPO REAL
            DrawText(TextFormat("Balas: %i", balasDisparadas), 10, 10, 20, YELLOW);
            DrawText(TextFormat("Tiempo: %.1f", tiempoJuego), 10, 35, 20, YELLOW);

        } else if (estadoActual == RESULTADOS) {
            // PANTALLA DE VICTORIA
            DrawText("MISION COMPLETADA", SCREEN_WIDTH/2 - MeasureText("MISION COMPLETADA", 40)/2, 100, 40, GOLD);
            
            DrawText(TextFormat("Tiempo Total: %.2f seg", tiempoJuego), SCREEN_WIDTH/2 - 150, 180, 30, WHITE);
            DrawText(TextFormat("Balas Usadas: %i", balasDisparadas), SCREEN_WIDTH/2 - 150, 230, 30, WHITE);
            
            // CALCULO DE EFICIENCIA
            float precision = (balasDisparadas > 0) ? (100.0f / balasDisparadas) * 10.0f : 0; // Formula simple de "score"
            if (precision > 100) precision = 100;
            
            if (DibujarBoton("MENU PRINCIPAL", SCREEN_WIDTH/2 - 120, 350, 240, 50)) {
                estadoActual = MENU_PRINCIPAL;
            }
        }

        EndDrawing();
    }

    delete quadtree;
    for (size_t i = 0; i < entities.size(); i++) delete entities[i];
    CloseWindow();
    return 0;
}
