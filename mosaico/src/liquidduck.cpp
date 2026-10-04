/**
 * LiquidDuck — ESP32-S3 (M5Cardputer)
 * 旋转互动版：实现鸭子不倒翁动态效果
 */

#include "platform.hpp"
#include "interaction.hpp"
#include <vector>
#include <cmath>
#include "config.h"
#include "character_assets.h"
#include "flip.h"


// ─────────────────────────────────────────────────────────────
//  物理模式与数据结构
// ─────────────────────────────────────────────────────────────
enum PhysicsMode {
    MODE_FLIP,
    MODE_SOLID_PIXEL,
    MODE_GRADIENT_PIXEL,
    MODE_BOTTLE
};
static PhysicsMode currentMode = MODE_FLIP;
static int currentCharacterIndex = 0; // 当前角色索引
static uint32_t currentBgColor = COLOR_BG_BLUE; // 当前背景色

static int currentFluidW = FLUID_WIDTH_BOTTLE;
static int currentFluidH = FLUID_HEIGHT_BOTTLE;


// ─────────────────────────────────────────────────────────────
//  数据结构
// ─────────────────────────────────────────────────────────────
struct Duck {
    float x, y;
    float lastX, lastY;
    float vx, vy;
    float angle;    // 当前角度 (度)
    float aVel;     // 角速度
};

static FlipFluid* fluid = nullptr;
struct Cloud {
    float x, y;
    float vx, vy;
    float size;
};
static Cloud clouds[2];
static Duck  duck;
static float imuGX = 0.0f, imuGY = 0.0f;

static inline uint16_t rgb32to16(uint32_t c) {
    uint8_t r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = (c) & 0xFF;
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

void initClouds() {
    for (int i = 0; i < 2; i++) {
        clouds[i].x = random(SCREEN_W);
        clouds[i].y = random(SCREEN_H);
        clouds[i].vx = 0;
        clouds[i].vy = 0;
        // 分化云朵大小：第一朵较大，第二朵更小
        if (i == 0) clouds[i].size = 18.0f + random(7); // 大云 (18-25)
        else        clouds[i].size = 12.0f + random(4);  // 小云 (12-16)
    }
}

void updateClouds(float dt) {
    // 与重力方向 (imuGX, imuGY) 相反的力
    float ax = -imuGX * 0.15f;
    float ay = -imuGY * 0.15f;

    for (int i = 0; i < 2; i++) {
        clouds[i].vx += ax * dt;
        clouds[i].vy += ay * dt;
        clouds[i].vx *= timeDecay(.606081f,dt); // 阻尼
        clouds[i].vy *= timeDecay(.606081f,dt);

        clouds[i].x += clouds[i].vx * dt;
        clouds[i].y += clouds[i].vy * dt;


    }

    // 云朵间的碰撞处理（1.0f 真实几何边界接触，轻微相撞后产生物理反弹并顺滑飘散荡开）
    float dx = clouds[1].x - clouds[0].x;
    float dy = clouds[1].y - clouds[0].y;
    float d2 = dx*dx + dy*dy;
    float minDist = clouds[0].size + clouds[1].size; // 还原为 1.0f 真实几何边缘接触
    if (d2 < minDist * minDist) {
        float d = micropixel::math::Sqrt(d2); if (d < 0.1f) d = 0.1f;
        float overlap = (minDist - d);
        float nx = dx / d, ny = dy / d;

        // 1. 物理位置轻微修正以防止穿透（仅做 20% 的硬性推开，保留 80% 作为弹性重合水乳交融的美感）
        clouds[0].x -= nx * overlap * (1-timeDecay(3.160815f,dt)); clouds[0].y -= ny * overlap * (1-timeDecay(3.160815f,dt));
        clouds[1].x += nx * overlap * (1-timeDecay(3.160815f,dt)); clouds[1].y += ny * overlap * (1-timeDecay(3.160815f,dt));

        // 2. 完美的法线动能反弹与轻微逃逸力
        float rvx = clouds[1].vx - clouds[0].vx;
        float rvy = clouds[1].vy - clouds[0].vy;
        float velNormal = rvx * nx + rvy * ny;

        // 当两朵云正在相互靠近时，进行法线动量反转反弹
        if (velNormal < 0.0f) {
            float restitution = 0.85f; // 高弹性，相撞时快速弹开
            float impulse = -(1.0f + restitution) * velNormal * 0.5f;
            clouds[0].vx -= nx * impulse; clouds[0].vy -= ny * impulse;
            clouds[1].vx += nx * impulse; clouds[1].vy += ny * impulse;
        }

        // 3. 额外注入“微观飘散排斥速度”，即使相对速度为零，也会在碰触瞬间优雅地朝两侧轻轻飘逸荡开
        float floatAwayPush = (12.0f + overlap * 8.0f)*dt*30; // 飘逸推力
        clouds[0].vx -= nx * floatAwayPush; clouds[0].vy -= ny * floatAwayPush;
        clouds[1].vx += nx * floatAwayPush; clouds[1].vy += ny * floatAwayPush;
    }
    // Include the rotated side circles and re-clamp after cloud separation.
    float sn=micropixel::math::Sin(duck.angle*(M_PI/180.0f));
    float cs=micropixel::math::Cos(duck.angle*(M_PI/180.0f));
    for(auto &c:clouds){
        float left=c.size,right=c.size,top=c.size,bottom=c.size;
        for(int sign=-1;sign<=1;sign+=2){
            float x=c.size*(sign*.7f*cs-.2f*sn),y=c.size*(sign*.7f*sn+.2f*cs);
            left=micropixel::math::Max(left,.8f*c.size-x);right=micropixel::math::Max(right,.8f*c.size+x);
            top=micropixel::math::Max(top,.8f*c.size-y);bottom=micropixel::math::Max(bottom,.8f*c.size+y);
        }
        if(c.x<left){c.x=left;if(c.vx<0)c.vx*=-.5f;}
        if(c.x>SCREEN_W-1-right){c.x=SCREEN_W-1-right;if(c.vx>0)c.vx*=-.5f;}
        if(c.y<top){c.y=top;if(c.vy<0)c.vy*=-.5f;}
        if(c.y>SCREEN_H-1-bottom){c.y=SCREEN_H-1-bottom;if(c.vy>0)c.vy*=-.5f;}
    }

}

void drawClouds(Canvas* cv) {
    float rad = duck.angle * (M_PI / 180.0f);
    float s_r = micropixel::math::Sin(rad);
    float c_r = micropixel::math::Cos(rad);

    for (int i = 0; i < 2; i++) {
        uint16_t color = rgb32to16(0xFFFFFF); // 纯白色，在浅蓝背景下更明亮
        float s = clouds[i].size;
        float cx = clouds[i].x;
        float cy = clouds[i].y;

        // 主圆
        cv->fillCircle(cx, cy, s, color);

        // 旋转两个侧翼圆的偏移量
        // 原始偏移: 左(-s*0.7, s*0.2), 右(s*0.7, s*0.2)
        float offX1 = -s * 0.7f, offY1 = s * 0.2f;
        float offX2 =  s * 0.7f, offY2 = s * 0.2f;

        cv->fillCircle(cx + (offX1 * c_r - offY1 * s_r), cy + (offX1 * s_r + offY1 * c_r), s * 0.8f, color);
        cv->fillCircle(cx + (offX2 * c_r - offY2 * s_r), cy + (offX2 * s_r + offY2 * c_r), s * 0.8f, color);
    }
}

static float flip_h = 0.0f;
static float flip_tank_w = 0.0f;
static float flip_tank_h = 0.0f;
static float flip_scale_x = 1.0f;
static float flip_scale_y = 1.0f;

static inline float sim_to_screen_x(float sim_x) { return (sim_x - flip_h) * flip_scale_x; }
static inline float sim_to_screen_y(float sim_y) { return (sim_y - flip_h) * flip_scale_y; }
static inline float screen_to_sim_x(float sc_x) { return (sc_x / flip_scale_x) + flip_h; }
static inline float screen_to_sim_y(float sc_y) { return (sc_y / flip_scale_y) + flip_h; }



static FlipFluid* bottleFluid = nullptr;
static float* bottleGrid = nullptr;

static const int PIXEL_PALETTE_COUNT = 8;
static const uint32_t PIXEL_PALETTES[PIXEL_PALETTE_COUNT][7] = {
    { 0x000500, 0x002200, 0x004400, 0x008800, 0x00CC00, 0x00FF00, 0x88FF88 }, // 黑客帝国绿 (Matrix Hacker)
    { 0x050005, 0x220033, 0x550066, 0x990099, 0xFF00FF, 0x00FFFF, 0xFFFFFF }, // 霓虹赛博 (Cyberpunk)
    { 0x0C0500, 0x331100, 0x662200, 0x994400, 0xCC6600, 0xFF8800, 0xFFBB66 }, // 经典琥珀 (Amber Terminal)
    { 0x080808, 0x222222, 0x444444, 0x777777, 0xAAAAAA, 0xDDDDDD, 0xFFFFFF }, // 复古黑白 (Classic Mono)
    { 0x0A0000, 0x330000, 0x660000, 0x990000, 0xCC0000, 0xFF0000, 0xFF8888 }, // 猩红警戒 (Blood Red)
    { 0x000511, 0x001133, 0x002266, 0x0044AA, 0x0088FF, 0x00CCFF, 0xAAFFFF }, // 深海蔚蓝 (Deep Sea)
    { 0x08000C, 0x1E002E, 0x3C005C, 0x5A008A, 0x7800B8, 0x39FF14, 0xE5FF00 }, // 生化毒液 (Toxic Venom)
    { 0x0D0B00, 0x332900, 0x5C4A00, 0x856B00, 0xAD8C00, 0xD6AD00, 0xFFF5CC }  // 奢华至臻 (Luxury Gold)
};
static int currentPixelPaletteIndex = 5; // 默认深海蔚蓝 (Deep Sea)

static uint32_t getFluidColor(float density) {
    if (density < 0.1f) return PIXEL_PALETTES[currentPixelPaletteIndex][0];
    float factor = density / 20.0f;
    int idx = (int)(factor * (7 - 1));
    if (idx < 1) idx = 1;
    if (idx >= 7) idx = 7 - 1;
    return PIXEL_PALETTES[currentPixelPaletteIndex][idx];
}

static uint32_t getPixelSolidColor(float density) {
    if (density < 0.1f) return PIXEL_PALETTES[currentPixelPaletteIndex][0];
    return PIXEL_PALETTES[currentPixelPaletteIndex][7 - 2]; // 亮色，p[5]
}

static uint32_t getPixelGradientColor(float density) {
    if (density < 0.5f) return PIXEL_PALETTES[currentPixelPaletteIndex][0];
    float factor = density / 4.0f; // 在 1 到 4 颗粒子汇集时获得完整色阶明暗映射
    int idx = (int)(factor * (7 - 1));
    if (idx < 1) idx = 1;
    if (idx >= 7) idx = 7 - 1;
    return PIXEL_PALETTES[currentPixelPaletteIndex][idx];
}

static void initFluidForMode(PhysicsMode mode) {
    if (bottleFluid) {
        flip_destroy(bottleFluid);
        bottleFluid = nullptr;
    }
    if (bottleGrid) {
        portFree(bottleGrid);
        bottleGrid = nullptr;
    }

    int gridW = 0;
    int gridH = 0;
    float fillRatio = FLUID_FILL_RATIO_BOTTLE; // 0.45f
    float gravity = 0.0f;
    int pushIters = 1;
    int pressIters = 12;
    float flipRatio = 0.9f;

    if (mode == MODE_BOTTLE) {
        gridW = FLUID_WIDTH_BOTTLE; // 27
        gridH = 24; // 48
        gravity = FLUID_GRAVITY_BOTTLE; // 16.0f
        pushIters = 1;
        pressIters = 12;
        flipRatio = FLIP_RATIO_BOTTLE;
    } else if (mode == MODE_SOLID_PIXEL || mode == MODE_GRADIENT_PIXEL) {
        gridW = 14;
        gridH = 12;
        fillRatio = FLUID_FILL_RATIO_PIXEL;
        gravity = 24.0f;
        pushIters = 1;
        pressIters = 16;
        flipRatio = FLIP_RATIO_PIXEL;
    } else {
        return;
    }

    currentFluidW = gridW;
    currentFluidH = gridH;

    float simW = 1.0f;
    float simH = simW * ((float)gridH / (float)gridW);
    bottleFluid = flip_create(simW, simH, gridW, gridH, fillRatio);
    if (bottleFluid) {
        flip_set_gravity_scale(bottleFluid, gravity);
        flip_set_solver_quality(bottleFluid, pushIters, pressIters, flipRatio);
    }
    bottleGrid = (float*)portAlloc(sizeof(float) * gridW * gridH);
    if (bottleGrid) {
        memset(bottleGrid, 0, sizeof(float) * gridW * gridH);
    }
}

[[clang::no_destroy]] static std::vector<uint8_t> particleColors;

// ─── 震动引擎全局状态 ───────────────────────────────────────────
static float    frameEnergy = 0.0f;  // 当前帧累积的碰撞能量
[[maybe_unused]] static float vibrLevel = 0.0f;  // 经低通滤波后的平滑震动强度输出

[[clang::no_destroy]] static Canvas canvas;
[[clang::no_destroy]] static Canvas duckSprite;
[[clang::no_destroy]] static Canvas ballSprites[2][8];

// 更新角色精灵图
void updateCharacterSprite() {
    const CharacterAsset& asset = CHARACTER_REGISTRY[currentCharacterIndex];
    duckSprite.deleteSprite();
    duckSprite.createSprite(asset.width, asset.height);
    duckSprite.setPivot(asset.width / 2, asset.height / 2);
    duckSprite.fillSprite(0x0001); // 透明色索引
    for (int i = 0; i < (asset.width * asset.height); i++) {
        if (asset.pixels[i] != 0x000000) {
            duckSprite.drawPixel(i % asset.width, i / asset.width, rgb32to16(asset.pixels[i]));
        }
    }
}

// ─────────────────────────────────────────────────────────────
//  辅助渲染工具
// ─────────────────────────────────────────────────────────────
static uint16_t getBrightened16(uint32_t c, float f) {
    uint8_t r = (uint8_t)min(255.0f, ((c >> 16) & 0xFF) * f);
    uint8_t g = (uint8_t)min(255.0f, ((c >> 8) & 0xFF) * f);
    uint8_t b = (uint8_t)min(255.0f, (c & 0xFF) * f);
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

static int currentBallThemeIndex = 0; // 当前海洋球小球主题 (默认 0: Ocean Water)

// 刷新小球精灵图的颜色与高光
void updateBallSprites(int themeIndex) {
    if (themeIndex < 0 || themeIndex >= BALL_THEME_COUNT) themeIndex = 0;
    const BallTheme& theme = BALL_THEMES[themeIndex];
    float pRadius = fluid ? flip_get_particle_radius(fluid) : 3.5f;
    int dia = (int)(pRadius * 2.4f);
    if (dia < 2) dia = 2;

    for (int i = 0; i < 8; i++) {
        uint32_t c = theme.palette[i];
        for (int b = 0; b < 2; b++) {
            if (ballSprites[b][i].width() == 0) {
                ballSprites[b][i].createSprite(dia, dia);
            }
            ballSprites[b][i].fillSprite(0x0001);
            if (b == 1 && theme.speedHighlightWhite) {
                // 超速纯白水花模式 (浪花飞溅效果)
                ballSprites[b][i].fillCircle(dia/2, dia/2, dia/2, rgb32to16(0x96CDFA));
                ballSprites[b][i].fillCircle(dia/2, dia/2, dia/2 - 1, rgb32to16(0xD8EDFD));
                ballSprites[b][i].fillCircle(dia/2 - 1, dia/2 - 1, 2, TFT_WHITE);
            } else {

                float f = (b == 0) ? 1.0f : 1.6f;
                ballSprites[b][i].fillCircle(dia/2, dia/2, dia/2, getBrightened16(c, f * 0.7f));
                ballSprites[b][i].fillCircle(dia/2, dia/2, dia/2 - 1, getBrightened16(c, f * 1.15f));
                ballSprites[b][i].fillCircle(dia/2 - 1, dia/2 - 1, (b == 0 ? 1 : 2), TFT_WHITE);
            }
        }
    }
}

// 随机切换主题（小球配色、背景色与角色同时随机变换）
void switchTheme() {
    // 1. 随机小球主题
    int oldTheme = currentBallThemeIndex;
    if (BALL_THEME_COUNT > 1) {
        while (currentBallThemeIndex == oldTheme) {
            currentBallThemeIndex = random(BALL_THEME_COUNT);
        }
    }
    updateBallSprites(currentBallThemeIndex);

    // 2. 随机背景色 (精选明亮清爽的高颜值背景色)
    static const uint32_t bgColors[] = {
        COLOR_BG_BLUE, COLOR_BG_ORANGE, COLOR_BG_GREEN,
        COLOR_BG_CYAN, COLOR_BG_PINK,   COLOR_BG_MINT
    };
    uint32_t oldColor = currentBgColor;
    if (random(2) == 0 && BALL_THEMES[currentBallThemeIndex].defaultBg != 0) {
        currentBgColor = BALL_THEMES[currentBallThemeIndex].defaultBg;
    } else {
        while (currentBgColor == oldColor) {
            currentBgColor = bgColors[random(sizeof(bgColors) / sizeof(bgColors[0]))];
        }
    }

    // 3. 随机角色 (确保变化)
    int oldCharIndex = currentCharacterIndex;
    if (CHARACTER_COUNT > 1) {
        while (currentCharacterIndex == oldCharIndex) {
            currentCharacterIndex = random(CHARACTER_COUNT);
        }
    }
    updateCharacterSprite();
}

static bool soundRequested = false;
static void playKeyTone() { soundRequested = true; }
static void drawBatteryStatus(Canvas*) {}
inline float distSq(float dx, float dy) { return dx * dx + dy * dy; }




static void physicsStepBOTTLE(float dt) {
    if (!bottleFluid) return;
    float ax = tiltX, ay = tiltY, az = tiltZ;
    float gx = tiltX;
    float gy = tiltY;
    if (micropixel::math::Abs(gx) < IMU_DEADZONE) gx = 0;
    if (micropixel::math::Abs(gy) < IMU_DEADZONE) gy = 0;

    float sub_dt = dt * 0.5f;
    flip_step(bottleFluid, sub_dt, gx, gy);
    flip_step(bottleFluid, sub_dt, gx, gy);
    flip_get_led_grid(bottleFluid, bottleGrid, currentFluidW, currentFluidH);

    float accelMag = micropixel::math::Sqrt(ax*ax + ay*ay + az*az);
    float currentShake = micropixel::math::Abs(accelMag - 1.0f);
    if (currentShake > 0.5f) {
        frameEnergy = micropixel::math::Max(frameEnergy, currentShake * 100.0f);
    }
}

static int mapParticleToCell(float position, float minBound, float maxBound, int cellCount) {
    if (cellCount <= 1 || maxBound <= minBound || position <= minBound) return 0;
    if (position >= maxBound) return cellCount - 1;
    int cell = int((position - minBound) * float(cellCount) / (maxBound - minBound));
    return cell < cellCount ? cell : cellCount - 1;
}

static void renderFrameBOTTLE() {
    uint32_t bg32 = (currentMode == MODE_BOTTLE) ? OCEAN_PALETTE[0] : PIXEL_PALETTES[currentPixelPaletteIndex][0];
    canvas.fillSprite(rgb32to16(bg32));
    float cellW = float(SCREEN_W) / currentFluidW;
    float cellH = float(SCREEN_H) / currentFluidH;

    if (currentMode == MODE_SOLID_PIXEL || currentMode == MODE_GRADIENT_PIXEL) {
        // ================= 赛博像素流体模式 (无分摊、孤立粒子点电荷累加引擎) =================
        // 专门开辟一块临时点密度网格，将每个粒子当成完美的“单格点”，彻底打破双线性插值带来的 4 格马赛克效应！
        static float tempGridStorage[FLUID_WIDTH_BOTTLE * FLUID_HEIGHT_BOTTLE];
        float* tempGrid = tempGridStorage;
        if (tempGrid) {
            memset(tempGrid, 0, sizeof(float) * currentFluidW * currentFluidH);

            int num = 0; float *pos = nullptr; float *vel = nullptr;
            flip_get_particles(bottleFluid, &num, &pos, &vel);
            float minX = 0.0f, maxX = 0.0f, minY = 0.0f, maxY = 0.0f;
            flip_get_particle_bounds(bottleFluid, &minX, &maxX, &minY, &maxY);

            if (num > 0 && pos) {
                for (int i = 0; i < num; i++) {
                    int gx = mapParticleToCell(pos[2*i+0], minX, maxX, currentFluidW);
                    int gy = mapParticleToCell(pos[2*i+1], minY, maxY, currentFluidH);

                    tempGrid[gx * currentFluidH + gy] += 1.0f; // 100% 力量加给它当前所在的唯一单格子
                }
            }

            for (int x = 0; x < currentFluidW; x++) {
                for (int y = 0; y < currentFluidH; y++) {
                    float density = tempGrid[x * currentFluidH + y];
                    if (density > 0.0f) {
                        uint32_t color32;
                        if (currentMode == MODE_SOLID_PIXEL) {
                            color32 = getPixelSolidColor(density);
                        } else {
                            color32 = getPixelGradientColor(density);
                        }
                        uint16_t color16 = rgb32to16(color32);

                        float rx = x * cellW + 1.0f;
                        float ry = y * cellH + 1.0f;
                        float rw = cellW - 2.0f;
                        float rh = cellH - 2.0f;
                        canvas.fillRoundRect(micropixel::math::RoundToInt(rx), micropixel::math::RoundToInt(ry), (int)__builtin_ceilf(rw), (int)__builtin_ceilf(rh), 2, color16);
                    }
                }
            }

        }
    } else {
        // ================= 原生柔连海洋瓶模式 (采用双线性插值平滑渐变波浪) =================
        for (int x = 0; x < currentFluidW; x++) {
            for (int y = 0; y < currentFluidH; y++) {
                float density = bottleGrid[x * currentFluidH + y];
                if (density > 0.1f) {
                    uint32_t color32 = getFluidColor(density);
                    uint16_t color16 = rgb32to16(color32);
                    canvas.fillRect(micropixel::math::RoundToInt(x * cellW), micropixel::math::RoundToInt(y * cellH), (int)__builtin_ceilf(cellW), (int)__builtin_ceilf(cellH), color16);
                }
            }
        }
    }
    drawBatteryStatus(&canvas);

}

static void triggerExplosionFLIP(float screenX=-1, float screenY=-1) {
    if (!fluid) return;
    int num; float *pos, *vel;
    flip_get_particles(fluid, &num, &pos, &vel);
    if (num <= 0) return;
    int target = random(num);
    float ex = screenX<0?pos[2*target+0]:screen_to_sim_x(screenX);
    float ey = screenY<0?pos[2*target+1]:screen_to_sim_y(screenY);
    float rSq = EXPLOSION_RADIUS * EXPLOSION_RADIUS;
    for (int i = 0; i < num; i++) {
        float dx = pos[2*i+0] - ex; float dy = pos[2*i+1] - ey;
        float d2 = dx*dx + dy*dy;
        if (d2 < rSq && d2 > 0.01f) {
            float d = micropixel::math::Sqrt(d2); float f = (EXPLOSION_RADIUS - d) / EXPLOSION_RADIUS * EXPLOSION_FORCE;
            vel[2*i+0] += (dx/d)*f; vel[2*i+1] += (dy/d)*f;
        }
    }
    duck.vx += (duck.x - ex) * 2.0f; duck.vy += (duck.y - ey) * 2.0f;
    duck.aVel += (random(60) - 30); // 爆炸时给一个随机角速度
    playKeyTone();
}

static void renderFrameFLIP() {
    canvas.fillSprite(rgb32to16(currentBgColor));
    drawClouds(&canvas); // 恢复海洋球模式下的云朵
    // Keep each particle's colour stable through acceleration and collisions.

    if (fluid) {
        int num; float *pos, *vel;
        flip_get_particles(fluid, &num, &pos, &vel);
        if (num <= 0 || !pos || !vel) return;
        float pRadius = flip_get_particle_radius(fluid);

        for (int i = 0; i < num; i++) {
            uint8_t cIdx = (size_t(i) < particleColors.size()) ? particleColors[i] : 0;
            float px = sim_to_screen_x(pos[2*i+0]);
            float py = sim_to_screen_y(pos[2*i+1]);
            ballSprites[0][cIdx].pushSprite(&canvas, (int)(px - pRadius), (int)(py - pRadius), 0x0001);
        }
    }
    // 渲染鸭子：有 polygon 路径时用旋转，否则直绘（软件旋转大精灵会卡住 Host）
    float dx = sim_to_screen_x(duck.x);
    float dy = sim_to_screen_y(duck.y);
    if(g_duckRotate)duckSprite.pushRotateZoom(&canvas, dx, dy, duck.angle, 1.0f, 1.0f, 0x0001);
    else duckSprite.pushSprite(&canvas, int(dx) - duckSprite.width()/2, int(dy) - duckSprite.height()/2, 0x0001);

    drawBatteryStatus(&canvas);

}

extern float flip_boundary_energy;

static void physicsStepFLIP(float dt) {
    if (!fluid) return;

    flip_step(fluid, dt, imuGX, imuGY);

    if (flip_boundary_energy > VIBR_V_THRESHOLD) {
        frameEnergy = micropixel::math::Max(frameEnergy, flip_boundary_energy * flip_boundary_energy * VIBR_BOUNDARY_W);
    }
    flip_boundary_energy = 0.0f;

    int num; float *pos, *vel;
    flip_get_particles(fluid, &num, &pos, &vel);
    if (num <= 0 || !pos || !vel) return;

    // 晶莹流体专属动力学微调已移除

    duck.vx += imuGX * DUCK_GRAVITY_SCALE * dt;
    duck.vy += imuGY * DUCK_GRAVITY_SCALE * dt;
    duck.lastX = duck.x; duck.lastY = duck.y;
    duck.x += duck.vx * dt; duck.y += duck.vy * dt;
    float pRadius = flip_get_particle_radius(fluid);

    const float charRadius = CHARACTER_REGISTRY[currentCharacterIndex].radius;
    const float duckMinDist = charRadius + pRadius;
    const float duckMinDistSq = duckMinDist * duckMinDist;

    for (int i = 0; i < num; i++) {
        float dx = pos[2*i+0] - duck.x, dy = pos[2*i+1] - duck.y;
        float d2 = distSq(dx, dy);
        if (d2 < duckMinDistSq) {
            float d = micropixel::math::Sqrt(d2); if (d < 0.01f) d = 0.1f;
            float overlap = (duckMinDist - d);
            float nx = dx / d, ny = dy / d;
            pos[2*i+0] += nx * overlap * 0.95f;
            pos[2*i+1] += ny * overlap * 0.95f;

            // ✨ 核心注入：当鸭子排斥海洋球时，附加一个弹射速度，彻底激活“撞击溅射感”！
            vel[2*i+0] += nx * overlap * 25.0f;
            vel[2*i+1] += ny * overlap * 25.0f;

            duck.x -= nx * overlap * 0.05f;
            duck.y -= ny * overlap * 0.05f;
            if (overlap > 0.1f) {
                frameEnergy += overlap * overlap * VIBR_PARTICLE_W;
            }
        }
    }

    // 鸭子的物理边界必须和 FLIP 流体的物理边界保持一致，否则会“卡”在流体底部的 padding 中无法上浮。
    // 在 FLIP 中，流体被限制在距离边缘 h (也就是 pRadius / 0.35) 的范围内。
    float duckMinX = flip_h + charRadius;
    float duckMaxX = flip_tank_w - charRadius;
    float duckMinY = flip_h + charRadius;
    float duckMaxY = flip_tank_h - charRadius;

    if (duck.x < duckMinX) { duck.x = duckMinX; duck.vx *= -0.5f; }
    else if (duck.x > duckMaxX) { duck.x = duckMaxX; duck.vx *= -0.5f; }
    if (duck.y < duckMinY) { duck.y = duckMinY; duck.vy *= -0.5f; }
    else if (duck.y > duckMaxY) { duck.y = duckMaxY; duck.vy *= -0.5f; }



    float invDt = 1.0f / dt;
    float dnvx = (duck.x - duck.lastX) * invDt;
    float dnvy = (duck.y - duck.lastY) * invDt;
    duck.vx = dnvx * timeDecay(1.212162f,dt);
    duck.vy = dnvy * timeDecay(1.212162f,dt);
}

// ─────────────────────────────────────────────────────────────
//  震动引擎
// ─────────────────────────────────────────────────────────────
static void initParticlesFLIP() {
    if (fluid) flip_destroy(fluid);
    float fillRatio = FLUID_FILL_RATIO;
    fluid = flip_create(SCREEN_W, SCREEN_H, FLIP_GRID_W, FLIP_GRID_H, fillRatio);
    if (fluid) {
        flip_set_gravity_scale(fluid, 1.0f);
        flip_set_solver_quality(fluid, FLIP_PUSH_ITERS, FLIP_PRESSURE_ITERS, FLIP_RATIO);

        int num;
        flip_get_particles(fluid, &num, nullptr, nullptr);
        if (num < 0) num = 0;
        if (num > 500) num = 500;
        particleColors.resize(num);
        for(int i=0; i<num; i++) {
            particleColors[i] = (uint8_t)random(8);
        }
    }

    int sim_num_x_d = FLIP_GRID_W + 2;
    int sim_num_y_d = FLIP_GRID_H + 2;
    float hx_d = (float)SCREEN_W / (sim_num_x_d - 1);
    float hy_d = (float)SCREEN_H / (sim_num_y_d - 1);
    flip_h = micropixel::math::Min(hx_d, hy_d);
    flip_tank_w = flip_h * (sim_num_x_d - 1);
    flip_tank_h = flip_h * (sim_num_y_d - 1);
    flip_scale_x = (float)SCREEN_W / (flip_tank_w - flip_h);
    flip_scale_y = (float)SCREEN_H / (flip_tank_h - flip_h);

    float startRadius = CHARACTER_REGISTRY[0].radius;
    duck.x = duck.lastX = screen_to_sim_x(SCREEN_W / 2.0f);
    duck.y = duck.lastY = screen_to_sim_y(startRadius + 5.0f);
    duck.vx = duck.vy = 0.0f; duck.angle = duck.aVel = 0.0f;
}
