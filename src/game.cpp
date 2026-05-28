#include "raylib.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static std::mt19937 g_rng{ std::random_device{}() };

static float Randf(float a, float b) {
    std::uniform_real_distribution<float> dist(a, b);
    return dist(g_rng);
}

static int Randi(int a, int b) {
    std::uniform_int_distribution<int> dist(a, b);
    return dist(g_rng);
}

static Vector2 V2(float x, float y) { return Vector2{ x, y }; }
static Vector2 Add(Vector2 a, Vector2 b) { return V2(a.x + b.x, a.y + b.y); }
static Vector2 Sub(Vector2 a, Vector2 b) { return V2(a.x - b.x, a.y - b.y); }
static Vector2 Mul(Vector2 a, float s) { return V2(a.x * s, a.y * s); }

static float Len(Vector2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }

static Vector2 Norm(Vector2 v) {
    float l = Len(v);
    if (l < 0.0001f) return V2(0, 0);
    return V2(v.x / l, v.y / l);
}

static float Dist(Vector2 a, Vector2 b) { return Len(Sub(a, b)); }

static Vector2 ClampToScreen(Vector2 p, float margin = 16.0f) {
    float w = (float)GetScreenWidth();
    float h = (float)GetScreenHeight();
    p.x = std::clamp(p.x, margin, w - margin);
    p.y = std::clamp(p.y, margin, h - margin);
    return p;
}

enum class GameState {
    Menu,
    Playing,
    Upgrade,
    GameOver
};

enum class EnemyType {
    Chaser,
    Shooter,
    Splitter,
    Bruiser,
    Turret,
    Boss
};

enum class PickupType {
    Scrap,
    Energy,
    Heart
};

enum class UpgradeType {
    MaxHp,
    Damage,
    FireRate,
    Dash,
    Energy,
    Shield,
    Magnet,
    Shotgun,
    Explosive,
    Chain,
    TimeSlow,
    CoreRepair
};

struct Player {
    Vector2 pos{};
    Vector2 vel{};

    float hp = 100.0f;
    float maxHp = 100.0f;

    float energy = 100.0f;
    float maxEnergy = 100.0f;

    float fireCd = 0.0f;
    float fireRate = 0.18f;
    float bulletDamage = 18.0f;

    float dashCd = 0.0f;
    float dashCooldown = 1.10f;
    float dashTime = 0.0f;
    float dashSpeed = 980.0f;
    float invuln = 0.0f;

    float slowCd = 0.0f;
    float slowTimer = 0.0f;

    float shieldDrain = 24.0f;
    float shieldEff = 1.0f;

    float magnet = 120.0f;
    int scrap = 0;
    int score = 0;

    int shotgunLevel = 0;
    bool explosiveShots = false;
    bool chainShots = false;
};

struct Enemy {
    EnemyType type = EnemyType::Chaser;
    Vector2 pos{};
    Vector2 vel{};
    float hp = 20.0f;
    float maxHp = 20.0f;
    float radius = 16.0f;
    float speed = 60.0f;
    float fireCd = 0.0f;
    int value = 10;
};

struct Bullet {
    Vector2 pos{};
    Vector2 vel{};
    float life = 2.5f;
    float radius = 4.0f;
    float damage = 10.0f;
    bool fromPlayer = true;
    bool explosive = false;
    bool chain = false;
};

struct Pickup {
    PickupType type = PickupType::Scrap;
    Vector2 pos{};
    Vector2 vel{};
    float amount = 1.0f;
};

struct Hazard {
    Vector2 pos{};
    float radius = 20.0f;
    float maxRadius = 120.0f;
    float timer = 0.0f;
    float life = 10.0f;
};

struct TouchInput {
    Vector2 move = { 0, 0 };
    bool startPressed = false;
    bool dashPressed = false;
    bool shieldHeld = false;
    bool slowPressed = false;
    int upgradePressed = -1;
};

static GameState g_state = GameState::Menu;
static Player g_player;

static std::vector<Enemy> g_enemies;
static std::vector<Bullet> g_bullets;
static std::vector<Pickup> g_pickups;
static std::vector<Hazard> g_hazards;

static int g_wave = 0;
static float g_waveClearTimer = 0.0f;
static float g_coreHp = 150.0f;
static float g_coreMaxHp = 150.0f;
static float g_screenShake = 0.0f;

static std::array<UpgradeType, 3> g_choices{};

static bool g_prevStart = false;
static bool g_prevDash = false;
static bool g_prevSlow = false;
static std::array<bool, 3> g_prevUpgrade{ false, false, false };

static void ResetTouchLatches() {
    g_prevStart = false;
    g_prevDash = false;
    g_prevSlow = false;
    g_prevUpgrade = { false, false, false };
}

static const char* UpgradeName(UpgradeType t) {
    switch (t) {
    case UpgradeType::MaxHp: return "MAX HP";
    case UpgradeType::Damage: return "DAMAGE";
    case UpgradeType::FireRate: return "FIRE RATE";
    case UpgradeType::Dash: return "DASH";
    case UpgradeType::Energy: return "ENERGY";
    case UpgradeType::Shield: return "SHIELD";
    case UpgradeType::Magnet: return "MAGNET";
    case UpgradeType::Shotgun: return "SHOTGUN";
    case UpgradeType::Explosive: return "EXPLOSIVE";
    case UpgradeType::Chain: return "CHAIN";
    case UpgradeType::TimeSlow: return "SLOW TIME";
    case UpgradeType::CoreRepair: return "CORE REPAIR";
    }
    return "";
}

static const char* UpgradeDesc(UpgradeType t) {
    switch (t) {
    case UpgradeType::MaxHp: return "+25 HP and full heal";
    case UpgradeType::Damage: return "+6 bullet damage";
    case UpgradeType::FireRate: return "Shoot faster";
    case UpgradeType::Dash: return "Shorter dash cooldown";
    case UpgradeType::Energy: return "+30 max energy";
    case UpgradeType::Shield: return "Cheaper shield drain";
    case UpgradeType::Magnet: return "More pickup magnet";
    case UpgradeType::Shotgun: return "Spread shots";
    case UpgradeType::Explosive: return "Bullets explode";
    case UpgradeType::Chain: return "Chain damage";
    case UpgradeType::TimeSlow: return "Longer slow ability";
    case UpgradeType::CoreRepair: return "Repair core +35 HP";
    }
    return "";
}

static void SpawnPickup(PickupType type, Vector2 pos, float amount) {
    Pickup p;
    p.type = type;
    p.pos = pos;
    p.amount = amount;
    p.vel = V2(Randf(-20.0f, 20.0f), Randf(-20.0f, 20.0f));
    g_pickups.push_back(p);
}

static void SpawnHazard() {
    float w = (float)GetScreenWidth();
    float h = (float)GetScreenHeight();

    Hazard hz;
    hz.pos = V2(Randf(120.0f, w - 120.0f), Randf(120.0f, h - 120.0f));
    hz.maxRadius = Randf(90.0f, 170.0f);
    hz.radius = 18.0f;
    hz.life = Randf(8.0f, 14.0f);
    g_hazards.push_back(hz);
}

static void SpawnEnemy(EnemyType type, Vector2 pos) {
    Enemy e;
    e.type = type;
    e.pos = pos;

    switch (type) {
    case EnemyType::Chaser:
        e.hp = 25.0f; e.maxHp = e.hp; e.radius = 16.0f; e.speed = 78.0f; e.value = 12;
        break;
    case EnemyType::Shooter:
        e.hp = 34.0f; e.maxHp = e.hp; e.radius = 18.0f; e.speed = 52.0f; e.value = 18; e.fireCd = Randf(0.2f, 1.0f);
        break;
    case EnemyType::Splitter:
        e.hp = 42.0f; e.maxHp = e.hp; e.radius = 20.0f; e.speed = 68.0f; e.value = 24;
        break;
    case EnemyType::Bruiser:
        e.hp = 90.0f; e.maxHp = e.hp; e.radius = 26.0f; e.speed = 42.0f; e.value = 35;
        break;
    case EnemyType::Turret:
        e.hp = 48.0f; e.maxHp = e.hp; e.radius = 19.0f; e.speed = 0.0f; e.value = 20; e.fireCd = Randf(0.5f, 1.3f);
        break;
    case EnemyType::Boss:
        e.hp = 420.0f; e.maxHp = e.hp; e.radius = 44.0f; e.speed = 34.0f; e.value = 200; e.fireCd = 0.4f;
        break;
    }

    g_enemies.push_back(e);
}

static void SpawnWave(int wave) {
    g_enemies.clear();
    g_bullets.clear();
    g_pickups.clear();
    g_hazards.clear();

    float w = (float)GetScreenWidth();
    float h = (float)GetScreenHeight();

    int count = 5 + wave * 2;
    for (int i = 0; i < count; ++i) {
        int roll = Randi(0, 100);
        EnemyType t = EnemyType::Chaser;

        if (wave < 3) {
            t = (roll < 78) ? EnemyType::Chaser : EnemyType::Shooter;
        }
        else if (wave < 6) {
            if (roll < 48) t = EnemyType::Chaser;
            else if (roll < 74) t = EnemyType::Shooter;
            else t = EnemyType::Splitter;
        }
        else if (wave < 10) {
            if (roll < 33) t = EnemyType::Chaser;
            else if (roll < 58) t = EnemyType::Shooter;
            else if (roll < 80) t = EnemyType::Splitter;
            else t = EnemyType::Bruiser;
        }
        else {
            if (roll < 22) t = EnemyType::Chaser;
            else if (roll < 48) t = EnemyType::Shooter;
            else if (roll < 72) t = EnemyType::Splitter;
            else if (roll < 90) t = EnemyType::Bruiser;
            else t = EnemyType::Turret;
        }

        Vector2 p;
        int edge = Randi(0, 3);
        if (edge == 0) p = V2(-20.0f, Randf(40.0f, h - 40.0f));
        else if (edge == 1) p = V2(w + 20.0f, Randf(40.0f, h - 40.0f));
        else if (edge == 2) p = V2(Randf(40.0f, w - 40.0f), -20.0f);
        else p = V2(Randf(40.0f, w - 40.0f), h + 20.0f);

        SpawnEnemy(t, p);
    }

    if (wave % 3 == 0) {
        SpawnHazard();
        SpawnHazard();
    }

    if (wave % 5 == 0) {
        SpawnEnemy(EnemyType::Boss, V2(w * 0.5f, 80.0f));
    }
}

static void ResetRun() {
    g_player = Player{};
    g_player.pos = V2((float)GetScreenWidth() * 0.5f, (float)GetScreenHeight() * 0.72f);
    g_player.maxHp = 100.0f;
    g_player.hp = 100.0f;
    g_player.maxEnergy = 100.0f;
    g_player.energy = 100.0f;
    g_player.fireRate = 0.18f;
    g_player.bulletDamage = 18.0f;
    g_player.dashCooldown = 1.10f;
    g_player.magnet = 120.0f;
    g_player.shotgunLevel = 0;
    g_player.explosiveShots = false;
    g_player.chainShots = false;

    g_wave = 1;
    g_waveClearTimer = 0.0f;
    g_coreMaxHp = 150.0f;
    g_coreHp = g_coreMaxHp;
    g_screenShake = 0.0f;

    ResetTouchLatches();
    g_state = GameState::Playing;
    SpawnWave(g_wave);
}

static void StartUpgradeScreen() {
    g_upgradeLockTimer = 0.4f;
    std::vector<UpgradeType> pool = {
        UpgradeType::MaxHp, UpgradeType::Damage, UpgradeType::FireRate, UpgradeType::Dash,
        UpgradeType::Energy, UpgradeType::Shield, UpgradeType::Magnet, UpgradeType::Shotgun,
        UpgradeType::Explosive, UpgradeType::Chain, UpgradeType::TimeSlow, UpgradeType::CoreRepair
    };

    std::shuffle(pool.begin(), pool.end(), g_rng);
    g_choices[0] = pool[0];
    g_choices[1] = pool[1];
    g_choices[2] = pool[2];
    g_state = GameState::Upgrade;
    g_waveClearTimer = 0.0f;
    ResetTouchLatches();
}

static void ApplyUpgrade(UpgradeType t) {
    switch (t) {
    case UpgradeType::MaxHp:
        g_player.maxHp += 25.0f;
        g_player.hp = g_player.maxHp;
        break;
    case UpgradeType::Damage:
        g_player.bulletDamage += 6.0f;
        break;
    case UpgradeType::FireRate:
        g_player.fireRate = std::max(0.06f, g_player.fireRate * 0.82f);
        break;
    case UpgradeType::Dash:
        g_player.dashCooldown = std::max(0.35f, g_player.dashCooldown * 0.82f);
        g_player.dashSpeed += 80.0f;
        break;
    case UpgradeType::Energy:
        g_player.maxEnergy += 30.0f;
        g_player.energy = g_player.maxEnergy;
        break;
    case UpgradeType::Shield:
        g_player.shieldEff *= 0.78f;
        break;
    case UpgradeType::Magnet:
        g_player.magnet += 85.0f;
        break;
    case UpgradeType::Shotgun:
        g_player.shotgunLevel = std::min(3, g_player.shotgunLevel + 1);
        break;
    case UpgradeType::Explosive:
        g_player.explosiveShots = true;
        break;
    case UpgradeType::Chain:
        g_player.chainShots = true;
        break;
    case UpgradeType::TimeSlow:
        g_player.slowCd = std::max(0.0f, g_player.slowCd - 1.5f);
        break;
    case UpgradeType::CoreRepair:
        g_coreHp = std::min(g_coreMaxHp, g_coreHp + 35.0f);
        break;
    }
}

static void DamagePlayer(float amount) {
    if (g_player.invuln > 0.0f) return;
    if (g_player.hp <= 0.0f) return;

    g_player.hp -= amount;
    g_player.invuln = 0.18f;
    g_screenShake = 0.18f;

    if (g_player.hp <= 0.0f) {
        g_player.hp = 0.0f;
        g_state = GameState::GameOver;
        ResetTouchLatches();
    }
}

static int FindNearestEnemyIndex() {
    if (g_enemies.empty()) return -1;

    int best = -1;
    float bestD = 1e9f;
    for (int i = 0; i < (int)g_enemies.size(); ++i) {
        float d = Dist(g_enemies[i].pos, g_player.pos);
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    return best;
}

static void FireAtNearestEnemy() {
    int best = FindNearestEnemyIndex();
    if (best < 0) return;

    Vector2 target = g_enemies[best].pos;
    Vector2 dir = Norm(Sub(target, g_player.pos));
    if (Len(dir) < 0.001f) dir = V2(0, -1);

    int pellets = 1 + g_player.shotgunLevel;
    float spread = 0.16f + 0.05f * (float)g_player.shotgunLevel;

    for (int i = 0; i < pellets; ++i) {
        float t = (pellets == 1) ? 0.0f : ((float)i / (float)(pellets - 1) - 0.5f);
        float ang = std::atan2(dir.y, dir.x) + t * spread;
        Vector2 d = V2(std::cos(ang), std::sin(ang));

        Bullet b;
        b.pos = g_player.pos;
        b.vel = Mul(d, 680.0f);
        b.life = 1.5f;
        b.radius = 4.0f;
        b.damage = g_player.bulletDamage;
        b.fromPlayer = true;
        b.explosive = g_player.explosiveShots;
        b.chain = g_player.chainShots;
        g_bullets.push_back(b);
    }
}

static void SpawnShockwave(Vector2 center) {
    for (auto& e : g_enemies) {
        float d = Dist(e.pos, center);
        if (d < 130.0f) {
            e.hp -= 32.0f;
            Vector2 away = Norm(Sub(e.pos, center));
            e.vel = Add(e.vel, Mul(away, 220.0f));
        }
    }

    for (auto& p : g_pickups) {
        float d = Dist(p.pos, center);
        if (d < 130.0f) {
            Vector2 away = Norm(Sub(p.pos, center));
            p.vel = Add(p.vel, Mul(away, 260.0f));
        }
    }

    g_screenShake = 0.24f;
}

static float g_upgradeLockTimer = 0.0f;
static TouchInput ReadTouchInput() {
    TouchInput in{};
    in.upgradePressed = -1;

    float w = (float)GetScreenWidth();
    float h = (float)GetScreenHeight();

    if (g_state == GameState::Upgrade && g_upgradeLockTimer > 0.0f) {
        g_upgradeLockTimer -= GetFrameTime();
    }

    Rectangle startRect = { w * 0.5f - 110.0f, h * 0.68f, 220.0f, 72.0f };
    Rectangle dashRect = { w - 104.0f, h - 92.0f, 74.0f, 74.0f };
    Rectangle shieldRect = { w - 192.0f, h - 84.0f, 78.0f, 50.0f };
    Rectangle slowRect = { w - 192.0f, h - 140.0f, 78.0f, 50.0f };

    Rectangle upgradeRects[3] = {
        { w * 0.5f - 330.0f, h * 0.38f, 210.0f, 150.0f },
        { w * 0.5f - 105.0f, h * 0.38f, 210.0f, 150.0f },
        { w * 0.5f + 120.0f, h * 0.38f, 210.0f, 150.0f }
    };

    Vector2 joyBase = { 86.0f, h - 108.0f };
    float joyRadius = 54.0f;
    Rectangle joyZone = { 0.0f, h * 0.40f, w * 0.48f, h * 0.60f };

    bool currentStart = false;
    bool currentDash = false;
    bool currentShield = false;
    bool currentSlow = false;

    Vector2 joyTouch = { 0, 0 };
    bool joyFound = false;
    float bestJoyScore = 1e9f;

    int count = GetTouchPointCount();
    int action = GetTouchAction();

    for (int i = 0; i < count; ++i) {
        Vector2 p = GetTouchPosition(i);

        if (g_state == GameState::Menu || g_state == GameState::GameOver) {
            if (CheckCollisionPointRec(p, startRect)) currentStart = true;
        }

        if (g_state == GameState::Playing) {
            if (CheckCollisionPointRec(p, dashRect)) currentDash = true;
            if (CheckCollisionPointRec(p, shieldRect)) currentShield = true;
            if (CheckCollisionPointRec(p, slowRect)) currentSlow = true;
        }

        if (g_state == GameState::Playing && CheckCollisionPointRec(p, joyZone)) {
            float score = Dist(p, joyBase);
            if (score < bestJoyScore) {
                bestJoyScore = score;
                joyTouch = p;
                joyFound = true;
            }
        }

        if (g_state == GameState::Upgrade && g_upgradeLockTimer <= 0.0f) {
            if (action == TOUCH_ACTION_DOWN) {
                for (int u = 0; u < 3; ++u) {
                    if (CheckCollisionPointRec(p, upgradeRects[u])) {
                        in.upgradePressed = u;
                        break;
                    }
                }
            }
        }
    }

    if (joyFound) {
        Vector2 delta = Sub(joyTouch, joyBase);
        float len = Len(delta);
        if (len > joyRadius) delta = Mul(Norm(delta), joyRadius);
        in.move = Mul(delta, 1.0f / joyRadius);
    } else {
        in.move = V2(0, 0); 
    }

    in.startPressed = currentStart && !g_prevStart;
    in.dashPressed = currentDash && !g_prevDash;
    in.shieldHeld = currentShield;
    in.slowPressed = currentSlow && !g_prevSlow;

    g_prevUpgrade = { false, false, false };

    g_prevStart = currentStart;
    g_prevDash = currentDash;
    g_prevSlow = currentSlow;

    return in;
}

static void FireEnemyProjectiles(Enemy& e, Vector2 target, float speed, float damage) {
    Bullet b;
    b.pos = e.pos;
    b.vel = Mul(Norm(Sub(target, e.pos)), speed);
    b.life = 3.0f;
    b.radius = 4.5f;
    b.damage = damage;
    b.fromPlayer = false;
    g_bullets.push_back(b);
}

static void KillEnemy(size_t index) {
    Enemy e = g_enemies[index];

    g_player.score += e.value;
    g_player.scrap += std::max(1, e.value / 12);

    if (e.type == EnemyType::Splitter) {
        SpawnPickup(PickupType::Scrap, e.pos, 2.0f);
        SpawnEnemy(EnemyType::Chaser, Add(e.pos, V2(18, 0)));
        SpawnEnemy(EnemyType::Chaser, Add(e.pos, V2(-18, 0)));
    }
    else if (e.type == EnemyType::Boss) {
        for (int i = 0; i < 8; ++i) {
            SpawnPickup(PickupType::Scrap, Add(e.pos, V2(Randf(-18, 18), Randf(-18, 18))), 4.0f);
        }
        SpawnPickup(PickupType::Heart, e.pos, 1.0f);
    }
    else {
        if (Randi(0, 100) < 28) SpawnPickup(PickupType::Scrap, e.pos, (float)Randi(1, 4));
        if (Randi(0, 100) < 14) SpawnPickup(PickupType::Energy, e.pos, (float)Randi(10, 20));
        if (Randi(0, 100) < 6) SpawnPickup(PickupType::Heart, e.pos, 1.0f);
    }

    g_enemies.erase(g_enemies.begin() + (long)index);
}

static void UpdatePlayer(float dt, const TouchInput& input) {
    if (g_player.dashCd > 0.0f) g_player.dashCd -= dt;
    if (g_player.slowCd > 0.0f) g_player.slowCd -= dt;
    if (g_player.fireCd > 0.0f) g_player.fireCd -= dt;
    if (g_player.invuln > 0.0f) g_player.invuln -= dt;
    if (g_player.dashTime > 0.0f) g_player.dashTime -= dt;
    if (g_player.slowTimer > 0.0f) g_player.slowTimer -= dt;
    if (g_player.slowTimer < 0.0f) g_player.slowTimer = 0.0f;

    g_player.energy = std::min(g_player.maxEnergy, g_player.energy + 10.0f * dt);

    Vector2 move = input.move;
    if (Len(move) <= 0.001f) {
        if (IsKeyDown(KEY_A)) move.x -= 1.0f;
        if (IsKeyDown(KEY_D)) move.x += 1.0f;
        if (IsKeyDown(KEY_W)) move.y -= 1.0f;
        if (IsKeyDown(KEY_S)) move.y += 1.0f;
        move = Norm(move);
    }

    if (input.dashPressed && g_player.dashCd <= 0.0f) {
        Vector2 dashDir = move;
        if (Len(dashDir) <= 0.001f) dashDir = V2(0, -1);
        g_player.vel = Add(g_player.vel, Mul(dashDir, g_player.dashSpeed));
        g_player.dashCd = g_player.dashCooldown;
        g_player.dashTime = 0.18f;
        g_player.invuln = std::max(g_player.invuln, 0.18f);
        g_screenShake = 0.10f;
    }

    if (input.slowPressed && g_player.slowCd <= 0.0f && g_player.energy >= 30.0f) {
        g_player.energy -= 30.0f;
        g_player.slowCd = 8.0f;
        g_player.slowTimer = 3.5f;
        g_screenShake = 0.08f;
    }

    if (g_player.dashTime <= 0.0f) {
        g_player.vel = Mul(move, 220.0f);
    }

    g_player.pos = Add(g_player.pos, Mul(g_player.vel, dt));
    g_player.pos = ClampToScreen(g_player.pos, 20.0f);

    if (g_player.fireCd <= 0.0f) {
        FireAtNearestEnemy();
        g_player.fireCd = g_player.fireRate;
    }

    if (input.shieldHeld && g_player.energy > 0.0f) {
        g_player.energy -= g_player.shieldDrain * g_player.shieldEff * dt;
        if (g_player.energy < 0.0f) g_player.energy = 0.0f;
    }

    Vector2 core = V2((float)GetScreenWidth() * 0.5f, (float)GetScreenHeight() * 0.5f);
    if (Dist(g_player.pos, core) < 95.0f && g_player.scrap >= 5 && g_coreHp < g_coreMaxHp) {
        g_player.scrap -= 5;
        g_coreHp = std::min(g_coreMaxHp, g_coreHp + 18.0f);
    }
}

static void UpdateEnemies(float dt) {
    float slowMul = (g_player.slowTimer > 0.0f) ? 0.45f : 1.0f;
    float w = (float)GetScreenWidth();
    float h = (float)GetScreenHeight();
    Vector2 core = V2(w * 0.5f, h * 0.5f);

    for (size_t i = 0; i < g_enemies.size();) {
        Enemy& e = g_enemies[i];

        if (e.fireCd > 0.0f) e.fireCd -= dt;

        Vector2 toPlayer = Sub(g_player.pos, e.pos);
        Vector2 toCore = Sub(core, e.pos);
        Vector2 dir = Norm((Len(toPlayer) < 280.0f || e.type == EnemyType::Boss) ? toPlayer : toCore);

        switch (e.type) {
        case EnemyType::Chaser:
            e.vel = Add(Mul(dir, e.speed), Mul(e.vel, 0.08f));
            break;

        case EnemyType::Shooter:
            if (Len(toPlayer) < 220.0f) {
                e.vel = Add(Mul(Norm(Mul(toPlayer, -1.0f)), 82.0f), Mul(e.vel, 0.04f));
            }
            else {
                e.vel = Add(Mul(dir, e.speed), Mul(e.vel, 0.05f));
            }
            if (e.fireCd <= 0.0f && Dist(e.pos, g_player.pos) < 420.0f) {
                FireEnemyProjectiles(e, g_player.pos, 250.0f, 8.0f);
                e.fireCd = Randf(1.0f, 1.45f);
            }
            break;

        case EnemyType::Splitter:
            e.vel = Add(Mul(dir, e.speed), Mul(e.vel, 0.08f));
            break;

        case EnemyType::Bruiser:
            e.vel = Add(Mul(dir, e.speed), Mul(e.vel, 0.05f));
            break;

        case EnemyType::Turret:
            e.vel = Mul(e.vel, 0.0f);
            if (e.fireCd <= 0.0f && Dist(e.pos, g_player.pos) < 520.0f) {
                FireEnemyProjectiles(e, g_player.pos, 290.0f, 10.0f);
                e.fireCd = Randf(0.9f, 1.25f);
            }
            break;

        case EnemyType::Boss:
            e.vel = Add(Mul(dir, e.speed), Mul(e.vel, 0.03f));
            if (e.fireCd <= 0.0f) {
                float angBase = std::atan2(toPlayer.y, toPlayer.x);
                for (int k = -2; k <= 2; ++k) {
                    float ang = angBase + (float)k * 0.17f + Randf(-0.03f, 0.03f);
                    Vector2 d = V2(std::cos(ang), std::sin(ang));
                    Bullet b;
                    b.pos = e.pos;
                    b.vel = Mul(d, 240.0f);
                    b.life = 3.0f;
                    b.radius = 5.0f;
                    b.damage = 12.0f;
                    b.fromPlayer = false;
                    g_bullets.push_back(b);
                }
                e.fireCd = Randf(0.55f, 0.9f);
            }
            if (Randi(0, 100) < 2) SpawnHazard();
            break;
        }

        e.pos = Add(e.pos, Mul(e.vel, dt * slowMul));
        e.pos = ClampToScreen(e.pos, 12.0f);

        if (Dist(e.pos, core) < e.radius + 30.0f) {
            g_coreHp -= (e.type == EnemyType::Boss ? 18.0f : 7.0f) * dt;
            g_screenShake = 0.06f;
        }

        if (Dist(e.pos, g_player.pos) < e.radius + 14.0f) {
            if (g_player.energy > 0.0f && IsKeyDown(KEY_SPACE) == false) {
                if (g_player.shieldDrain > 0.0f) {
                    if (g_player.slowTimer >= 0.0f) {
                        if (g_player.energy > 0.0f) {
                            e.hp -= 15.0f * dt;
                            g_player.energy -= g_player.shieldDrain * g_player.shieldEff * 0.5f * dt;
                        }
                    }
                }
            }
            else {
                DamagePlayer((e.type == EnemyType::Boss ? 28.0f : 12.0f) * dt);
            }
        }

        if (e.hp <= 0.0f) {
            if (e.type == EnemyType::Splitter) {
                SpawnEnemy(EnemyType::Chaser, Add(e.pos, V2(18, 0)));
                SpawnEnemy(EnemyType::Chaser, Add(e.pos, V2(-18, 0)));
            }
            KillEnemy(i);
            continue;
        }

        ++i;
    }
}

static void UpdateBullets(float dt) {
    float slowMul = (g_player.slowTimer > 0.0f) ? 0.55f : 1.0f;

    for (size_t i = 0; i < g_bullets.size();) {
        Bullet& b = g_bullets[i];
        b.life -= dt;
        if (b.life <= 0.0f) {
            g_bullets.erase(g_bullets.begin() + (long)i);
            continue;
        }

        b.pos = Add(b.pos, Mul(b.vel, dt * slowMul));

        float w = (float)GetScreenWidth();
        float h = (float)GetScreenHeight();
        if (b.pos.x < -40 || b.pos.x > w + 40 || b.pos.y < -40 || b.pos.y > h + 40) {
            g_bullets.erase(g_bullets.begin() + (long)i);
            continue;
        }

        bool removed = false;

        if (b.fromPlayer) {
            for (size_t e = 0; e < g_enemies.size(); ++e) {
                if (Dist(b.pos, g_enemies[e].pos) < b.radius + g_enemies[e].radius) {
                    g_enemies[e].hp -= b.damage;
                    g_screenShake = 0.04f;

                    if (b.explosive) {
                        for (auto& ee : g_enemies) {
                            if (Dist(ee.pos, b.pos) < 78.0f) {
                                ee.hp -= b.damage * 0.65f;
                            }
                        }
                    }

                    if (b.chain) {
                        int bestIndex = -1;
                        float bestD = 999999.0f;
                        for (size_t k = 0; k < g_enemies.size(); ++k) {
                            if (k == e) continue;
                            float d = Dist(g_enemies[k].pos, g_enemies[e].pos);
                            if (d < 170.0f && d < bestD) {
                                bestD = d;
                                bestIndex = (int)k;
                            }
                        }
                        if (bestIndex >= 0) {
                            g_enemies[bestIndex].hp -= b.damage * 0.55f;
                        }
                    }

                    removed = true;
                    break;
                }
            }
        }
        else {
            bool shieldHeld = false;
            int tc = GetTouchPointCount();
            Rectangle shieldRect = { (float)GetScreenWidth() - 192.0f, (float)GetScreenHeight() - 84.0f, 78.0f, 50.0f };
            for (int t = 0; t < tc; ++t) {
                if (CheckCollisionPointRec(GetTouchPosition(t), shieldRect)) {
                    shieldHeld = true;
                    break;
                }
            }

            if (shieldHeld && g_player.energy > 0.0f) {
                removed = true;
            }
            else if (Dist(b.pos, g_player.pos) < b.radius + 14.0f) {
                DamagePlayer(b.damage);
                removed = true;
            }
        }

        if (removed) {
            g_bullets.erase(g_bullets.begin() + (long)i);
            continue;
        }

        ++i;
    }
}

static void UpdatePickups(float dt) {
    for (size_t i = 0; i < g_pickups.size();) {
        Pickup& p = g_pickups[i];
        Vector2 toPlayer = Sub(g_player.pos, p.pos);
        float d = Len(toPlayer);

        if (d < g_player.magnet) {
            p.vel = Add(p.vel, Mul(Norm(toPlayer), 900.0f * dt));
        }

        p.pos = Add(p.pos, Mul(p.vel, dt));
        p.vel = Mul(p.vel, 0.98f);

        if (d < 22.0f) {
            if (p.type == PickupType::Scrap) {
                g_player.scrap += (int)p.amount;
                g_player.score += 3 * (int)p.amount;
            }
            else if (p.type == PickupType::Energy) {
                g_player.energy = std::min(g_player.maxEnergy, g_player.energy + p.amount);
                g_player.score += 4;
            }
            else {
                g_player.hp = std::min(g_player.maxHp, g_player.hp + 22.0f);
                g_player.score += 8;
            }
            g_pickups.erase(g_pickups.begin() + (long)i);
            continue;
        }

        ++i;
    }
}

static void UpdateHazards(float dt) {
    for (size_t i = 0; i < g_hazards.size();) {
        Hazard& h = g_hazards[i];
        h.timer += dt;
        h.life -= dt;

        float pulse = 0.5f + 0.5f * std::sin(h.timer * 4.5f);
        h.radius = 25.0f + pulse * h.maxRadius * 0.95f;

        if (Dist(g_player.pos, h.pos) < h.radius + 14.0f) {
            DamagePlayer(18.0f * dt);
            g_player.vel = Add(g_player.vel, Mul(Norm(Sub(g_player.pos, h.pos)), 160.0f * dt));
        }

        for (auto& e : g_enemies) {
            if (Dist(e.pos, h.pos) < h.radius + e.radius) {
                e.hp -= 12.0f * dt;
                e.vel = Add(e.vel, Mul(Norm(Sub(e.pos, h.pos)), 120.0f * dt));
            }
        }

        if (h.life <= 0.0f) {
            g_hazards.erase(g_hazards.begin() + (long)i);
            continue;
        }
        ++i;
    }
}

static void TryEndWave() {
    if (g_state != GameState::Playing) return;

    if (g_enemies.empty()) {
        g_waveClearTimer += GetFrameTime();
        if (g_waveClearTimer > 1.0f) {
            StartUpgradeScreen();
        }
    }
    else {
        g_waveClearTimer = 0.0f;
    }
}

static void DrawBar(int x, int y, int w, int h, float pct, Color fill, Color back, const char* label) {
    DrawRectangle(x, y, w, h, back);
    DrawRectangle(x, y, (int)(w * std::clamp(pct, 0.0f, 1.0f)), h, fill);
    DrawRectangleLines(x, y, w, h, Fade(WHITE, 0.18f));
    DrawText(label, x, y - 18, 12, RAYWHITE);
}

static void DrawButton(Rectangle r, const char* text, Color fill, bool active) {
    DrawRectangleRounded(r, 0.22f, 8, Fade(fill, active ? 0.92f : 0.62f));
    DrawRectangleRoundedLines(r, 0.22f, 8, Fade(WHITE, 0.18f));
    int fontSize = 14;
    int tw = MeasureText(text, fontSize);
    DrawText(text, (int)(r.x + r.width * 0.5f - tw * 0.5f), (int)(r.y + r.height * 0.5f - 7), fontSize, RAYWHITE);
}

static void DrawTouchUI(const TouchInput& input) {
    float w = (float)GetScreenWidth();
    float h = (float)GetScreenHeight();

    Vector2 joyBase = { 86.0f, h - 108.0f };
    float joyRadius = 54.0f;

    DrawCircleLines((int)joyBase.x, (int)joyBase.y, joyRadius, Fade(WHITE, 0.20f));
    DrawCircleLines((int)joyBase.x, (int)joyBase.y, joyRadius * 0.58f, Fade(WHITE, 0.10f));
    Vector2 knob = Add(joyBase, Mul(input.move, joyRadius));
    DrawCircleV(knob, 18.0f, Fade(SKYBLUE, 0.85f));

    Rectangle dashRect = { w - 104.0f, h - 92.0f, 74.0f, 74.0f };
    Rectangle shieldRect = { w - 192.0f, h - 84.0f, 78.0f, 50.0f };
    Rectangle slowRect = { w - 192.0f, h - 140.0f, 78.0f, 50.0f };

    DrawButton(dashRect, "DASH", BLUE, input.dashPressed);
    DrawButton(shieldRect, "SHIELD", SKYBLUE, input.shieldHeld);
    DrawButton(slowRect, "SLOW", PURPLE, input.slowPressed);
}

static TouchInput g_input{};
static void DrawGame() {
    BeginDrawing();
    ClearBackground(Color{ 12, 13, 18, 255 });

    float shakeX = 0.0f;
    float shakeY = 0.0f;
    if (g_screenShake > 0.0f) {
        shakeX = Randf(-g_screenShake, g_screenShake) * 12.0f;
        shakeY = Randf(-g_screenShake, g_screenShake) * 12.0f;
        g_screenShake -= GetFrameTime();
        if (g_screenShake < 0.0f) g_screenShake = 0.0f;
    }

    Camera2D cam{};
    cam.offset = V2(shakeX, shakeY);
    cam.target = V2(0, 0);
    cam.rotation = 0.0f;
    cam.zoom = 1.0f;

    BeginMode2D(cam);

    float w = (float)GetScreenWidth();
    float h = (float)GetScreenHeight();
    Vector2 core = V2(w * 0.5f, h * 0.5f);

    for (int x = 0; x < (int)w; x += 40) DrawLine(x, 0, x, (int)h, Fade(Color{ 60, 70, 90, 255 }, 0.16f));
    for (int y = 0; y < (int)h; y += 40) DrawLine(0, y, (int)w, y, Fade(Color{ 60, 70, 90, 255 }, 0.16f));

    float corePulse = 1.0f + 0.04f * std::sin((float)GetTime() * 3.0f);
    DrawCircleV(core, 28.0f * corePulse, Color{ 45, 160, 255, 255 });
    DrawCircleLines((int)core.x, (int)core.y, 70.0f, Fade(SKYBLUE, 0.35f));
    DrawCircleLines((int)core.x, (int)core.y, 92.0f, Fade(SKYBLUE, 0.18f));
    DrawCircleLines((int)core.x, (int)core.y, 18.0f, WHITE);

    for (const auto& hz : g_hazards) {
        DrawCircleLines((int)hz.pos.x, (int)hz.pos.y, hz.radius, Fade(PURPLE, 0.55f));
        DrawCircleLines((int)hz.pos.x, (int)hz.pos.y, hz.radius * 0.75f, Fade(VIOLET, 0.28f));
        DrawCircleV(hz.pos, 5.0f, PURPLE);
    }

    for (const auto& p : g_pickups) {
        Color c = GOLD;
        float r = 6.0f;
        if (p.type == PickupType::Energy) { c = SKYBLUE; r = 7.0f; }
        if (p.type == PickupType::Heart) { c = PINK; r = 8.0f; }
        DrawCircleV(p.pos, r, c);
        DrawCircleLines((int)p.pos.x, (int)p.pos.y, r + 3.0f, Fade(WHITE, 0.18f));
    }

    for (const auto& b : g_bullets) {
        if (b.fromPlayer) {
            DrawCircleV(b.pos, b.radius, b.explosive ? ORANGE : YELLOW);
            if (b.chain) DrawCircleLines((int)b.pos.x, (int)b.pos.y, 8.0f, Fade(SKYBLUE, 0.7f));
        }
        else {
            DrawCircleV(b.pos, b.radius, RED);
        }
    }

    for (const auto& e : g_enemies) {
        Color c = RED;
        switch (e.type) {
        case EnemyType::Chaser: c = MAROON; break;
        case EnemyType::Shooter: c = Color{ 255, 80, 80, 255 }; break;
        case EnemyType::Splitter: c = ORANGE; break;
        case EnemyType::Bruiser: c = Color{ 180, 40, 40, 255 }; break;
        case EnemyType::Turret: c = PURPLE; break;
        case EnemyType::Boss: c = Color{ 255, 30, 100, 255 }; break;
        }

        DrawCircleV(e.pos, e.radius, c);
        DrawCircleLines((int)e.pos.x, (int)e.pos.y, e.radius + 2.0f, Fade(WHITE, 0.16f));
        DrawCircleLines((int)e.pos.x, (int)e.pos.y, e.radius + 8.0f, Fade(BLACK, 0.24f));

        float hpPct = e.hp / e.maxHp;
        DrawRectangle((int)e.pos.x - (int)e.radius, (int)e.pos.y - (int)e.radius - 12, (int)(e.radius * 2), 4, Fade(BLACK, 0.7f));
        DrawRectangle((int)e.pos.x - (int)e.radius, (int)e.pos.y - (int)e.radius - 12, (int)(e.radius * 2 * hpPct), 4, LIME);
    }

    Color playerCol = (g_player.invuln > 0.0f || g_player.dashTime > 0.0f) ? Color{ 120, 255, 255, 255 } : BLUE;
    DrawCircleV(g_player.pos, 14.0f, playerCol);
    DrawCircleLines((int)g_player.pos.x, (int)g_player.pos.y, 22.0f, Fade(WHITE, 0.18f));

    EndMode2D();

    DrawRectangle(14, 14, 310, 164, Fade(BLACK, 0.58f));
    DrawRectangleLines(14, 14, 310, 164, Fade(WHITE, 0.12f));

    DrawText(TextFormat("WAVE: %d", g_wave), 24, 22, 22, RAYWHITE);
    DrawText(TextFormat("SCORE: %d", g_player.score), 24, 48, 20, GOLD);
    DrawText(TextFormat("SCRAP: %d", g_player.scrap), 24, 70, 20, ORANGE);

    DrawBar(24, 98, 260, 14, g_player.hp / g_player.maxHp, RED, Color{ 60, 25, 25, 255 }, "HP");
    DrawBar(24, 124, 260, 14, g_player.energy / g_player.maxEnergy, SKYBLUE, Color{ 25, 35, 50, 255 }, "ENERGY");
    DrawBar(24, 150, 260, 14, g_coreHp / g_coreMaxHp, GREEN, Color{ 20, 40, 20, 255 }, "CORE");

    DrawTouchUI(g_input);

    DrawText("LEFT: MOVE   DASH: BURST   SHIELD: HOLD   SLOW: ABILITY", 18, (int)h - 22, 14, Fade(WHITE, 0.72f));
    DrawText("AUTO FIRE IS ON", (int)w - 126, 18, 14, Fade(WHITE, 0.72f));

    if (g_state == GameState::Menu) {
        DrawRectangle(0, 0, (int)w, (int)h, Fade(BLACK, 0.74f));
        DrawText("CHROME VIGIL", (int)(w * 0.5f) - MeasureText("CHROME VIGIL", 46) / 2, (int)(h * 0.26f), 46, RAYWHITE);
        DrawText("Survive waves, protect the core, upgrade fast",
            (int)(w * 0.5f) - MeasureText("Survive waves, protect the core, upgrade fast", 18) / 2,
            (int)(h * 0.39f), 18, Fade(RAYWHITE, 0.85f));

        Rectangle startRect = { w * 0.5f - 110.0f, h * 0.67f, 220.0f, 72.0f };
        DrawRectangleRounded(startRect, 0.18f, 10, Fade(BLUE, 0.92f));
        DrawRectangleRoundedLines(startRect, 0.18f, 10, Fade(WHITE, 0.18f));
        DrawText("START", (int)(startRect.x + startRect.width * 0.5f - MeasureText("START", 28) / 2), (int)(startRect.y + 20), 28, RAYWHITE);
        DrawText("Tap / Enter", (int)(w * 0.5f) - MeasureText("Tap / Enter", 16) / 2, (int)(h * 0.80f), 16, Fade(WHITE, 0.68f));
    }

    if (g_state == GameState::Upgrade) {
        DrawRectangle(0, 0, (int)w, (int)h, Fade(BLACK, 0.78f));
        DrawText("CHOOSE UPGRADE", (int)(w * 0.5f) - MeasureText("CHOOSE UPGRADE", 30) / 2, 40, 30, RAYWHITE);

        const int cardW = 210;
        const int cardH = 150;
        const int gap = 15;
        int totalW = cardW * 3 + gap * 2;
        int startX = (int)(w * 0.5f) - totalW / 2;
        int y = (int)(h * 0.38f);

        for (int i = 0; i < 3; ++i) {
            int x = startX + i * (cardW + gap);
            Rectangle r = { (float)x, (float)y, (float)cardW, (float)cardH };
            
            bool hover = CheckCollisionPointRec(GetMousePosition(), r);

            DrawRectangleRounded(r, 0.16f, 10, hover ? Fade(SKYBLUE, 0.24f) : Fade(WHITE, 0.08f));
            DrawRectangleRoundedLines(r, 0.16f, 10, hover ? SKYBLUE : Fade(WHITE, 0.18f));

            DrawText(TextFormat("%d", i + 1), x + 12, y + 8, 20, YELLOW);
            DrawText(UpgradeName(g_choices[i]), x + 16, y + 34, 18, RAYWHITE);
            DrawText(UpgradeDesc(g_choices[i]), x + 16, y + 70, 14, Fade(RAYWHITE, 0.88f));
        }
        
        if (g_upgradeLockTimer > 0.0f) {
            DrawText("Loading...", (int)(w * 0.5f) - MeasureText("Loading...", 18) / 2, (int)h - 70, 18, GRAY);
        } else {
            DrawText("Tap one card", (int)(w * 0.5f) - MeasureText("Tap one card", 18) / 2, (int)h - 70, 18, GOLD);
        }
    }

    if (g_state == GameState::GameOver) {
        DrawRectangle(0, 0, (int)w, (int)h, Fade(BLACK, 0.82f));
        DrawText("GAME OVER", (int)(w * 0.5f) - MeasureText("GAME OVER", 46) / 2, (int)(h * 0.30f), 46, RED);
        DrawText(TextFormat("WAVE: %d", g_wave), (int)(w * 0.5f) - MeasureText(TextFormat("WAVE: %d", g_wave), 26) / 2, (int)(h * 0.43f), 26, RAYWHITE);
        DrawText(TextFormat("SCORE: %d", g_player.score), (int)(w * 0.5f) - MeasureText(TextFormat("SCORE: %d", g_player.score), 24) / 2, (int)(h * 0.50f), 24, GOLD);

        Rectangle startRect = { w * 0.5f - 110.0f, h * 0.67f, 220.0f, 72.0f };
        DrawRectangleRounded(startRect, 0.18f, 10, Fade(RED, 0.90f));
        DrawRectangleRoundedLines(startRect, 0.18f, 10, Fade(WHITE, 0.18f));
        DrawText("RESTART", (int)(startRect.x + startRect.width * 0.5f - MeasureText("RESTART", 28) / 2), (int)(startRect.y + 20), 28, RAYWHITE);
    }

    EndDrawing();
}

static void UpdateGame(float dt) {
    g_input = ReadTouchInput();

    if (g_state == GameState::Menu) {
        if (g_input.startPressed || IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ResetRun();
        }
        DrawGame();
        return;
    }

    if (g_state == GameState::GameOver) {
        if (g_input.startPressed || IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ResetRun();
        }
        DrawGame();
        return;
    }

    if (g_state == GameState::Upgrade) {
        if (g_input.upgradePressed >= 0) {
            ApplyUpgrade(g_choices[g_input.upgradePressed]);
            g_wave++;
            g_state = GameState::Playing;
            SpawnWave(g_wave);
        }
        DrawGame();
        return;
    }

    if (g_coreHp <= 0.0f || g_player.hp <= 0.0f) {
        g_state = GameState::GameOver;
        DrawGame();
        return;
    }

    UpdatePlayer(dt, g_input);
    UpdateEnemies(dt);
    UpdateBullets(dt);
    UpdatePickups(dt);
    UpdateHazards(dt);

    if (g_player.hp <= 0.0f || g_coreHp <= 0.0f) {
        g_state = GameState::GameOver;
        DrawGame();
        return;
    }

    TryEndWave();

    DrawGame();
}

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "Chrono Vigil");
    SetTargetFPS(60);

    g_player.pos = V2(640.0f, 520.0f);
    g_state = GameState::Menu;

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop([]() {
        float dt = GetFrameTime();
        if (dt > 0.033f) dt = 0.033f;
        UpdateGame(dt);
    }, 0, 1);
#else
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.033f) dt = 0.033f;
        UpdateGame(dt);
    }
    CloseWindow();
#endif

    return 0;
}
