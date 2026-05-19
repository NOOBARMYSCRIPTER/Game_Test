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
Vector2 g_playerPos = { 0.0f, 0.0f };
int g_nextEntityId = 3;

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
    for (auto& enemy : g_enemies) {
        Vector2 dir = { g_playerPos.x - enemy.position.x, g_playerPos.y - enemy.position.y };
        float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        
        if (length > 0.5f) {
            enemy.position.x += (dir.x / length) * 1.8f * deltaTime;
            enemy.position.y += (dir.y / length) * 1.8f * deltaTime;
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

// Отрисовка кадра
void DrawGame() {
    BeginDrawing();
    ClearBackground({ 20, 20, 25, 255 });

    DrawRectangle(640 - 20, 360 - 20, 40, 40, BLUE);

    for (const auto& enemy : g_enemies) {
        DrawRectangle((int)enemy.position.x - 20, (int)enemy.position.y - 20, 40, 40, RED);
    }

    EndDrawing();
}

void GameLoop() {
    float dt = GetFrameTime();
    UpdateGame(dt);
    DrawGame();
}

extern "C" {
    void EMSCRIPTEN_KEEPALIVE StartGameSession() {
        SpawnEnemy(100.0f, 360.0f);
        SpawnEnemy(1180.0f, 360.0f);
        
#ifdef __EMSCRIPTEN__
        emscripten_set_main_loop(GameLoop, 0, 1);
#endif
    }
}

int main() {
    InitWindow(1280, 720, "Math Duel: Raylib Edition");
    
#ifndef __EMSCRIPTEN__
    while (!WindowShouldClose()) {
        GameLoop();
    }
    CloseWindow();
#endif

    return 0;
}
