#include "raylib.h"
#include <string>
#include <vector>
#include <cmath>
#include <random>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

struct Enemy {
    int id;
    Vector2 position;
    std::string challenge_text;
    int expected_answer;
};

std::vector<Enemy> g_enemies;
Vector2 g_playerPos = { 640.0f, 360.0f };
int g_nextEntityId = 3;
bool g_sessionStarted = false;

std::string GenerateChallenge(int& out_answer) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(1, 10);
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

    for (auto& enemy : g_enemies) {
        Vector2 dir = { g_playerPos.x - enemy.position.x, g_playerPos.y - enemy.position.y };
        float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        
        if (length > 10.0f) { 
            enemy.position.x += (dir.x / length) * 100.0f * deltaTime;
            enemy.position.y += (dir.y / length) * 100.0f * deltaTime;
        }

        float pctX = enemy.position.x / 1280.0f;
        float pctY = enemy.position.y / 720.0f;

#ifdef __EMSCRIPTEN__
        MAIN_THREAD_EM_ASM({
            if (window.Module && window.Module.updateMonsterUI) {
                window.Module.updateMonsterUI($0, UTF8ToString($1), $2, $3);
            }
        }, enemy.id, enemy.challenge_text.c_str(), pctX, pctY);
#endif
    }
}

void DrawGame() {
    BeginDrawing();
    ClearBackground({ 20, 20, 25, 255 });

    if (g_sessionStarted) {
        DrawRectangle((int)g_playerPos.x - 20, (int)g_playerPos.y - 20, 40, 40, BLUE);

        for (const auto& enemy : g_enemies) {
            DrawRectangle((int)enemy.position.x - 20, (int)enemy.position.y - 20, 40, 40, RED);
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
        SpawnEnemy(100.0f, 360.0f);
        SpawnEnemy(1180.0f, 360.0f);
        g_sessionStarted = true;
    }
}

int main() {
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
