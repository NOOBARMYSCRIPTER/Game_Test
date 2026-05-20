#include "raylib.h"
#include <string>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

struct Enemy {
    int id;
    Vector2 position;
    std::string challenge_text;
    int expected_answer;
};

struct Projectile {
    Vector2 position;
    Vector2 target_pos;
    int target_enemy_id;
    float speed;
    bool active;
};

std::vector<Enemy> g_enemies;
std::vector<Projectile> g_projectiles;
Vector2 g_playerPos = { 640.0f, 360.0f };
int g_nextEntityId = 3;
bool g_sessionStarted = false;

std::string GenerateChallenge(int& out_answer) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(1, 5);
    int a = dist(gen);
    int b = dist(gen);
    out_answer = a + b;
    return std::to_string(a) + " + " + std::to_string(b);
}

void SpawnEnemy(float x, float y) {
    Enemy e;
    e.id = g_nextEntityId++;
    e.position = { x, y };
    e.challenge_text = GenerateChallenge(e.expected_answer);
    g_enemies.push_back(e);
}

void UpdateGame(float deltaTime) {
    if (!g_sessionStarted) return;

    float currentWidth = (float)GetScreenWidth();
    float currentHeight = (float)GetScreenHeight();

    g_playerPos = { currentWidth / 2.0f, currentHeight / 2.0f };

    for (auto& enemy : g_enemies) {
        Vector2 dir = { g_playerPos.x - enemy.position.x, g_playerPos.y - enemy.position.y };
        float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        
        if (length < 40.0f) {
            g_sessionStarted = false;
#ifdef __EMSCRIPTEN__
            MAIN_THREAD_EM_ASM({
                if (window.Module && window.Module.showGameOver) {
                    window.Module.showGameOver();
                }
            });
#endif
            return; 
        }

        if (length > 10.0f) { 
            enemy.position.x += (dir.x / length) * 60.0f * deltaTime;
            enemy.position.y += (dir.y / length) * 60.0f * deltaTime;
        }

        float pctX = enemy.position.x / currentWidth;
        float pctY = enemy.position.y / currentHeight;

#ifdef __EMSCRIPTEN__
        MAIN_THREAD_EM_ASM({
            if (window.Module && window.Module.updateMonsterUI) {
                window.Module.updateMonsterUI($0, UTF8ToString($1), $2, $3);
            }
        }, enemy.id, enemy.challenge_text.c_str(), pctX, pctY);
#endif
    }

    for (auto& proj : g_projectiles) {
        if (!proj.active) continue;

        auto it = std::find_if(g_enemies.begin(), g_enemies.end(), [&](const Enemy& e) {
            return e.id == proj.target_enemy_id;
        });

        Vector2 target = (it != g_enemies.end()) ? it->position : proj.target_pos;
        Vector2 dir = { target.x - proj.position.x, target.y - proj.position.y };
        float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);

        if (length < 15.0f) {
            proj.active = false;
            if (it != g_enemies.end()) {
#ifdef __EMSCRIPTEN__
                MAIN_THREAD_EM_ASM({
                    if (window.Module && window.Module.removeMonsterUI) {
                        window.Module.removeMonsterUI($0);
                    }
                }, it->id);
#endif
                g_enemies.erase(it);

                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> sideDist(0, 1);
                std::uniform_real_distribution<float> yDist(100.0f, currentHeight - 100.0f);
                float spawnX = sideDist(gen) == 0 ? 50.0f : (currentWidth - 50.0f);
                SpawnEnemy(spawnX, yDist(gen));
            }
        } else {
            proj.position.x += (dir.x / length) * proj.speed * deltaTime;
            proj.position.y += (dir.y / length) * proj.speed * deltaTime;
        }
    }

    g_projectiles.erase(std::remove_if(g_projectiles.begin(), g_projectiles.end(), 
        [](const Projectile& p) { return !p.active; }), g_projectiles.end());
}

void DrawGame() {
    BeginDrawing();
    ClearBackground({ 20, 20, 25, 255 });

    if (g_sessionStarted) {
        DrawRectangle((int)g_playerPos.x - 20, (int)g_playerPos.y - 20, 40, 40, BLUE);

        for (const auto& enemy : g_enemies) {
            DrawRectangle((int)enemy.position.x - 20, (int)enemy.position.y - 20, 40, 40, RED);
        }

        for (const auto& proj : g_projectiles) {
            if (proj.active) {
                DrawCircle((int)proj.position.x, (int)proj.position.y, 8.0f, YELLOW);
            }
        }
    }

    EndDrawing();
}

void GameLoop() {
    float dt = GetFrameTime();
    if (dt > 0.1f) dt = 0.1f; 
    
    UpdateGame(dt);
    DrawGame();
}

extern "C" {
    void EMSCRIPTEN_KEEPALIVE StartGameSession() {
        g_enemies.clear();
        g_projectiles.clear();
        g_nextEntityId = 3;
        
        float w = (float)GetScreenWidth();
        float h = (float)GetScreenHeight();

        SpawnEnemy(100.0f, 200.0f);
        SpawnEnemy(w - 100.0f, h - 200.0f);
        g_sessionStarted = true;
    }

    void EMSCRIPTEN_KEEPALIVE StartGameSession() {
        g_enemies.clear();
        g_projectiles.clear();
        g_nextEntityId = 3;
        SpawnEnemy(100.0f, 200.0f);
        SpawnEnemy(1180.0f, 500.0f);
        g_sessionStarted = true;
    }

    void EMSCRIPTEN_KEEPALIVE RegisterGestureResult(int fingersCount) {
        if (!g_sessionStarted) return;

        for (const auto& enemy : g_enemies) {
            if (enemy.expected_answer == fingersCount) {
                Projectile p;
                p.position = g_playerPos;
                p.target_pos = enemy.position;
                p.target_enemy_id = enemy.id;
                p.speed = 500.0f;
                p.active = true;
                g_projectiles.push_back(p);
                break;
            }
        }
    }
}

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Math Duel: Raylib Edition");
    
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(GameLoop, 0, 1);
#else
    while (!WindowShouldClose()) {
        GameLoop();
    }
    CloseWindow();
#endif

    return 0;
}
