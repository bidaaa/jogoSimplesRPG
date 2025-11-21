#include "raylib.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>

//------------------------------------------------------------------------------------
// CONSTANTES
//------------------------------------------------------------------------------------
#define SCREEN_WIDTH 1000
#define SCREEN_HEIGHT 700

//------------------------------------------------------------------------------------
// ESTRUTURAS
//------------------------------------------------------------------------------------

typedef enum {
    GAME_STATE_EXPLORE,
    GAME_STATE_BATTLE,
    GAME_STATE_ENDING_GOOD,
    GAME_STATE_ENDING_BAD,
    GAME_STATE_ENDING_ESCAPE
} GameState;

typedef enum {
    ITEM_NONE,
    ITEM_POTION,
    ITEM_SWORD,
    ITEM_BOMB,
    ITEM_COIN,
    ITEM_ARMOR
} ItemType;

typedef struct {
    int hp;
    int maxHp;
} Player;

typedef struct {
    int hp;
    int maxHp;
    int attack;
} Boss;

typedef enum {
    BATTLE_PLAYER_TURN,
    BATTLE_BOSS_TURN,
} BattleState;

//------------------------------------------------------------------------------------
// VARIÁVEIS GLOBAIS
//------------------------------------------------------------------------------------
static GameState currentState;
static BattleState battleState;

static Player player;
static Boss boss;

#define INVENTORY_SIZE 4
static ItemType inventory[INVENTORY_SIZE];
static bool itemUsed[INVENTORY_SIZE]; // Agora rastreia apenas itens de uso único
static int inventoryCount;
static int selectedItemIndex; // Item selecionado na batalha (0-3)

static int currentStage; // Estágio da exploração (0 a 3)

// Variáveis da batalha
static const char* battleMessage; // Mensagem de status (Ex: "Você usou Poção!")
static char messageBuffer[256]; // Buffer para formatar mensagens
static bool playerHasArmor;
static float bossTurnTimer; // Um pequeno delay para o turno do chefe

// Animações de batalha
static float playerAttackTimer;
static const float PLAYER_ATTACK_DURATION = 0.45f;
static bool playerIsAttacking;

static float playerHurtTimer;
static const float PLAYER_HURT_DURATION = 0.9f;

// Sprites / textures
static Texture2D playerTexture;
static Texture2D bossTexture;
static bool texturesInitialized = false;
// Optional animated boss frames (for GIF support)
static Texture2D *bossFrames = NULL;
static int bossFrameCount = 0;
static int bossFrameIndex = 0;
static float bossFrameTime = 0.0f;
static float bossFrameDuration = 0.12f; // seconds per frame
// Attack spritesheet (optional)
static Texture2D playerAttackTexture;
static int playerAttackFrameCount;
static int playerAttackFrame;
static float playerAttackFrameTime; // time since last frame
static float playerAttackFrameDuration; // seconds per frame
// Custom attack / hit textures
static Texture2D bossAttackTexture;
static Texture2D playerHitTexture;
static Texture2D bossHitTexture;
// Boss attack spritesheet frames
static int bossAttackFrameCount;
static int bossAttackFrame;
static float bossAttackFrameTime;
static float bossAttackFrameDuration;
// Boss attack / hurt state
static bool bossIsAttacking;
static float bossAttackTimer;
// Reduced duration to speed up boss attack spritesheet animation
static const float BOSS_ATTACK_DURATION = 0.40f;
static float bossHurtTimer;
static const float BOSS_HURT_DURATION = 0.9f;

// Variáveis para a mensagem de coleta de item
static float itemMessageTimer = 0.0f;
static ItemType lastItemCollected = ITEM_NONE;

// Exploração: posição do personagem e portas
static float explorePlayerX;
static float explorePlayerY;
static Rectangle doorLeftRect;
static Rectangle doorRightRect;
static float explorePlayerSpeed;

//------------------------------------------------------------------------------------
// FUNÇÕES
//------------------------------------------------------------------------------------

// Forward declaration (placed early so InitGame may call it)
static bool SaveImageAsBMP(const Image img, const char *fileName);

const char* GetItemName(ItemType item) {
    switch (item) {
        case ITEM_POTION: return "Pocao (Cura 50 HP)";
        case ITEM_SWORD:  return "Espada (Dano 30)";
        case ITEM_BOMB:   return "Bomba (Dano 70)";
        case ITEM_COIN:   return "Moeda (Pode \ndistrair o chefe)";
        case ITEM_ARMOR:  return "Armadura (Reduz dano)";
        default:          return "Vazio";
    }
}

ItemType GetItemForChoice(int stage, int choice) {
    if (stage == 0) {
        return (choice == 0) ? ITEM_POTION : ITEM_POTION;
    }
    if (stage == 1) {
        return (choice == 0) ? ITEM_SWORD : ITEM_BOMB;
    }
    if (stage == 2) {
        return (choice == 0) ? ITEM_ARMOR : ITEM_COIN;
    }
    if (stage == 3) {
        return (choice == 0) ? ITEM_POTION : ITEM_POTION;
    }
    return ITEM_NONE;
}

void BossAttack() {
    // start boss attack animation
    bossIsAttacking = true;
    bossAttackTimer = BOSS_ATTACK_DURATION;
    // init boss attack frames
    bossAttackFrame = 0;
    bossAttackFrameTime = 0.0f;
    if (bossAttackFrameCount > 0) bossAttackFrameDuration = BOSS_ATTACK_DURATION / (float)bossAttackFrameCount;
    if (playerHasArmor) {
        int min = (boss.attack - 5) / 2;
        if (min < 1) min = 1;
        int max = (boss.attack + 5) / 2;
        int damage = min + (rand() % (max - min + 1));
        player.hp -= damage;
        if (player.hp < 0) player.hp = 0;
        sprintf(messageBuffer, "Chefe ataca com armadura ativa! Voce levou %d de dano.", damage);
        battleMessage = messageBuffer;
    } else {
        int min = boss.attack - 5;
        if (min < 1) min = 1;
        int max = boss.attack + 5;
        int damage = min + (rand() % (max - min + 1));
        player.hp -= damage;
        if (player.hp < 0) player.hp = 0;
        sprintf(messageBuffer, "Chefe ataca! Voce levou %d de dano!", damage);
        battleMessage = messageBuffer;
    }
    // Trigger player hurt animation (blink)
    playerHurtTimer = PLAYER_HURT_DURATION;
}

void UseItem(int index) {
    if (itemUsed[index]) {
        battleMessage = "Este item ja foi usado!";
        return;
    }

    ItemType item = inventory[index];

    switch (item) {
        case ITEM_POTION:
            player.hp += 50;
            if (player.hp > player.maxHp) player.hp = player.maxHp;
            battleMessage = "Voce usou Pocao! Curou 50 HP!";
            itemUsed[index] = true;
            break;
        case ITEM_SWORD:
            battleMessage = "Espada: aumenta seu dano. Use ATACAR [A].";
            return;
        case ITEM_BOMB:
            {
                int min = 60;
                int max = 90;
                int dmg = min + (rand() % (max - min + 1));
                boss.hp -= dmg;
                if (boss.hp < 0) boss.hp = 0;
                sprintf(messageBuffer, "Voce usou Bomba! Causou %d de dano!", dmg);
                battleMessage = messageBuffer;
                itemUsed[index] = true;
            }
            break;
        case ITEM_COIN:
            itemUsed[index] = true;
            if (rand() % 2 == 0) {
                battleMessage = "Voce usou Moeda! Distraiu o chefe e fugiu!";
                currentState = GAME_STATE_ENDING_ESCAPE;
                return;
            } else {
                battleMessage = "Voce usou Moeda! Mas o chefe nao se distraiu...";
                battleState = BATTLE_BOSS_TURN;
                bossTurnTimer = 1.5f;
                return;
            }
        case ITEM_ARMOR:
            playerHasArmor = true;
            battleMessage = "Voce equipou Armadura! Proximos ataques causarao menos dano.";
            break;
        default:
            battleMessage = "Item invalido?";
            break;
    }
    
    if (boss.hp < 0) boss.hp = 0;

    battleState = BATTLE_BOSS_TURN;
    bossTurnTimer = 1.5f;
}

bool PlayerHasSword(void) {
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        if (inventory[i] == ITEM_SWORD) return true;
    }
    return false;
}

void PlayerAttack(void) {
    int damage;
    if (PlayerHasSword()) {
        int min = 20;
        int max = 40;
        damage = min + (rand() % (max - min + 1));
        boss.hp -= damage;
        if (boss.hp < 0) boss.hp = 0;
        sprintf(messageBuffer, "Voce atacou com a espada! Causou %d de dano!", damage);
    } else {
        int min = 15;
        int max = 22;
        damage = min + (rand() % (max - min + 1));
        boss.hp -= damage;
        if (boss.hp < 0) boss.hp = 0;
        sprintf(messageBuffer, "Voce atacou desarmado! Causou %d de dano!", damage);
    }
    battleMessage = messageBuffer;

    // Start attack animation (player lunges forward)
    playerIsAttacking = true;
    playerAttackTimer = PLAYER_ATTACK_DURATION;
    // init attack frames
    playerAttackFrame = 0;
    playerAttackFrameTime = 0.0f;

    // trigger boss hurt animation
    bossHurtTimer = BOSS_HURT_DURATION;

    // Proceed to boss turn after a short delay
    battleState = BATTLE_BOSS_TURN;
    bossTurnTimer = 1.1f; // slightly shorter so animation can play
}

void InitGame(void) {
    currentState = GAME_STATE_EXPLORE;
    currentStage = 0;
    inventoryCount = 0;
    itemMessageTimer = 0.0f;
    lastItemCollected = ITEM_NONE;

    for (int i = 0; i < INVENTORY_SIZE; i++) {
        inventory[i] = ITEM_NONE;
        itemUsed[i] = false;
    }

    player.hp = 120;
    player.maxHp = 120;

    boss.hp = 200;
    boss.maxHp = 200;
    boss.attack = 22;

    battleState = BATTLE_PLAYER_TURN;
    selectedItemIndex = 0;
    playerHasArmor = false;
    battleMessage = "Batalha contra o Chefe! Escolha seu item.";

    srand((unsigned int)time(NULL));

    // Exploração: inicializa jogador na sala
    explorePlayerX = SCREEN_WIDTH / 2 - 10;
    explorePlayerY = 420; // alinhado com os elementos da UI
    explorePlayerSpeed = 250.0f; // pixels por segundo
    doorLeftRect = (Rectangle){ 100, 240, 150, 220 };
    doorRightRect = (Rectangle){ SCREEN_WIDTH - 250, 240, 150, 220 };

    // Inicializar/Carregar texturas de sprites (assets/player.png, assets/boss.png)
    if (!texturesInitialized) {
        // garante que a pasta assets existe
        struct stat st = {0};
        if (stat("assets", &st) == -1) {
            // tentar criar a pasta
            #if defined(_WIN32)
                mkdir("assets");
            #else
                mkdir("assets", 0755);
            #endif
        }

        if (FileExists("assets/player.png")) {
            playerTexture = LoadTexture("assets/player.png");
        } else if (FileExists("assets/player.bmp")) {
            playerTexture = LoadTexture("assets/player.bmp");
        } else {
            // gerar imagem exemplo e salvar em assets/player.bmp
            Image img = GenImageColor(64, 64, BLUE);
            // desenha um rosto simples na imagem
            for (int y = 10; y < 54; y++) {
                for (int x = 10; x < 54; x++) {
                    if ((x-32)*(x-32)+(y-24)*(y-24) < 10*10) ImageDrawPixel(&img, x, y, (Color){255,200,150,255});
                }
            }
            SaveImageAsBMP(img, "assets/player.bmp");
            playerTexture = LoadTextureFromImage(img);
            UnloadImage(img);
        }

        if (FileExists("assets/boss.png")) {
            bossTexture = LoadTexture("assets/boss.png");
        } else if (FileExists("assets/boss.bmp")) {
            bossTexture = LoadTexture("assets/boss.bmp");
        } else {
            Image img2 = GenImageColor(96, 96, RED);
            // desenha olhos simples
            for (int y = 28; y < 40; y++) {
                for (int x = 22; x < 30; x++) ImageDrawPixel(&img2, x, y, WHITE);
                for (int x = 66; x < 74; x++) ImageDrawPixel(&img2, x, y, WHITE);
            }
            SaveImageAsBMP(img2, "assets/boss.bmp");
            bossTexture = LoadTextureFromImage(img2);
            UnloadImage(img2);
        }

        // Try loading an animated GIF for the boss (assets/boss.gif)
        // Note: full animated GIF decoding depends on your raylib build.
        // As a safe fallback, load boss.gif as a regular texture (first frame).
        bossFrameCount = 0;
        bossFrameIndex = 0;
        bossFrameTime = 0.0f;
        if (FileExists("assets/boss.gif")) {
            bossTexture = LoadTexture("assets/boss.gif");
            // If your raylib supports LoadImageAnim, we can later adapt to extract frames.
        }

        // Load optional boss attack and hit/player hit textures
        if (FileExists("assets/boss_attack.png")) {
            bossAttackTexture = LoadTexture("assets/boss_attack.png");
        } else if (FileExists("assets/boss_attack.bmp")) {
            bossAttackTexture = LoadTexture("assets/boss_attack.bmp");
        } else {
            bossAttackTexture = (Texture2D){0};
        }

        // if boss attack texture is a spritesheet (horizontal strip), compute frames
        bossAttackFrameCount = 0;
        bossAttackFrame = 0;
        bossAttackFrameTime = 0.0f;
        bossAttackFrameDuration = BOSS_ATTACK_DURATION;
        if (bossAttackTexture.id != 0) {
            if (bossAttackTexture.height > 0) {
                bossAttackFrameCount = bossAttackTexture.width / bossAttackTexture.height;
                if (bossAttackFrameCount < 1) bossAttackFrameCount = 1;
                bossAttackFrameDuration = BOSS_ATTACK_DURATION / (float)bossAttackFrameCount;
            } else {
                bossAttackFrameCount = 1;
                bossAttackFrameDuration = BOSS_ATTACK_DURATION;
            }
        }

        if (FileExists("assets/player_hit.png")) {
            playerHitTexture = LoadTexture("assets/player_hit.png");
        } else if (FileExists("assets/player_hit.bmp")) {
            playerHitTexture = LoadTexture("assets/player_hit.bmp");
        } else {
            playerHitTexture = (Texture2D){0};
        }

        if (FileExists("assets/boss_hit.png")) {
            bossHitTexture = LoadTexture("assets/boss_hit.png");
        } else if (FileExists("assets/boss_hit.bmp")) {
            bossHitTexture = LoadTexture("assets/boss_hit.bmp");
        } else {
            bossHitTexture = (Texture2D){0};
        }

        // Try loading optional attack spritesheet: assets/player_attack.png or .bmp
        playerAttackFrameCount = 0;
        playerAttackFrame = 0;
        playerAttackFrameTime = 0.0f;
        playerAttackFrameDuration = PLAYER_ATTACK_DURATION; // will be divided if frames exist
        if (FileExists("assets/player_attack.png")) {
            playerAttackTexture = LoadTexture("assets/player_attack.png");
        } else if (FileExists("assets/player_attack.bmp")) {
            playerAttackTexture = LoadTexture("assets/player_attack.bmp");
        } else {
            playerAttackTexture = (Texture2D){0};
        }

        if (playerAttackTexture.id != 0) {
            // assume horizontal strip: frames = width / height
            if (playerAttackTexture.height > 0) {
                playerAttackFrameCount = playerAttackTexture.width / playerAttackTexture.height;
                if (playerAttackFrameCount < 1) playerAttackFrameCount = 1;
                playerAttackFrameDuration = PLAYER_ATTACK_DURATION / (float)playerAttackFrameCount;
            } else {
                playerAttackFrameCount = 1;
                playerAttackFrameDuration = PLAYER_ATTACK_DURATION;
            }
        }

        texturesInitialized = true;
        // initialize boss/player attack/hurt states
        bossIsAttacking = false;
        bossAttackTimer = 0.0f;
        bossHurtTimer = 0.0f;
    }
}

//------------------------------------------------------------------------------------
// UPDATE
//------------------------------------------------------------------------------------

void UpdateExplore(void) {
    if (itemMessageTimer > 0) {
        itemMessageTimer -= GetFrameTime();
        if (itemMessageTimer <= 0 && currentStage >= 4) {
            currentState = GAME_STATE_BATTLE;
            battleState = BATTLE_PLAYER_TURN;
        }
        return;
    }

    int choice = -1;

    // Movimento do personagem pela sala
    float delta = GetFrameTime();
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
        explorePlayerX += explorePlayerSpeed * delta;
    }
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
        explorePlayerX -= explorePlayerSpeed * delta;
    }

    // Mantem dentro da sala (limites simples)
    if (explorePlayerX < 60) explorePlayerX = 60;
    if (explorePlayerX > SCREEN_WIDTH - 80) explorePlayerX = SCREEN_WIDTH - 80;

    // Ao pressionar ENTER, verificar se o jogador esta proximo a uma porta
    if (IsKeyPressed(KEY_ENTER)) {
        // verifica colisao simples com a area da porta
        Rectangle playerRect = (Rectangle){ explorePlayerX - 5, explorePlayerY - 10, 30, 60 };
        int chosen = -1;
        if (CheckCollisionRecs(playerRect, doorLeftRect)) chosen = 0;
        if (CheckCollisionRecs(playerRect, doorRightRect)) chosen = 1;

        if (chosen != -1) {
            lastItemCollected = GetItemForChoice(currentStage, chosen);
            if (inventoryCount < INVENTORY_SIZE) {
                inventory[inventoryCount] = lastItemCollected;
                inventoryCount++;
            }
            currentStage++;
            itemMessageTimer = 2.0f;
        }
    }
}

void UpdateBattle(void) {
    float delta = GetFrameTime();

    // Animations timers
    if (playerIsAttacking) {
        playerAttackTimer -= delta;
        if (playerAttackTimer <= 0.0f) {
            playerIsAttacking = false;
            playerAttackTimer = 0.0f;
        }
        // Advance attack spritesheet frames if available
        if (playerAttackFrameCount > 1) {
            playerAttackFrameTime += delta;
            if (playerAttackFrameTime >= playerAttackFrameDuration) {
                playerAttackFrameTime -= playerAttackFrameDuration;
                playerAttackFrame++;
                if (playerAttackFrame >= playerAttackFrameCount) playerAttackFrame = playerAttackFrameCount - 1;
            }
        }
    }
    if (bossIsAttacking) {
        bossAttackTimer -= delta;
        if (bossAttackTimer <= 0.0f) {
            bossIsAttacking = false;
            bossAttackTimer = 0.0f;
        }
        // advance boss attack spritesheet frames
        if (bossAttackFrameCount > 1) {
            bossAttackFrameTime += delta;
            if (bossAttackFrameTime >= bossAttackFrameDuration) {
                bossAttackFrameTime -= bossAttackFrameDuration;
                bossAttackFrame++;
                if (bossAttackFrame >= bossAttackFrameCount) bossAttackFrame = bossAttackFrameCount - 1;
            }
        }
    }
    if (bossHurtTimer > 0.0f) {
        bossHurtTimer -= delta;
        if (bossHurtTimer < 0.0f) bossHurtTimer = 0.0f;
    }
    if (playerHurtTimer > 0.0f) {
        playerHurtTimer -= delta;
        if (playerHurtTimer < 0.0f) playerHurtTimer = 0.0f;
    }

    // Advance boss GIF frames if loaded
    if (bossFrameCount > 0) {
        bossFrameTime += delta;
        if (bossFrameTime >= bossFrameDuration) {
            bossFrameTime -= bossFrameDuration;
            bossFrameIndex = (bossFrameIndex + 1) % bossFrameCount;
        }
    }

    if (boss.hp <= 0) {
        currentState = GAME_STATE_ENDING_GOOD;
        return;
    }
    if (player.hp <= 0) {
        currentState = GAME_STATE_ENDING_BAD;
        return;
    }

    if (battleState == BATTLE_PLAYER_TURN) {
        if (IsKeyPressed(KEY_RIGHT)) {
            selectedItemIndex = (selectedItemIndex + 1) % INVENTORY_SIZE;
        }
        if (IsKeyPressed(KEY_LEFT)) {
            selectedItemIndex = (selectedItemIndex - 1 + INVENTORY_SIZE) % INVENTORY_SIZE;
        }

        if (IsKeyPressed(KEY_A)) {
            PlayerAttack();
        }
        if (IsKeyPressed(KEY_ENTER)) {
            UseItem(selectedItemIndex);
        }
    } else if (battleState == BATTLE_BOSS_TURN) {
        bossTurnTimer -= GetFrameTime();
        if (bossTurnTimer <= 0) {
            BossAttack();
            battleState = BATTLE_PLAYER_TURN;
        }
    }
}

void UpdateEnding(void) {
    if (IsKeyPressed(KEY_ENTER)) {
        InitGame();
    }
}

//------------------------------------------------------------------------------------
// DRAW
//------------------------------------------------------------------------------------

// Forward declaration for BMP saver
static bool SaveImageAsBMP(const Image img, const char *fileName);

// Simple BMP saver for Raylib Image (assumes image format UNCOMPRESSED_R8G8B8A8)
static bool SaveImageAsBMP(const Image img, const char *fileName) {
    if (!img.data) return false;
    FILE *f = fopen(fileName, "wb");
    if (!f) return false;

    int width = img.width;
    int height = img.height;
    int rowSize = (width * 3 + 3) & (~3); // padded to 4 bytes
    int dataSize = rowSize * height;

    uint32_t fileSize = 14 + 40 + dataSize;

    unsigned char fileHeader[14] = {0};
    unsigned char infoHeader[40] = {0};

    // BITMAPFILEHEADER
    fileHeader[0] = 'B'; fileHeader[1] = 'M';
    fileHeader[2] = (unsigned char)(fileSize & 0xFF);
    fileHeader[3] = (unsigned char)((fileSize >> 8) & 0xFF);
    fileHeader[4] = (unsigned char)((fileSize >> 16) & 0xFF);
    fileHeader[5] = (unsigned char)((fileSize >> 24) & 0xFF);
    fileHeader[10] = 14 + 40; // pixel data offset

    // BITMAPINFOHEADER
    infoHeader[0] = 40;
    infoHeader[4] = (unsigned char)(width & 0xFF);
    infoHeader[5] = (unsigned char)((width >> 8) & 0xFF);
    infoHeader[6] = (unsigned char)((width >> 16) & 0xFF);
    infoHeader[7] = (unsigned char)((width >> 24) & 0xFF);
    infoHeader[8] = (unsigned char)(height & 0xFF);
    infoHeader[9] = (unsigned char)((height >> 8) & 0xFF);
    infoHeader[10] = (unsigned char)((height >> 16) & 0xFF);
    infoHeader[11] = (unsigned char)((height >> 24) & 0xFF);
    infoHeader[12] = 1; // planes
    infoHeader[14] = 24; // bits per pixel

    fwrite(fileHeader, 1, 14, f);
    fwrite(infoHeader, 1, 40, f);

    unsigned char *pixels = (unsigned char *)img.data; // RGBA8

    // BMP stores pixels bottom-to-top
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            unsigned char r = pixels[(y*width + x)*4 + 0];
            unsigned char g = pixels[(y*width + x)*4 + 1];
            unsigned char b = pixels[(y*width + x)*4 + 2];
            unsigned char bggr[3] = { b, g, r };
            fwrite(bggr, 1, 3, f);
        }
        // padding
        for (int p = 0; p < (rowSize - width*3); p++) fputc(0, f);
    }

    fclose(f);
    return true;
}

void DrawPlayerSprite(int posX, int posY) {
    DrawCircle(posX + 10, posY - 10, 10, (Color){255, 200, 150, 255});
    DrawCircleLines(posX + 10, posY - 10, 10, BLACK);
    DrawCircle(posX + 6, posY - 12, 2, BLACK);
    DrawCircle(posX + 14, posY - 12, 2, BLACK);
    DrawRectangle(posX, posY, 20, 40, (Color){0, 150, 255, 255});
    DrawRectangleLines(posX, posY, 20, 40, (Color){0, 100, 200, 255});
    DrawRectangle(posX - 5, posY + 5, 5, 20, (Color){255, 200, 150, 255});
    DrawRectangle(posX + 20, posY + 5, 5, 20, (Color){255, 200, 150, 255});
    DrawRectangle(posX + 2, posY + 40, 6, 20, (Color){50, 50, 50, 255});
    DrawRectangle(posX + 12, posY + 40, 6, 20, (Color){50, 50, 50, 255});
}

void DrawBossSprite(int posX, int posY) {
    DrawCircle(posX + 20, posY + 30, 50, (Color){100, 0, 0, 100});
    DrawCircle(posX + 20, posY - 15, 15, (Color){80, 20, 20, 255});
    DrawCircleLines(posX + 20, posY - 15, 15, (Color){200, 50, 50, 255});
    DrawCircle(posX + 14, posY - 18, 3, (Color){255, 100, 0, 255});
    DrawCircle(posX + 26, posY - 18, 3, (Color){255, 100, 0, 255});
    DrawRectangle(posX, posY, 40, 60, (Color){200, 0, 0, 255});
    DrawRectangleLines(posX, posY, 40, 60, (Color){100, 0, 0, 255});
    DrawRectangle(posX - 10, posY + 10, 10, 30, (Color){150, 0, 0, 255});
    DrawRectangle(posX + 40, posY + 10, 10, 30, (Color){150, 0, 0, 255});
    DrawRectangle(posX + 5, posY + 60, 10, 30, (Color){100, 0, 0, 255});
    DrawRectangle(posX + 25, posY + 60, 10, 30, (Color){100, 0, 0, 255});
}


void DrawExplore(void) {
    ClearBackground((Color){20, 20, 40, 255});

    if (itemMessageTimer > 0) {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){0, 0, 0, 200});
        DrawRectangle(SCREEN_WIDTH / 2 - 300, SCREEN_HEIGHT / 2 - 100, 600, 200, (Color){50, 50, 100, 255});
        DrawRectangleLines(SCREEN_WIDTH / 2 - 300, SCREEN_HEIGHT / 2 - 100, 600, 200, (Color){100, 200, 255, 255});
        
        sprintf(messageBuffer, "Voce coletou: %s!", GetItemName(lastItemCollected));
        int textWidth = MeasureText(messageBuffer, 30);
        DrawText(messageBuffer, SCREEN_WIDTH / 2 - textWidth / 2, SCREEN_HEIGHT / 2 - 50, 30, (Color){100, 255, 150, 255});
        
        const char* msg2 = "Carregando proximo cenario...";
        int textWidth2 = MeasureText(msg2, 18);
        DrawText(msg2, SCREEN_WIDTH / 2 - textWidth2 / 2, SCREEN_HEIGHT / 2 + 40, 18, (Color){150, 150, 200, 255});
        return;
    }
    const char* storyText = "";
    switch (currentStage) {
        case 0: storyText = "Voce chega na entrada da masmorra. O caminho esta fechado,\nvoce segue pela: Caverna umida ou Ponte antiga"; break;
        case 1: storyText = "Voce entra em um salao com dois pedestais, voce toca o:\npedestal das chamas ou porta das sombras."; break;
        case 2: storyText = "Um guarda protege o caminho. Ir pelo tunel escuro\n ou usar o sino quebrado para distrai-lo."; break;
        case 3: storyText = "Voce esta cada vez mais perto: escolha um lado e avance. Lado bronze\n ou lado prata"; break;
    }

    // Desenhar a sala
    DrawRectangle(60, 120, SCREEN_WIDTH - 120, 420, (Color){30, 40, 70, 255});
    DrawRectangleLines(60, 120, SCREEN_WIDTH - 120, 420, (Color){100, 150, 200, 255});
    DrawText(storyText, 80, 140, 22, (Color){200, 220, 255, 255});

    // Desenhar portas esquerda e direita com rótulos por estágio
    const char* leftDoorLabel = "Porta A";
    const char* rightDoorLabel = "Porta B";
    switch (currentStage) {
        case 0:
            leftDoorLabel = "Caverna umida";
            rightDoorLabel = "Ponte antiga";
            break;
        case 1:
            leftDoorLabel = "Pedestal da Chama";
            rightDoorLabel = "Pedestal da Sombra";
            break;
        case 2:
            leftDoorLabel = "Tunel Escuro";
            rightDoorLabel = "Sino Quebrado";
            break;
        case 3:
            leftDoorLabel = "Lado Bronze";
            rightDoorLabel = "Lado Prata";
            break;
    }

    DrawRectangleRec(doorLeftRect, (Color){80, 40, 30, 255});
    DrawRectangleLines((int)doorLeftRect.x, (int)doorLeftRect.y, (int)doorLeftRect.width, (int)doorLeftRect.height, (Color){200, 180, 150, 255});
    DrawText(leftDoorLabel, (int)doorLeftRect.x + 10, (int)doorLeftRect.y + 90, 16, (Color){255, 220, 180, 255});

    DrawRectangleRec(doorRightRect, (Color){40, 60, 90, 255});
    DrawRectangleLines((int)doorRightRect.x, (int)doorRightRect.y, (int)doorRightRect.width, (int)doorRightRect.height, (Color){180, 200, 255, 255});
    DrawText(rightDoorLabel, (int)doorRightRect.x + 10, (int)doorRightRect.y + 90, 16, (Color){220, 240, 255, 255});

    // Desenhar o personagem explorando
    DrawPlayerSprite((int)explorePlayerX, (int)explorePlayerY);

    // Indicar se esta proximo a uma porta
    Rectangle playerRect = (Rectangle){ explorePlayerX - 5, explorePlayerY - 10, 30, 60 };
    if (CheckCollisionRecs(playerRect, doorLeftRect)) {
        DrawTextEx(GetFontDefault(), TextFormat("[ENTER] Entrar em %s", leftDoorLabel), (Vector2){80, SCREEN_HEIGHT - 120}, 18, 1, (Color){200, 255, 200, 255});
    } else if (CheckCollisionRecs(playerRect, doorRightRect)) {
        DrawTextEx(GetFontDefault(), TextFormat("[ENTER] Entrar em %s", rightDoorLabel), (Vector2){80, SCREEN_HEIGHT - 120}, 18, 1, (Color){200, 255, 200, 255});
    } else {
        DrawText("Use SETAS ou A/D para mover. Aproximo-se da porta e pressione [ENTER].", 80, SCREEN_HEIGHT - 120, 16, (Color){180, 200, 255, 255});
    }
}

void DrawBattle(void) {
    char bossHpText[64];
    char playerHpText[64];
    
    ClearBackground((Color){30, 30, 50, 255});

    // --- Desenhar Chefe ---
    // compute boss draw position and scale first
    Vector2 posB = { (float)(SCREEN_WIDTH/2), 110 };
    float scaleB = 1.0f;
    float bossDrawHeight = 0.0f;
    // compute a stable base height for the boss (used to position HP bar and player)
    float bossBaseHeight = 120.0f;
    if (texturesInitialized) {
        if (bossFrameCount > 0 && bossFrames != NULL) {
            bossBaseHeight = (float)bossFrames[0].height * scaleB;
        } else if (bossTexture.id != 0) {
            bossBaseHeight = (float)bossTexture.height * scaleB;
        }
    }

    // Boss attack movement: lunge toward player area while attacking
    float bossAttackOffsetX = 0.0f;
    float bossAttackOffsetY = 0.0f;
    if (bossIsAttacking) {
        float prog = 1.0f - (bossAttackTimer / BOSS_ATTACK_DURATION);
        if (prog < 0.0f) prog = 0.0f;
        if (prog > 1.0f) prog = 1.0f;
        float ease = sinf(prog * 3.14159f);
        // small, subtle lunge: limit movement to a few dozen pixels so it doesn't go too far
        const float LUNGE_X = 20.0f; // horizontal pixels
        const float LUNGE_Y = 170.0f; // vertical pixels
        float targetX = posB.x - LUNGE_X; // slight left
        float targetY = posB.y + LUNGE_Y; // slight down toward player
        bossAttackOffsetX = (targetX - posB.x) * ease;
        bossAttackOffsetY = (targetY - posB.y) * ease;
    }
    Vector2 drawPosB = { posB.x + bossAttackOffsetX, posB.y + bossAttackOffsetY };

    if (texturesInitialized) {
        // boss tint (blink when hurt)
        float bossAlphaF = 1.0f;
        if (bossHurtTimer > 0.0f) {
            bossAlphaF = (sinf(bossHurtTimer * 30.0f) > 0.0f) ? 1.0f : 0.25f;
        }
        Color bossTint = (Color){255,255,255,(unsigned char)(255.0f * bossAlphaF)};

        // If boss is attacking and has an attack texture, draw the attack animation instead of the idle sprite
        if (bossIsAttacking && bossAttackTexture.id != 0) {
            if (bossAttackFrameCount > 1) {
                int frameW = (bossAttackFrameCount > 0) ? (bossAttackTexture.width / bossAttackFrameCount) : bossAttackTexture.width;
                int frameH = bossAttackTexture.height;
                Rectangle srcAtk = { (float)(bossAttackFrame * frameW), 0, (float)frameW, (float)frameH };
                Vector2 originAtk = { frameW/2.0f, frameH/2.0f };
                Rectangle destAtk = { drawPosB.x, drawPosB.y, frameW * scaleB, frameH * scaleB };
                DrawTexturePro(bossAttackTexture, srcAtk, destAtk, originAtk, 0.0f, bossTint);
                bossDrawHeight = frameH * scaleB;
            } else {
                Rectangle srcAtk = {0,0,(float)bossAttackTexture.width,(float)bossAttackTexture.height};
                Vector2 originAtk = { bossAttackTexture.width/2.0f, bossAttackTexture.height/2.0f };
                Rectangle destAtk = { drawPosB.x, drawPosB.y, bossAttackTexture.width*scaleB, bossAttackTexture.height*scaleB };
                DrawTexturePro(bossAttackTexture, srcAtk, destAtk, originAtk, 0.0f, bossTint);
                bossDrawHeight = bossAttackTexture.height * scaleB;
            }
        } else {
            // draw idle boss (frames or static)
            if (bossFrameCount > 0 && bossFrames != NULL) {
                Texture2D tex = bossFrames[bossFrameIndex];
                Rectangle srcB = {0,0,(float)tex.width,(float)tex.height};
                Vector2 originB = { tex.width/2.0f, tex.height/2.0f };
                Rectangle destB = { drawPosB.x, drawPosB.y, tex.width*scaleB, tex.height*scaleB };
                DrawTexturePro(tex, srcB, destB, originB, 0.0f, bossTint);
                bossDrawHeight = tex.height * scaleB;
            } else if (bossTexture.id != 0) {
                Rectangle srcB = {0,0,(float)bossTexture.width,(float)bossTexture.height};
                Vector2 originB = { bossTexture.width/2.0f, bossTexture.height/2.0f };
                Rectangle destB = { drawPosB.x, drawPosB.y, bossTexture.width*scaleB, bossTexture.height*scaleB };
                DrawTexturePro(bossTexture, srcB, destB, originB, 0.0f, bossTint);
                bossDrawHeight = bossTexture.height * scaleB;
            } else {
                DrawBossSprite((int)drawPosB.x - 20, (int)drawPosB.y - 60);
                bossDrawHeight = 120.0f; // approximate height of the shape fallback
            }
        }
    } else {
        DrawBossSprite(SCREEN_WIDTH / 2 - 20, 50);
        bossDrawHeight = 120.0f;
    }

    // If boss is hurt and has a hit texture, draw it over the boss
    if (bossHurtTimer > 0.0f && bossHitTexture.id != 0) {
        float hitW = (float)bossHitTexture.width;
        float hitH = (float)bossHitTexture.height;
        Rectangle srcHit = {0,0,hitW,hitH};
        Vector2 originHit = { hitW/2.0f, hitH/2.0f };
        Rectangle destHit = { drawPosB.x, drawPosB.y, hitW, hitH };
        DrawTexturePro(bossHitTexture, srcHit, destHit, originHit, 0.0f, WHITE);
    }

    // HP do Chefe com borda, positioned below the boss base position (doesn't move with lunge)
    float bossBarY = posB.y + (bossBaseHeight / 2.0f) + 12.0f;
    DrawRectangle((float)SCREEN_WIDTH / 2 - 150, bossBarY, 300, 25, (Color){50, 50, 80, 255});
    DrawRectangleLines((float)SCREEN_WIDTH / 2 - 150, bossBarY, 300, 25, (Color){100, 100, 150, 255});
    DrawRectangle((float)SCREEN_WIDTH / 2 - 150, bossBarY, (int)(300.0f * ((float)boss.hp / boss.maxHp)), 25, (Color){255, 50, 50, 255});
    sprintf(bossHpText, "HP: %d / %d", boss.hp, boss.maxHp);
    DrawText(bossHpText, SCREEN_WIDTH / 2 - 65, (int)bossBarY + 2, 20, WHITE);

    // --- Desenhar Jogador (com sprites e animações) ---
    int baseX = SCREEN_WIDTH / 2 - 10;
    // compute player center Y so player's feet are below the boss HP bar
    float playerScale = 1.0f;
    float playerTexH = 40.0f; // default approximate height for fallback
    if (texturesInitialized) {
        if (playerIsAttacking && playerAttackTexture.id != 0 && playerAttackFrameCount > 0) {
            playerTexH = (float)playerAttackTexture.height;
        } else {
            playerTexH = (float)playerTexture.height;
        }
    }
    float playerCenterY = bossBarY + 25.0f + (playerTexH * playerScale) / 2.0f + 8.0f;

    // Attack movement: player moves toward boss when attacking
    float attackOffsetX = 0.0f;
    float attackOffsetY = 0.0f;
    if (playerIsAttacking) {
        float progress = 1.0f - (playerAttackTimer / PLAYER_ATTACK_DURATION);
        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;
        float ease = sinf(progress * 3.14159f);
        // target in front of boss
        float bossCenterX = drawPosB.x;
        float bossCenterY = drawPosB.y;
        float targetX = bossCenterX - 40.0f; // slightly left of boss center
        float targetY = bossCenterY + (bossDrawHeight * 0.15f);
        // normal player center (below boss bar)
        float normalX = (float)SCREEN_WIDTH/2.0f;
        float normalY = playerCenterY;
        // lerp
        float curX = normalX + (targetX - normalX) * ease;
        float curY = normalY + (targetY - normalY) * ease;
        attackOffsetX = curX - (float)SCREEN_WIDTH/2.0f;
        attackOffsetY = curY - playerCenterY;
    }

    // Hurt blinking alpha for player
    float alphaF = 1.0f;
    if (playerHurtTimer > 0.0f) {
        alphaF = (sinf(playerHurtTimer * 30.0f) > 0.0f) ? 1.0f : 0.25f;
    }
    Color tint = (Color){255, 255, 255, (unsigned char)(255.0f * alphaF)};

    int drawX = baseX + (int)attackOffsetX;
    float drawCenterY = playerCenterY + attackOffsetY;

    // Draw player sprite scaled (use attack spritesheet if playing attack)
    if (texturesInitialized) {
        if (playerIsAttacking && playerAttackTexture.id != 0 && playerAttackFrameCount > 0) {
            int frameW = playerAttackTexture.height; // frames are square height x height
            int frameH = playerAttackTexture.height;
            Rectangle src = { (float)(playerAttackFrame * frameW), 0, (float)frameW, (float)frameH };
            Vector2 origin = { frameW/2.0f, frameH/2.0f };
            Rectangle dest = { (float)(SCREEN_WIDTH/2) + attackOffsetX, drawCenterY, frameW * playerScale, frameH * playerScale };
            DrawTexturePro(playerAttackTexture, src, dest, origin, 0.0f, tint);
        } else {
            float scale = playerScale;
            Rectangle src = {0, 0, (float)playerTexture.width, (float)playerTexture.height};
            Vector2 origin = { playerTexture.width/2.0f, playerTexture.height/2.0f };
            Rectangle dest = { (float)(SCREEN_WIDTH/2) + attackOffsetX, drawCenterY, playerTexture.width*scale, playerTexture.height*scale };
            DrawTexturePro(playerTexture, src, dest, origin, 0.0f, tint);
        }
        // player hit overlay
        if (playerHurtTimer > 0.0f && playerHitTexture.id != 0) {
            float hitW = (float)playerHitTexture.width;
            float hitH = (float)playerHitTexture.height;
            Rectangle srcHit = {0,0,hitW,hitH};
            Vector2 originHit = { hitW/2.0f, hitH/2.0f };
            Rectangle destHit = { (float)(SCREEN_WIDTH/2) + attackOffsetX, drawCenterY, hitW, hitH };
            DrawTexturePro(playerHitTexture, srcHit, destHit, originHit, 0.0f, WHITE);
        }
    } else {
        // fallback: draw primitive sprite with top-left positioned so it sits below the bar
        int fallbackTop = (int)(playerCenterY - 10.0f);
        DrawPlayerSprite(drawX, fallbackTop);
        if (playerHurtTimer > 0.0f && playerHitTexture.id != 0) {
            // overlay simple rectangle to indicate hit when using fallback
            DrawRectangle(drawX - 5, fallbackTop - 10, 40, 10, RED);
        }
    }
    
    // HP do Jogador com borda (moved slightly down)
    float playerBarY = 350.0f + 12.0f; // lowered by 12 pixels
    DrawRectangle(SCREEN_WIDTH / 2 - 150, (int)playerBarY, 300, 25, (Color){50, 50, 80, 255});
    DrawRectangleLines(SCREEN_WIDTH / 2 - 150, (int)playerBarY, 300, 25, (Color){100, 100, 150, 255});
    DrawRectangle(SCREEN_WIDTH / 2 - 150, (int)playerBarY, (int)(300.0f * ((float)player.hp / player.maxHp)), 25, (Color){50, 200, 100, 255});
    sprintf(playerHpText, "HP: %d / %d", player.hp, player.maxHp);
    DrawText(playerHpText, SCREEN_WIDTH / 2 - 65, (int)playerBarY + 2, 20, WHITE);

    // --- Mensagem de Batalha ---
    DrawRectangle(0, SCREEN_HEIGHT - 180, SCREEN_WIDTH, 70, (Color){20, 20, 40, 255});
    DrawRectangleLines(0, SCREEN_HEIGHT - 180, SCREEN_WIDTH, 70, (Color){100, 150, 200, 255});
    DrawText(battleMessage, 25, SCREEN_HEIGHT - 160, 16, (Color){150, 255, 200, 255});

    DrawText("INVENTARIO (Use SETAS e ENTER):", 20, SCREEN_HEIGHT - 100, 16, (Color){100, 200, 255, 255});
    
    int itemPosX = 20;
    int itemWidth = 180;
    
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        Color itemColor = (Color){200, 200, 220, 255};
        Color bgColor = (Color){50, 50, 100, 255};
        
        // Item selecionado
        if (i == selectedItemIndex) {
            DrawRectangle(itemPosX - 5, SCREEN_HEIGHT - 70, itemWidth + 10, 40, (Color){100, 200, 255, 255});
            DrawRectangleLines(itemPosX - 5, SCREEN_HEIGHT - 70, itemWidth + 10, 40, (Color){150, 255, 200, 255});
            itemColor = (Color){20, 20, 40, 255};
        } else {
            DrawRectangle(itemPosX - 5, SCREEN_HEIGHT - 70, itemWidth + 10, 40, bgColor);
            DrawRectangleLines(itemPosX - 5, SCREEN_HEIGHT - 70, itemWidth + 10, 40, (Color){80, 80, 120, 255});
        }
        
        // Item já usado
        if (itemUsed[i]) {
            itemColor = (Color){100, 100, 100, 255};
        }
        
        const char* displayName = GetItemName(inventory[i]);
        if (inventory[i] == ITEM_SWORD || inventory[i] == ITEM_ARMOR) displayName = "Vazio";
        DrawText(displayName, itemPosX + 5, SCREEN_HEIGHT - 58, 12, itemColor);
        itemPosX += itemWidth + 20;
    }

    int btnW = 130;
    int btnH = 45;
    int btnX = SCREEN_WIDTH - btnW - 20;
    int btnY = SCREEN_HEIGHT - 85;
    DrawRectangle(btnX, btnY, btnW, btnH, (Color){180, 50, 50, 255});
    DrawRectangleLines(btnX, btnY, btnW, btnH, (Color){255, 100, 100, 255});
    DrawText("ATACAR [A]", btnX + 10, btnY + 10, 16, WHITE);
}

void DrawEnding(bool playerWon) {
    ClearBackground((Color){20, 20, 40, 255});
    DrawRectangle(SCREEN_WIDTH / 2 - 400, SCREEN_HEIGHT / 2 - 200, 800, 400, (Color){40, 40, 80, 200});
    DrawRectangleLines(SCREEN_WIDTH / 2 - 400, SCREEN_HEIGHT / 2 - 200, 800, 400, (Color){100, 200, 255, 255});
    
    if (playerWon) {
        DrawText("VITORIA!", 280, 200, 60, (Color){100, 255, 100, 255});
        DrawText("O Chefe foi derrotado e a masmorra esta segura.", 120, 300, 22, (Color){200, 255, 200, 255});
    } else {
        DrawText("GAME OVER", 260, 200, 60, (Color){255, 80, 80, 255});
        DrawText("Voce foi derrotado...", 280, 300, 22, (Color){255, 150, 150, 255});
    }
    
    DrawText("Pressione [ENTER] para jogar novamente.", 200, 500, 20, (Color){150, 200, 255, 255});
}

void DrawEscapeEnding(void) {
    ClearBackground((Color){20, 20, 40, 255});
    DrawRectangle(SCREEN_WIDTH / 2 - 400, SCREEN_HEIGHT / 2 - 200, 800, 400, (Color){40, 40, 80, 200});
    DrawRectangleLines(SCREEN_WIDTH / 2 - 400, SCREEN_HEIGHT / 2 - 200, 800, 400, (Color){255, 200, 100, 255});
    
    DrawText("FUGA!", 300, 200, 60, (Color){255, 200, 100, 255});
    DrawText("Voce distraiu o chefe com a moeda e conseguiu fugir da masmorra.", 80, 300, 20, (Color){255, 220, 150, 255});
    DrawText("Este final conta como uma fuga — nem vitoria nem derrota.", 100, 340, 18, (Color){200, 180, 120, 255});
    DrawText("Pressione [ENTER] para jogar novamente.", 200, 520, 20, (Color){150, 200, 255, 255});
}

int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Raylib RPG de Turnos");
    InitGame();
    SetTargetFPS(60);

    // Loop principal
    while (!WindowShouldClose()) {

        switch (currentState) {
            case GAME_STATE_EXPLORE:
                UpdateExplore();
                break;
            case GAME_STATE_BATTLE:
                UpdateBattle();
                break;
            case GAME_STATE_ENDING_GOOD:
            case GAME_STATE_ENDING_BAD:
            case GAME_STATE_ENDING_ESCAPE:
                UpdateEnding();
                break;
        }

        BeginDrawing();

        switch (currentState) {
            case GAME_STATE_EXPLORE:
                DrawExplore();
                break;
            case GAME_STATE_BATTLE:
                DrawBattle();
                break;
            case GAME_STATE_ENDING_GOOD:
                DrawEnding(true);
                break;
            case GAME_STATE_ENDING_BAD:
                DrawEnding(false);
                break;
            case GAME_STATE_ENDING_ESCAPE:
                DrawEscapeEnding();
                break;
        }

            EndDrawing();
        }

        // Cleanup textures before exit
        if (playerAttackTexture.id) UnloadTexture(playerAttackTexture);
        if (playerTexture.id) UnloadTexture(playerTexture);
        if (bossTexture.id) UnloadTexture(bossTexture);
        if (bossFrames) {
            for (int i = 0; i < bossFrameCount; i++) {
                if (bossFrames[i].id) UnloadTexture(bossFrames[i]);
            }
            free(bossFrames);
            bossFrames = NULL;
            bossFrameCount = 0;
        }
        if (bossAttackTexture.id) UnloadTexture(bossAttackTexture);
        if (playerHitTexture.id) UnloadTexture(playerHitTexture);
        if (bossHitTexture.id) UnloadTexture(bossHitTexture);

        CloseWindow();
        return 0;
    }
