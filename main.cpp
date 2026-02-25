/**
 * PROYECTO: ASTEROIDS FINAL - CON STATS Y VICTORIA
 * AUTOR: Mateo Llallire Meza y Mariano Sánchez Arce
 * FECHA: 03/02/2026
 */

#include "raylib.h"
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>


// ------------------------------------------------------------
// PARTE 1: ESTRUCTURAS DE DATOS (CREACION DE VECTOR PROPIO)
// ------------------------------------------------------------

template <typename T>
class MiVector {
private:
    T* data; //puntero a arreglo dinamico
    size_t capacity; //capacidad total reservada
    size_t count; //cantidad elementos

    void resize(size_t new_capacity) {
        T* new_data = new T[new_capacity]; //reserva nueva memoria
        for (size_t i = 0; i < count; i++) {
            new_data[i] = data[i]; //copia todos los elementos
        }
        delete[] data; //libera memoria antigua
        data = new_data; //actualiza punteros
        capacity = new_capacity;
    }

public:
    MiVector() {
        capacity = 10; //capacidad por defecto
        count = 0;
        data = new T[capacity];
    }

    ~MiVector() {
        if (data != nullptr) {
            delete[] data;
            data = nullptr;
        }
    }

    void push_back(const T& element) {
        if (count >= capacity) {
            resize(capacity * 2); //duplica capacidad si se llena
        }
        data[count] = element; //se agrega elemento al final
        count++;
    }

    void clear() { //solo se reinicia contador, no se elimina memoria
        count = 0;
    }

    T& operator[](size_t index) { return data[index]; } //acceso por indice vect[i]
    const T& operator[](size_t index) const { return data[index]; }

    size_t size() const { return count; }
    bool empty() const { return count == 0; }

    void erase(size_t index) { //Se elimina elemento con indice especifico
        if (index >= count) return;
        for (size_t i = index; i < count - 1; i++) {
            data[i] = data[i + 1]; //Se mmueven todos los elementos posteriores; O(n)
        }
        count--;
    }

    //hacemos compatibles los iteradores (solo por si acaso, para poder usar: for(auto& x : lista))
    T* begin() { return data; }
    T* end() { return data + count; }
    const T* begin() const { return data; }
    const T* end() const { return data + count; }
};

// -------------------------------------------
// PARTE 2: GEOMETRIA Y ENTIDADES
// -------------------------------------------

struct Rect { //para limites del Quadtree y para las cajas de colisión de las entidades.
    float x, y;
    float width, height;
    bool intersects(const Rect& other) const {//verifica si un rectangulo intersecta con otro
        return (x < other.x + other.width && x + width > other.x &&
                y < other.y + other.height && y + height > other.y);
    }
    bool contains(const Rect& other) const {//verifica si un rect contiene otro adentro, util para saber si un objeto cabe en un nodo hijo
        return (other.x >= x &&
                other.x + other.width <= x + width &&
                other.y >= y &&
                other.y + other.height <= y + height);
    }
};

class Entity { //clase padre; derivaran otras entidades
public:
    float x, y, width, height;
    bool active; //indica s ise debe eliminar la entidad (ejm: asteroide destruido)

    Entity(float _x, float _y, float _w, float _h)
        : x(_x), y(_y), width(_w), height(_h), active(true) {}

    virtual ~Entity() = default; //destructor virtual para polimorfismo
    Rect getBounds() const { return { x, y, width, height }; } //da el rectangulo de colision actual
    virtual void update() = 0; //metodos virtuales puros para implementar despues
    virtual void draw() = 0;
};

// ------------------------------------------
// PARTE 3: QUADTREE
// -------------------------------------------

class Quadtree {
private:
    const int MAX_OBJECTS = 4; //maximo de objetos antes de dividir
    const int MAX_LEVELS = 5; //profundidad maxima del arbol; evita recursividad infinita
    int level; //nivel profundidad en arbol
    Rect bounds; //area que cubre el nodo
    MiVector<Entity*> objects; //objetos en el nodo (se usa el vector que creamos!!! yayy)
    Quadtree* nodes[4]; // Los 4 subnodos (hijos); usamos arreglo nromal;

public:
    Quadtree(int pLevel, Rect pBounds) : level(pLevel), bounds(pBounds) {
        for (int i = 0; i < 4; i++) {nodes[i] = nullptr;} //se inicializa nullptr para saber que es una hoja
    }

    ~Quadtree() {clear();}

    void clear() {
        objects.clear();
        for (int i = 0; i < 4; i++) {
            if (nodes[i] != nullptr) {
                delete nodes[i]; //llamada recursiva al destructor del hijo
                nodes[i] = nullptr;
            }
        }
    }

    void split() { //dividir el nodo en 4 cuadrantes
        float subWidth = bounds.width / 2;
        float subHeight = bounds.height / 2;
        float x = bounds.x;
        float y = bounds.y;
        nodes[0] = new Quadtree(level + 1, { x + subWidth, y, subWidth, subHeight }); //NE
        nodes[1] = new Quadtree(level + 1, { x, y, subWidth, subHeight }); //NW
        nodes[2] = new Quadtree(level + 1, { x, y + subHeight, subWidth, subHeight }); //SW
        nodes[3] = new Quadtree(level + 1, { x + subWidth, y + subHeight, subWidth, subHeight }); //SE
    }

    int getIndex(Entity* entity) {//determina en que cuadrante encaja la entidad (-1 si no encaja en ninguno y debe quedarse en el padre)

        Rect r = entity->getBounds();

        float subWidth  = bounds.width  / 2.0f;
        float subHeight = bounds.height / 2.0f;
        float x = bounds.x;
        float y = bounds.y;

        // Crear rectangulos de los 4 hijos
        Rect ne = { x + subWidth, y, subWidth, subHeight };
        Rect nw = { x, y, subWidth, subHeight };
        Rect sw = { x, y + subHeight, subWidth, subHeight };
        Rect se = { x + subWidth, y + subHeight, subWidth, subHeight };

        if (ne.contains(r)) return 0;
        if (nw.contains(r)) return 1;
        if (sw.contains(r)) return 2;
        if (se.contains(r)) return 3;

        return -1; // no cabe completamente en ningun hijo
    }

    void insert(Entity* pRect) {
        if (nodes[0] != nullptr) { //Si no es hoja, intento insert en uno de lso hijos
            int index = getIndex(pRect);
            if (index != -1) { nodes[index]->insert(pRect); return; } //Si cabe en hijo, se inserta ahi
        }
        objects.push_back(pRect); //Si no cabe en el hijo, guardar en el nodo actual
        if (objects.size() > MAX_OBJECTS && level < MAX_LEVELS) {//verifico si excedi la capacidad y si puedo dividir mas
            if (nodes[0] == nullptr) split(); //Si no tiene hijos, se crean
            int i = 0;
            while (i < objects.size()) { //redistribuir las entidades a los nuevos hijos si es posible
                int index = getIndex(objects[i]);
                if (index != -1) { //si se puede, mover al hijo
                    nodes[index]->insert(objects[i]);
                    objects.erase(i);  //se saca de mi lista
                } else { i++; } //si no cabe en hijos, se queda aca y avanzo al sgte nodo
            }
        }
    }

    //recupera todos los objetos que podrian colisionar con el objeto dado (devuelve lista de cercanos)
    void retrieve(MiVector<Entity*>& returnObjects, Entity* pRect) {
        int index = getIndex(pRect); //si hay hijos, buscar en el cuadrante correspondiente
        if (index != -1 && nodes[0] != nullptr) {nodes[index]->retrieve(returnObjects, pRect);} //se recorre recursivamente la rama el el qt y se extraen los elementos
        for (size_t i = 0; i < objects.size(); i++) {returnObjects.push_back(objects[i]);} //se agregan todos los objetos almacenados en este nodo
        if (index == -1 && nodes[0] != nullptr) { //si la entidad no encaja en los hijos, se agrega los objetos de TODOS los hijos (podria chocar con cualquiera)
            for(int i=0; i<4; i++) nodes[i]->retrieve(returnObjects, pRect);
        }
    }

    //para dibujar las lineas del Quadtree; usaremos "DrawRectangleLines" de raylib, pero
    //para simplificar, haremos un metodo recursivo que llena un vector de Rects para dibujar luego.
    void getAllBounds(MiVector<Rect>& list) {
        list.push_back(bounds);
        if (nodes[0] != nullptr) {
            for (int i = 0; i < 4; i++) nodes[i]->getAllBounds(list);
        }
    }
};

// --------------------------------------
// PARTE 4: CLASES DE JUEGO (HERENCIA)
// --------------------------------------

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 500;


struct Vec2 { //para física de vectores
    float x, y;
};


class Bullet : public Entity {
public:
    Vec2 velocity;
    float lifeTime; //desaparece despues de cierto tiempo
    Bullet(float _x, float _y, float angle) : Entity(_x, _y, 5, 5) {
        float speed = 1100.0f;
        velocity.x = cos(angle * DEG2RAD) * speed;
        velocity.y = sin(angle * DEG2RAD) * speed;
        lifeTime = 0.45f;
    }
    void update() override { //actualiza dependiendo de los FPS
        float dt = GetFrameTime();
        x += velocity.x * dt;
        y += velocity.y * dt;
        lifeTime -= dt;
        if (lifeTime <= 0) {active = false;} //para borrar
        if (x > SCREEN_WIDTH) {x = 0;} if (x < 0) {x = SCREEN_WIDTH;} //para que no desaparezca de la pantalla
        if (y > SCREEN_HEIGHT) {y = 0;} if (y < 0) {y = SCREEN_HEIGHT;}
    }
    void draw() override { DrawRectangle((int)x, (int)y, (int)width, (int)height, YELLOW); } //dibuja la bala
};

class Asteroid : public Entity {
public:
    Vec2 velocity;
    int sizeLevel; // 3 = Grande, 2 = Mediano, 1 = Pequenio
    Vector2 shapePoints[12]; //Guardamos los "offsets" (distancias) desde el centro
    int totalPoints;

    Asteroid(float _x, float _y, int _size) : Entity(_x, _y, 0, 0), sizeLevel(_size) {
        float baseRadius = (sizeLevel == 3) ? 30 : (sizeLevel == 2 ? 20 : 10); //radio del asteroide dependiendo de su tamanio
        width = height = baseRadius * 2;
        float speed = (sizeLevel == 3) ? 100 : (sizeLevel == 2 ? 200 : 300); //velocidad del asteroide dependiendo de su tamanio
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
    void update() override { //actualizacion dependiente de FPS
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
    float invulnerabilityTime; //Si hay colision con asteroide

    Player(MiVector<Entity*>* entitiesPtr, int* bulletsRef)
        : Entity(SCREEN_WIDTH/2, SCREEN_HEIGHT/2, 20, 20), gameEntitiesRef(entitiesPtr), bulletsCounterRef(bulletsRef) {
        velocity = {0, 0}; rotation = 0; acceleration = 1000.0f; friction = 0.98f; invulnerabilityTime = 0.0f;
    }

    void update() override {
        float dt = GetFrameTime();
        if (invulnerabilityTime > 0) //Para ser invulnerable al ser chocado
            invulnerabilityTime -= GetFrameTime();

        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) {rotation -= 300.0f * dt;}
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) {rotation += 300.0f * dt;}
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) {
            velocity.x += cos(rotation * DEG2RAD) * acceleration * dt;
            velocity.y += sin(rotation * DEG2RAD) * acceleration * dt;
        }
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) {
            velocity.x -= cos(rotation * DEG2RAD) * acceleration * dt;
            velocity.y -= sin(rotation * DEG2RAD) * acceleration * dt;
        }
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_KP_0)){shoot();}

        x += velocity.x * dt; y += velocity.y * dt; //actualizar posicion
        velocity.x *= friction; velocity.y *= friction; //Disminuyendo velocidad por la friccion para proxima actualizacion
        if (x > SCREEN_WIDTH) x = 0; if (x < 0) x = SCREEN_WIDTH;
        if (y > SCREEN_HEIGHT) y = 0; if (y < 0) y = SCREEN_HEIGHT;
    }

    void shoot() {
        float cx = x + width/2, cy = y + height/2;
        gameEntitiesRef->push_back(new Bullet(cx, cy, rotation));
        // AUMENTAR CONTADOR DE BALAS
        if (bulletsCounterRef) (*bulletsCounterRef)++;
    }

    void draw() override {
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
        acceleration = 1100.0f; friction = 0.75f; invulnerabilityTime = 0.0f;
    }

    void update() override {
        float dt = GetFrameTime();
        if (invulnerabilityTime > 0)
            invulnerabilityTime -= dt;
        Entity* target = nullptr;
        float minDistance = 10000.0f;

        for (size_t i = 0; i < gameEntitiesRef->size(); i++) {
            Entity* e = (*gameEntitiesRef)[i];
            if (e == this || !e->active) continue; //ignora a si mismo y entidades inactivas
            if (dynamic_cast<Asteroid*>(e)) { //solo considera asteroides
                float dx = e->x - x, dy = e->y - y; //calculo distancia euclidiana
                float dist = sqrt(dx*dx + dy*dy);
                if (dist < minDistance) { minDistance = dist; target = e; } //si es menor al actual, se vuelve el objetivo
            } //Siempre elige el mas cercano
        }

        if (target != nullptr) {
            Asteroid* ast = dynamic_cast<Asteroid*>(target);
            float timeToHit = minDistance / 1100.0f; //aproxima el tiempo en el que el disparo llegara con velocidad de la bala
            float fx = target->x + (ast->velocity.x * timeToHit) + target->width/2; //predice direcciones del asteroide
            float fy = target->y + (ast->velocity.y * timeToHit) + target->height/2;
            float desiredAngle = atan2(fy - (y+height/2), fx - (x+width/2)) * RAD2DEG; //obtiene el angulo deseado
            if (desiredAngle < 0) desiredAngle += 360;

            float diff = desiredAngle - rotation;
            while (diff > 180) diff -= 360; while (diff < -180) diff += 360; //normaliza entre -180 y 180 para que no rote por el camino largo

            if (diff > 0) rotation += 300.0f * dt; else rotation -= 300.0f * dt; //rota hacia el angulo deseado

            if (abs(diff) < 30.0f && (rand() % 100) < 30) shoot(); //si esta alineado a ma so menos 30 grados, dispara un 30% de los frames

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

// -----------------------------------------
// CONTROL DE ESTADO
// -----------------------------------------

void ReiniciarJuego(MiVector<Entity*>& entities, bool modoBot, int* bulletsRef, float* timeRef, int*vecesPerdidas) {
    for (size_t i = 0; i < entities.size(); i++) delete entities[i];
    entities.clear();

    // RESETEAR STATS
    *bulletsRef = 0;
    *timeRef = 0.0f;
    *vecesPerdidas = 0;

    if (modoBot) entities.push_back(new BotPlayer(&entities, bulletsRef));
    else entities.push_back(new Player(&entities, bulletsRef));

    for (int i = 0; i < 10; i++) { //Cantidad de ASTEROIDES
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
    bool showHitboxes = false;

    // VARIABLES DE ESTADISTICAS
    int balasDisparadas = 0;
    float tiempoJuego = 0.0f;
    int vecesPerdidas = 0;

    while (!WindowShouldClose()) {

        if (estadoActual == JUGANDO) {
            if (IsKeyPressed(KEY_ESCAPE)) estadoActual = MENU_PRINCIPAL;
            if (IsKeyPressed(KEY_P)) showDebug = !showDebug;
            if (IsKeyPressed(KEY_H)) showHitboxes = !showHitboxes;

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
                    //Para detectar veces que perdiste
                    Player* p = dynamic_cast<Player*>(entityA);
                    Asteroid* ast = dynamic_cast<Asteroid*>(entityB);

                    if (p && ast && p->invulnerabilityTime <= 0 && p->getBounds().intersects(ast->getBounds())) {
                        vecesPerdidas++;
                        p->invulnerabilityTime = 1.5f; // 1 segundo invulnerable
                        }
                    if (b && a && b->getBounds().intersects(a->getBounds())) { //EN CUANTOS SE DIVIDE LOS ASTEORIDES
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
                ReiniciarJuego(entities, false, &balasDisparadas, &tiempoJuego, &vecesPerdidas);
                estadoActual = JUGANDO;
            }
            if (DibujarBoton("CON BOT", SCREEN_WIDTH/2 - 100, 280, 200, 50)) {
                ReiniciarJuego(entities, true, &balasDisparadas, &tiempoJuego, &vecesPerdidas);
                estadoActual = JUGANDO;
            }

        } else if (estadoActual == JUGANDO) {
            for (size_t i = 0; i < entities.size(); i++) entities[i]->draw();
            if (showDebug) {
                MiVector<Rect> nodes; quadtree->getAllBounds(nodes);
                for (size_t i = 0; i < nodes.size(); i++) DrawRectangleLines(nodes[i].x, nodes[i].y, nodes[i].width, nodes[i].height, Fade(GRAY, 0.3f));
            }
            if (showHitboxes) {
                for (size_t i = 0; i < entities.size(); i++) {
                    // Solo Player y Asteroid
                    if (dynamic_cast<Player*>(entities[i]) ||
                        dynamic_cast<Asteroid*>(entities[i])) {
                        Rect r = entities[i]->getBounds();
                        DrawRectangleLines(r.x,r.y,r.width,r.height,RED);
                        }
                }
            }
            // HUD EN TIEMPO REAL
            DrawText(TextFormat("Balas: %i", balasDisparadas), 10, 10, 20, YELLOW);
            DrawText(TextFormat("Tiempo: %.1f", tiempoJuego), 10, 35, 20, YELLOW);
            DrawText(TextFormat("Perdidas: %i", vecesPerdidas), 10, 60, 20, RED);

        } else if (estadoActual == RESULTADOS) {
            // PANTALLA DE VICTORIA
            DrawText("MISION COMPLETADA", SCREEN_WIDTH/2 - MeasureText("MISION COMPLETADA", 40)/2, 100, 40, GOLD);
            
            DrawText(TextFormat("Tiempo Total: %.2f seg", tiempoJuego), SCREEN_WIDTH/2 - 150, 180, 30, WHITE);
            DrawText(TextFormat("Balas Usadas: %i", balasDisparadas), SCREEN_WIDTH/2 - 150, 230, 30, WHITE);
            DrawText(TextFormat("Veces que perdiste: %i", vecesPerdidas), SCREEN_WIDTH/2 - 150, 280, 30, RED);

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
