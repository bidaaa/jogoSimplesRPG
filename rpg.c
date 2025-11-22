#include "raylib.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define SCREEN_WIDTH 1000
#define SCREEN_HEIGHT 700
#define INVENTORY_SIZE 4

typedef enum
{
    GAME_STATE_TITLE,
    GAME_STATE_EXPLORE,
    GAME_STATE_BATTLE,
    GAME_STATE_ENDING_GOOD,
    GAME_STATE_ENDING_BAD,
    GAME_STATE_ENDING_ESCAPE
} GameState;

typedef enum
{
    ITEM_NONE,
    ITEM_POTION,
    ITEM_SWORD,
    ITEM_BOMB,
    ITEM_COIN,
    ITEM_ARMOR
} ItemType;

typedef enum
{
    BATTLE_PLAYER_TURN,
    BATTLE_BOSS_TURN
} BattleState;

typedef struct
{
    int hp;
    int maxHp;
} Player;

typedef struct
{
    int hp;
    int maxHp;
    int attack;
} Boss;

static GameState currentState;
static BattleState battleState;

static Player player;
static Boss boss;

static ItemType inventory[INVENTORY_SIZE];
static bool itemUsed[INVENTORY_SIZE];
static int inventoryCount;
static int selectedItemIndex;
static int currentStage;

static const char *battleMessage;
static char messageBuffer[256];
static float itemMessageTimer = 0.0f;
static ItemType lastItemCollected = ITEM_NONE;

static bool playerHasArmor;
static float bossTurnTimer;
static float playerAttackTimer;
static const float PLAYER_ATTACK_DURATION = 0.45f;
static bool playerIsAttacking;
static float playerHurtTimer;
static const float PLAYER_HURT_DURATION = 0.9f;

static bool bossIsAttacking;
static float bossAttackTimer;
static const float BOSS_ATTACK_DURATION = 0.40f;
static float bossHurtTimer;
static const float BOSS_HURT_DURATION = 0.9f;

static Texture2D playerTexture;
static Texture2D bossTexture;
static Texture2D titleBackgroundTexture;
static Texture2D battleBackgroundTexture;
static Texture2D playerAttackTexture;
static Texture2D bossAttackTexture;
static Texture2D playerHitTexture;
static Texture2D bossHitTexture;

static Texture2D bgStage1;
static Texture2D bgStage2;
static Texture2D bgStage3;
static Texture2D bgStage4;

static bool texturesInitialized = false;

static int playerAttackFrameCount;
static int playerAttackFrame;
static float playerAttackFrameTime;
static float playerAttackFrameDuration;

static int bossAttackFrameCount;
static int bossAttackFrame;
static float bossAttackFrameTime;
static float bossAttackFrameDuration;

static float explorePlayerX;
static float explorePlayerY;
static Rectangle doorLeftRect;
static Rectangle doorRightRect;
static float explorePlayerSpeed;

const char *GetItemName(ItemType item)
{
    switch (item)
    {
    case ITEM_POTION:
        return "Pocao (Cura 50 HP)";
    case ITEM_SWORD:
        return "Espada (Dano 30)";
    case ITEM_BOMB:
        return "Bomba (Dano 70)";
    case ITEM_COIN:
        return "Moeda (Pode \ndistrair o chefe)";
    case ITEM_ARMOR:
        return "Armadura (Reduz dano)";
    default:
        return "Vazio";
    }
}

ItemType GetItemForChoice(int stage, int choice)
{
    if (stage == 0)
        return ITEM_POTION;
    if (stage == 1)
        return (choice == 0) ? ITEM_SWORD : ITEM_BOMB;
    if (stage == 2)
        return (choice == 0) ? ITEM_ARMOR : ITEM_COIN;
    return ITEM_POTION;
}

void BossAttack()
{
    bossIsAttacking = true;
    bossAttackTimer = BOSS_ATTACK_DURATION;
    bossAttackFrame = 0;
    bossAttackFrameTime = 0.0f;

    if (bossAttackFrameCount > 0)
        bossAttackFrameDuration = BOSS_ATTACK_DURATION / (float)bossAttackFrameCount;

    int min = boss.attack - 5;
    int max = boss.attack + 5;

    if (playerHasArmor)
    {
        min = (boss.attack - 5) / 2;
        max = (boss.attack + 5) / 2;
    }
    if (min < 1)
        min = 1;

    int damage = min + (rand() % (max - min + 1));
    player.hp -= damage;
    if (player.hp < 0)
        player.hp = 0;

    if (playerHasArmor)
        sprintf(messageBuffer, "Chefe ataca com armadura ativa! Voce levou %d de dano.", damage);
    else
        sprintf(messageBuffer, "Chefe ataca! Voce levou %d de dano!", damage);

    battleMessage = messageBuffer;
    playerHurtTimer = PLAYER_HURT_DURATION;
}

void UseItem(int index)
{
    if (itemUsed[index])
    {
        battleMessage = "Este item ja foi usado!";
        return;
    }

    ItemType item = inventory[index];
    switch (item)
    {
    case ITEM_POTION:
        player.hp += 50;
        if (player.hp > player.maxHp)
            player.hp = player.maxHp;
        battleMessage = "Voce usou Pocao! Curou 50 HP!";
        itemUsed[index] = true;
        break;
    case ITEM_SWORD:
        battleMessage = "Espada: aumenta seu dano. Use ATACAR [A].";
        return;
    case ITEM_BOMB:
    {
        int dmg = 60 + (rand() % 31);
        boss.hp -= dmg;
        sprintf(messageBuffer, "Voce usou Bomba! Causou %d de dano!", dmg);
        battleMessage = messageBuffer;
        itemUsed[index] = true;
        break;
    }
    case ITEM_COIN:
        itemUsed[index] = true;
        if (rand() % 2 == 0)
        {
            battleMessage = "Voce usou Moeda! Distraiu o chefe e fugiu!";
            currentState = GAME_STATE_ENDING_ESCAPE;
            return;
        }
        else
        {
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

    if (boss.hp < 0)
        boss.hp = 0;
    battleState = BATTLE_BOSS_TURN;
    bossTurnTimer = 1.5f;
}

bool PlayerHasSword(void)
{
    for (int i = 0; i < INVENTORY_SIZE; i++)
    {
        if (inventory[i] == ITEM_SWORD)
            return true;
    }
    return false;
}

void PlayerAttack(void)
{
    int damage;
    if (PlayerHasSword())
    {
        damage = 20 + (rand() % 21);
        sprintf(messageBuffer, "Voce atacou com a espada! Causou %d de dano!", damage);
    }
    else
    {
        damage = 15 + (rand() % 8);
        sprintf(messageBuffer, "Voce atacou desarmado! Causou %d de dano!", damage);
    }
    boss.hp -= damage;
    if (boss.hp < 0)
        boss.hp = 0;
    battleMessage = messageBuffer;

    playerIsAttacking = true;
    playerAttackTimer = PLAYER_ATTACK_DURATION;
    playerAttackFrame = 0;
    playerAttackFrameTime = 0.0f;
    bossHurtTimer = BOSS_HURT_DURATION;

    battleState = BATTLE_BOSS_TURN;
    bossTurnTimer = 1.1f;
}

Texture2D LoadAsset(const char *baseName)
{
    char path[128];
    sprintf(path, "assets/%s.png", baseName);
    if (FileExists(path))
        return LoadTexture(path);

    sprintf(path, "assets/%s.jpg", baseName);
    if (FileExists(path))
        return LoadTexture(path);

    sprintf(path, "assets/%s.bmp", baseName);
    if (FileExists(path))
        return LoadTexture(path);

    return (Texture2D){0};
}

void InitGame(void)
{
    currentState = GAME_STATE_TITLE;
    currentStage = 0;
    inventoryCount = 0;
    itemMessageTimer = 0.0f;
    lastItemCollected = ITEM_NONE;

    for (int i = 0; i < INVENTORY_SIZE; i++)
    {
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

    explorePlayerX = SCREEN_WIDTH / 2 - 10;
    explorePlayerY = 420;
    explorePlayerSpeed = 250.0f;
    doorLeftRect = (Rectangle){100, 240, 150, 220};
    doorRightRect = (Rectangle){SCREEN_WIDTH - 250, 240, 150, 220};

    if (!texturesInitialized)
    {
        playerTexture = LoadAsset("boss_player/player");
        bossTexture = LoadAsset("boss_player/boss");
        titleBackgroundTexture = LoadAsset("cenarios/title_bg");
        battleBackgroundTexture = LoadAsset("cenarios/battle_bg");

        bgStage1 = LoadAsset("cenarios/cenario1");
        bgStage2 = LoadAsset("cenarios/cenario2");
        bgStage3 = LoadAsset("cenarios/cenario3");
        bgStage4 = LoadAsset("cenarios/cenario4");

        bossAttackTexture = LoadAsset("boss_attack");
        playerHitTexture = LoadAsset("player_hit");
        bossHitTexture = LoadAsset("boss_hit");
        playerAttackTexture = LoadAsset("player_attack");

        bossAttackFrameCount = 1;
        bossAttackFrameDuration = BOSS_ATTACK_DURATION;
        if (bossAttackTexture.id != 0 && bossAttackTexture.height > 0)
        {
            bossAttackFrameCount = bossAttackTexture.width / bossAttackTexture.height;
            if (bossAttackFrameCount < 1)
                bossAttackFrameCount = 1;
            bossAttackFrameDuration = BOSS_ATTACK_DURATION / (float)bossAttackFrameCount;
        }

        playerAttackFrameCount = 1;
        playerAttackFrameDuration = PLAYER_ATTACK_DURATION;
        if (playerAttackTexture.id != 0 && playerAttackTexture.height > 0)
        {
            playerAttackFrameCount = playerAttackTexture.width / playerAttackTexture.height;
            if (playerAttackFrameCount < 1)
                playerAttackFrameCount = 1;
            playerAttackFrameDuration = PLAYER_ATTACK_DURATION / (float)playerAttackFrameCount;
        }

        texturesInitialized = true;
    }

    bossIsAttacking = false;
    bossAttackTimer = 0.0f;
    bossHurtTimer = 0.0f;
}

void UpdateExplore(void)
{
    if (itemMessageTimer > 0)
    {
        itemMessageTimer -= GetFrameTime();
        if (itemMessageTimer <= 0 && currentStage >= 4)
        {
            currentState = GAME_STATE_BATTLE;
            battleState = BATTLE_PLAYER_TURN;
        }
        return;
    }

    float delta = GetFrameTime();
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
        explorePlayerX += explorePlayerSpeed * delta;
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))
        explorePlayerX -= explorePlayerSpeed * delta;

    if (explorePlayerX < 60)
        explorePlayerX = 60;
    if (explorePlayerX > SCREEN_WIDTH - 80)
        explorePlayerX = SCREEN_WIDTH - 80;

    if (IsKeyPressed(KEY_ENTER))
    {
        Rectangle playerRect = (Rectangle){explorePlayerX - 5, explorePlayerY - 10, 30, 60};
        int chosen = -1;
        if (CheckCollisionRecs(playerRect, doorLeftRect))
            chosen = 0;
        if (CheckCollisionRecs(playerRect, doorRightRect))
            chosen = 1;

        if (chosen != -1)
        {
            lastItemCollected = GetItemForChoice(currentStage, chosen);
            if (inventoryCount < INVENTORY_SIZE)
            {
                inventory[inventoryCount] = lastItemCollected;
                inventoryCount++;
            }
            currentStage++;
            itemMessageTimer = 2.0f;
            explorePlayerX = SCREEN_WIDTH / 2 - 10;
        }
    }
}

void UpdateBattle(void)
{
    float delta = GetFrameTime();

    if (playerIsAttacking)
    {
        playerAttackTimer -= delta;
        if (playerAttackTimer <= 0.0f)
            playerIsAttacking = false;

        if (playerAttackFrameCount > 1)
        {
            playerAttackFrameTime += delta;
            if (playerAttackFrameTime >= playerAttackFrameDuration)
            {
                playerAttackFrameTime -= playerAttackFrameDuration;
                playerAttackFrame = (playerAttackFrame + 1) % playerAttackFrameCount;
            }
        }
    }

    if (bossIsAttacking)
    {
        bossAttackTimer -= delta;
        if (bossAttackTimer <= 0.0f)
            bossIsAttacking = false;

        if (bossAttackFrameCount > 1)
        {
            bossAttackFrameTime += delta;
            if (bossAttackFrameTime >= bossAttackFrameDuration)
            {
                bossAttackFrameTime -= bossAttackFrameDuration;
                bossAttackFrame = (bossAttackFrame + 1) % bossAttackFrameCount;
            }
        }
    }

    if (bossHurtTimer > 0.0f)
        bossHurtTimer -= delta;
    if (playerHurtTimer > 0.0f)
        playerHurtTimer -= delta;

    if (boss.hp <= 0)
    {
        currentState = GAME_STATE_ENDING_GOOD;
        return;
    }
    if (player.hp <= 0)
    {
        currentState = GAME_STATE_ENDING_BAD;
        return;
    }

    if (battleState == BATTLE_PLAYER_TURN)
    {
        if (IsKeyPressed(KEY_RIGHT))
            selectedItemIndex = (selectedItemIndex + 1) % INVENTORY_SIZE;
        if (IsKeyPressed(KEY_LEFT))
            selectedItemIndex = (selectedItemIndex - 1 + INVENTORY_SIZE) % INVENTORY_SIZE;
        if (IsKeyPressed(KEY_A))
            PlayerAttack();
        if (IsKeyPressed(KEY_ENTER))
            UseItem(selectedItemIndex);
    }
    else if (battleState == BATTLE_BOSS_TURN)
    {
        bossTurnTimer -= GetFrameTime();
        if (bossTurnTimer <= 0)
        {
            BossAttack();
            battleState = BATTLE_PLAYER_TURN;
        }
    }
}

void UpdateTitleScreen(void)
{
    if (IsKeyPressed(KEY_ENTER))
    {
        currentState = GAME_STATE_EXPLORE;
    }
}

void DrawPlayerSprite(int posX, int posY)
{
    DrawCircle(posX + 10, posY - 10, 10, (Color){255, 200, 150, 255});
    DrawRectangle(posX, posY, 20, 40, (Color){0, 150, 255, 255});
    DrawRectangleLines(posX, posY, 20, 40, (Color){0, 100, 200, 255});
}

void DrawBossSprite(int posX, int posY)
{
    DrawCircle(posX + 20, posY + 30, 50, (Color){100, 0, 0, 100});
    DrawRectangle(posX, posY, 40, 60, (Color){200, 0, 0, 255});
    DrawRectangleLines(posX, posY, 40, 60, (Color){100, 0, 0, 255});
}

void DrawExplore(void)
{
    Texture2D currentBg = {0};

    if (currentStage == 0)
        currentBg = bgStage1;
    else if (currentStage == 1)
        currentBg = bgStage2;
    else if (currentStage == 2)
        currentBg = bgStage3;
    else if (currentStage == 3)
        currentBg = bgStage4;

    if (currentBg.id != 0)
    {
        DrawTexturePro(currentBg,
                       (Rectangle){0, 0, (float)currentBg.width, (float)currentBg.height},
                       (Rectangle){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT},
                       (Vector2){0, 0}, 0.0f, WHITE);
    }
    else
    {
        ClearBackground((Color){20, 20, 40, 255});
        DrawRectangle(60, 120, SCREEN_WIDTH - 120, 420, (Color){30, 40, 70, 255});
        DrawRectangleLines(60, 120, SCREEN_WIDTH - 120, 420, (Color){100, 150, 200, 255});
    }

    if (itemMessageTimer > 0)
    {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){0, 0, 0, 200});
        DrawRectangle(SCREEN_WIDTH / 2 - 300, SCREEN_HEIGHT / 2 - 100, 600, 200, (Color){50, 50, 100, 255});
        DrawRectangleLines(SCREEN_WIDTH / 2 - 300, SCREEN_HEIGHT / 2 - 100, 600, 200, (Color){100, 200, 255, 255});

        sprintf(messageBuffer, "Voce coletou: %s!", GetItemName(lastItemCollected));
        int textWidth = MeasureText(messageBuffer, 30);
        DrawText(messageBuffer, SCREEN_WIDTH / 2 - textWidth / 2, SCREEN_HEIGHT / 2 - 50, 30, (Color){100, 255, 150, 255});
        DrawText("Carregando proximo cenario...", SCREEN_WIDTH / 2 - 120, SCREEN_HEIGHT / 2 + 40, 18, (Color){150, 150, 200, 255});
        return;
    }

    const char *storyText = "";
    const char *leftDoorLabel = "Porta A";
    const char *rightDoorLabel = "Porta B";

    switch (currentStage)
    {
    case 0:
        storyText = "Voce chega aos portoes do Castelo exausto.\nPrecisa recuperar as forcas antes de entrar.";
        leftDoorLabel = "Beber da Fonte";
        rightDoorLabel = "Comer Frutas";
        break;
    case 1:
        storyText = "No arsenal abandonado, voce ve duas armas.\nQual estilo de combate voce prefere?";
        leftDoorLabel = "Espada Antiga";
        rightDoorLabel = "Bomba Caseira";
        break;
    case 2:
        storyText = "Um esqueleto segura dois itens valiosos.\nVoce prioriza protecao ou tenta subornar o chefe?";
        leftDoorLabel = "Armadura Leve";
        rightDoorLabel = "Bolsa de Ouro";
        break;
    case 3:
        storyText = "A porta do trono esta a frente. O medo gela a espinha.\nUltima chance de curar ferimentos.";
        leftDoorLabel = "Usar Curativos";
        rightDoorLabel = "Tonico Vital";
        break;
    }

    DrawText(storyText, 82, 62, 22, BLACK);
    DrawText(storyText, 80, 60, 22, WHITE);

    DrawRectangleRec(doorLeftRect, (Color){255, 255, 255, 30});
    DrawRectangleLines((int)doorLeftRect.x, (int)doorLeftRect.y, (int)doorLeftRect.width, (int)doorLeftRect.height, YELLOW);

    DrawText(leftDoorLabel, (int)doorLeftRect.x + 12, (int)doorLeftRect.y + 92, 16, BLACK);
    DrawText(leftDoorLabel, (int)doorLeftRect.x + 10, (int)doorLeftRect.y + 90, 16, WHITE);

    DrawRectangleRec(doorRightRect, (Color){255, 255, 255, 30});
    DrawRectangleLines((int)doorRightRect.x, (int)doorRightRect.y, (int)doorRightRect.width, (int)doorRightRect.height, YELLOW);

    DrawText(rightDoorLabel, (int)doorRightRect.x + 12, (int)doorRightRect.y + 92, 16, BLACK);
    DrawText(rightDoorLabel, (int)doorRightRect.x + 10, (int)doorRightRect.y + 90, 16, WHITE);

    if (texturesInitialized && playerTexture.id != 0)
    {
        Rectangle src = {0, 0, (float)playerTexture.width, (float)playerTexture.height};
        Vector2 origin = {playerTexture.width / 2.0f, playerTexture.height / 2.0f};
        Rectangle dest = {explorePlayerX + 10, explorePlayerY + playerTexture.height / 2.0f, (float)playerTexture.width, (float)playerTexture.height};
        DrawTexturePro(playerTexture, src, dest, origin, 0.0f, WHITE);
    }
    else
    {
        DrawPlayerSprite((int)explorePlayerX, (int)explorePlayerY);
    }

    DrawText("Use SETAS ou A/D e [ENTER] na porta.", 82, SCREEN_HEIGHT - 38, 16, BLACK);
    DrawText("Use SETAS ou A/D e [ENTER] na porta.", 80, SCREEN_HEIGHT - 40, 16, WHITE);
}

void DrawBattle(void)
{
    if (battleBackgroundTexture.id != 0)
    {
        DrawTexturePro(battleBackgroundTexture, (Rectangle){0, 0, (float)battleBackgroundTexture.width, (float)battleBackgroundTexture.height},
                       (Rectangle){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}, (Vector2){0, 0}, 0.0f, WHITE);
    }
    else
    {
        ClearBackground((Color){30, 30, 50, 255});
    }

    const int BAR_MARGIN = 20;
    const int BAR_W = 300;
    const int BAR_H = 25;

    DrawText("Player (voce)", BAR_MARGIN, BAR_MARGIN, 22, (Color){150, 200, 255, 255});
    DrawRectangle(BAR_MARGIN, BAR_MARGIN + 35, BAR_W, BAR_H, (Color){50, 50, 80, 255});
    DrawRectangle(BAR_MARGIN, BAR_MARGIN + 35, (int)(BAR_W * ((float)player.hp / player.maxHp)), BAR_H, (Color){50, 200, 100, 255});
    DrawRectangleLines(BAR_MARGIN, BAR_MARGIN + 35, BAR_W, BAR_H, WHITE);
    DrawText(TextFormat("HP: %d / %d", player.hp, player.maxHp), BAR_MARGIN + 80, BAR_MARGIN + 37, 20, WHITE);

    int bossBarX = SCREEN_WIDTH - BAR_W - BAR_MARGIN;
    DrawText("Boss", bossBarX, BAR_MARGIN, 22, (Color){255, 100, 100, 255});
    DrawRectangle(bossBarX, BAR_MARGIN + 35, BAR_W, BAR_H, (Color){50, 50, 80, 255});
    DrawRectangle(bossBarX, BAR_MARGIN + 35, (int)(BAR_W * ((float)boss.hp / boss.maxHp)), BAR_H, (Color){255, 50, 50, 255});
    DrawRectangleLines(bossBarX, BAR_MARGIN + 35, BAR_W, BAR_H, WHITE);
    DrawText(TextFormat("HP: %d / %d", boss.hp, boss.maxHp), bossBarX + 80, BAR_MARGIN + 37, 20, WHITE);

    const float GROUND_Y = 480.0f;
    Vector2 posB = {SCREEN_WIDTH - 250.0f, GROUND_Y};
    float bossOffX = 0;
    if (bossIsAttacking)
        bossOffX = -((posB.x - 550.0f) * sinf((1.0f - bossAttackTimer / BOSS_ATTACK_DURATION) * 3.14f));

    float bossAlpha = (bossHurtTimer > 0 && ((int)(bossHurtTimer * 30) % 2 == 0)) ? 0.5f : 1.0f;
    Color bossTint = Fade(WHITE, bossAlpha);

    if (texturesInitialized && bossTexture.id != 0)
    {
        Texture2D tex = bossTexture;
        if (bossIsAttacking && bossAttackTexture.id != 0)
            tex = bossAttackTexture;

        float w = (float)tex.width;
        if (bossIsAttacking && bossAttackFrameCount > 1)
            w /= bossAttackFrameCount;

        Rectangle src = {bossIsAttacking ? bossAttackFrame * w : 0, 0, w, (float)tex.height};
        Vector2 origin = {w / 2, tex.height / 2.0f};
        DrawTexturePro(tex, src, (Rectangle){posB.x + bossOffX, posB.y - tex.height / 2.0f, w, (float)tex.height}, origin, 0, bossTint);

        if (bossHurtTimer > 0 && bossHitTexture.id != 0)
            DrawTexturePro(bossHitTexture, (Rectangle){0, 0, (float)bossHitTexture.width, (float)bossHitTexture.height},
                           (Rectangle){posB.x + bossOffX, posB.y - tex.height / 2.0f, (float)bossHitTexture.width, (float)bossHitTexture.height}, origin, 0, WHITE);
    }
    else
    {
        DrawBossSprite((int)(posB.x + bossOffX) - 20, (int)GROUND_Y - 120);
    }

    int baseX = 250;
    float atkOffX = 0;
    if (playerIsAttacking)
        atkOffX = (450.0f - baseX) * sinf((1.0f - playerAttackTimer / PLAYER_ATTACK_DURATION) * 3.14f);

    float playerAlpha = (playerHurtTimer > 0 && ((int)(playerHurtTimer * 30) % 2 == 0)) ? 0.5f : 1.0f;
    Color playerTint = Fade(WHITE, playerAlpha);

    if (texturesInitialized && playerTexture.id != 0)
    {
        Texture2D tex = playerTexture;
        if (playerIsAttacking && playerAttackTexture.id != 0)
            tex = playerAttackTexture;

        float w = (float)tex.width;
        if (playerIsAttacking && playerAttackFrameCount > 1)
            w /= playerAttackFrameCount;

        Rectangle src = {playerIsAttacking ? playerAttackFrame * w : 0, 0, w, (float)tex.height};
        Vector2 origin = {w / 2, tex.height / 2.0f};
        DrawTexturePro(tex, src, (Rectangle){baseX + atkOffX, GROUND_Y - tex.height / 2.0f, w, (float)tex.height}, origin, 0, playerTint);

        if (playerHurtTimer > 0 && playerHitTexture.id != 0)
            DrawTexturePro(playerHitTexture, (Rectangle){0, 0, (float)playerHitTexture.width, (float)playerHitTexture.height},
                           (Rectangle){baseX + atkOffX, GROUND_Y - tex.height / 2.0f, (float)playerHitTexture.width, (float)playerHitTexture.height}, origin, 0, WHITE);
    }
    else
    {
        DrawPlayerSprite(baseX + (int)atkOffX - 10, (int)GROUND_Y - 60);
    }

    int itemPosX = 20;
    for (int i = 0; i < INVENTORY_SIZE; i++)
    {
        Color bgColor = (i == selectedItemIndex) ? (Color){100, 200, 255, 255} : (Color){50, 50, 100, 255};
        DrawRectangle(itemPosX, SCREEN_HEIGHT - 70, 180, 40, bgColor);
        DrawRectangleLines(itemPosX, SCREEN_HEIGHT - 70, 180, 40, LIGHTGRAY);

        Color txtColor = itemUsed[i] ? GRAY : (i == selectedItemIndex ? BLACK : WHITE);
        const char *name = (inventory[i] == ITEM_SWORD || inventory[i] == ITEM_ARMOR) ? "Vazio" : GetItemName(inventory[i]);
        DrawText(name, itemPosX + 5, SCREEN_HEIGHT - 58, 12, txtColor);
        itemPosX += 200;
    }

    DrawRectangle(SCREEN_WIDTH - 150, SCREEN_HEIGHT - 85, 130, 45, RED);
    DrawRectangleLines(SCREEN_WIDTH - 150, SCREEN_HEIGHT - 85, 130, 45, MAROON);
    DrawText("ATACAR [A]", SCREEN_WIDTH - 140, SCREEN_HEIGHT - 75, 16, WHITE);

    if (battleMessage)
    {
        DrawText(battleMessage, SCREEN_WIDTH / 2 - MeasureText(battleMessage, 20) / 2, 100, 20, YELLOW);
    }
}

void DrawEnding(bool playerWon)
{
    ClearBackground((Color){20, 20, 40, 255});
    DrawRectangleLines(SCREEN_WIDTH / 2 - 400, SCREEN_HEIGHT / 2 - 200, 800, 400, BLUE);

    const char *title = playerWon ? "VITORIA!" : "GAME OVER";
    Color col = playerWon ? GREEN : RED;
    DrawText(title, SCREEN_WIDTH / 2 - MeasureText(title, 60) / 2, 200, 60, col);
    DrawText("Pressione [ENTER] para jogar novamente.", 200, 500, 20, WHITE);
}

void DrawEscapeEnding(void)
{
    ClearBackground((Color){20, 20, 40, 255});
    DrawText("FUGA!", SCREEN_WIDTH / 2 - MeasureText("FUGA!", 60) / 2, 200, 60, ORANGE);
    DrawText("Voce fugiu com sucesso.", SCREEN_WIDTH / 2 - 100, 300, 20, WHITE);
    DrawText("Pressione [ENTER] para jogar novamente.", 200, 500, 20, WHITE);
}

void DrawTitleScreen(void)
{
    if (titleBackgroundTexture.id != 0)
    {
        DrawTexturePro(titleBackgroundTexture, (Rectangle){0, 0, (float)titleBackgroundTexture.width, (float)titleBackgroundTexture.height},
                       (Rectangle){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}, (Vector2){0, 0}, 0.0f, WHITE);
    }
    else
    {
        ClearBackground((Color){10, 10, 30, 255});
    }

    DrawText("Rush RPG", SCREEN_WIDTH / 2 - MeasureText("RushRPG", 80) / 2, 100, 80, GOLD);

    if (((int)(GetTime() * 2) % 2) == 0)
    {
        DrawText("Pressione [ENTER] para comecar", SCREEN_WIDTH / 2 - MeasureText("Pressione [ENTER] para comecar", 30) / 2, SCREEN_HEIGHT - 100, 30, GREEN);
    }

    const char *students[] = {"Lucas Del Pozo", "Lucas Sassi de Souza", "Eduardo Parize", "Vinicius Ribas Bida"};
    int namesY = SCREEN_HEIGHT - 180;
    DrawRectangle(SCREEN_WIDTH - 250, namesY - 10, 240, 150, (Color){0, 0, 0, 150});
    DrawText("Desenvolvido por:", SCREEN_WIDTH - 240, namesY, 12, WHITE);
    for (int i = 0; i < 4; i++)
        DrawText(students[i], SCREEN_WIDTH - 240, namesY + 30 + (i * 20), 10, LIGHTGRAY);
}

int main(void)
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Raylib RPG de Turnos");
    InitGame();
    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        switch (currentState)
        {
        case GAME_STATE_TITLE:
            UpdateTitleScreen();
            break;
        case GAME_STATE_EXPLORE:
            UpdateExplore();
            break;
        case GAME_STATE_BATTLE:
            UpdateBattle();
            break;
        case GAME_STATE_ENDING_GOOD:
        case GAME_STATE_ENDING_BAD:
        case GAME_STATE_ENDING_ESCAPE:
            if (IsKeyPressed(KEY_ENTER))
                InitGame();
            break;
        }

        BeginDrawing();
        switch (currentState)
        {
        case GAME_STATE_TITLE:
            DrawTitleScreen();
            break;
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

    if (playerTexture.id)
        UnloadTexture(playerTexture);
    if (bossTexture.id)
        UnloadTexture(bossTexture);
    if (titleBackgroundTexture.id)
        UnloadTexture(titleBackgroundTexture);
    if (battleBackgroundTexture.id)
        UnloadTexture(battleBackgroundTexture);
    if (bossAttackTexture.id)
        UnloadTexture(bossAttackTexture);
    if (playerAttackTexture.id)
        UnloadTexture(playerAttackTexture);

    if (bgStage1.id)
        UnloadTexture(bgStage1);
    if (bgStage2.id)
        UnloadTexture(bgStage2);
    if (bgStage3.id)
        UnloadTexture(bgStage3);
    if (bgStage4.id)
        UnloadTexture(bgStage4);

    CloseWindow();
    return 0;
}