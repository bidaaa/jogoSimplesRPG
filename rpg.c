#include "raylib.h"
#include <stdio.h>  // Para sprintf (formatar texto)
#include <stdbool.h> // Para bool, true, false

//------------------------------------------------------------------------------------
// Constantes e Definições de Tela
//------------------------------------------------------------------------------------
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

//------------------------------------------------------------------------------------
// Estruturas de Dados do Jogo
//------------------------------------------------------------------------------------

// Estados principais do jogo
typedef enum {
    GAME_STATE_EXPLORE,
    GAME_STATE_BATTLE,
    GAME_STATE_ENDING_GOOD,
    GAME_STATE_ENDING_BAD
} GameState;

// Itens que o jogador pode coletar
typedef enum {
    ITEM_NONE,
    ITEM_POTION,      // Cura o jogador (Uso único)
    ITEM_SWORD,       // Causa dano médio ao chefe (Reutilizável)
    ITEM_BOMB,        // Causa dano alto ao chefe (Uso único)
    ITEM_SHIELD       // Bloqueia o próximo ataque do chefe (Reutilizável)
} ItemType;

// Estrutura do Jogador
typedef struct {
    int hp;
    int maxHp;
} Player;

// Estrutura do Chefe
typedef struct {
    int hp;
    int maxHp;
    int attack;
} Boss;

// Estados da batalha
typedef enum {
    BATTLE_PLAYER_TURN,
    BATTLE_BOSS_TURN,
} BattleState;

//------------------------------------------------------------------------------------
// Variáveis Globais do Jogo
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
static bool playerIsShielded;
static float bossTurnTimer; // Um pequeno delay para o turno do chefe

// Variáveis para a mensagem de coleta de item
static float itemMessageTimer = 0.0f;
static ItemType lastItemCollected = ITEM_NONE;

//------------------------------------------------------------------------------------
// Funções Auxiliares
//------------------------------------------------------------------------------------

// Retorna o nome do item para exibição
const char* GetItemName(ItemType item) {
    switch (item) {
        case ITEM_POTION: return "Pocao (Cura 50 HP)";
        case ITEM_SWORD:  return "Espada (Dano 30)";
        case ITEM_BOMB:   return "Bomba (Dano 70)";
        case ITEM_SHIELD: return "Escudo (Bloqueia)";
        default:          return "Vazio";
    }
}

// Define qual item o jogador recebe com base no estágio e na escolha (0 para esquerda/cima, 1 para direita/baixo)
ItemType GetItemForChoice(int stage, int choice) {
    if (stage == 0) {
        return (choice == 0) ? ITEM_POTION : ITEM_SHIELD;
    }
    if (stage == 1) {
        return (choice == 0) ? ITEM_SWORD : ITEM_BOMB;
    }
    if (stage == 2) {
        return (choice == 0) ? ITEM_POTION : ITEM_SWORD;
    }
    if (stage == 3) {
        return (choice == 0) ? ITEM_SHIELD : ITEM_BOMB;
    }
    return ITEM_NONE;
}

// Ação do chefe
void BossAttack() {
    if (playerIsShielded) {
        playerIsShielded = false;
        battleMessage = "Chefe ataca! Voce bloqueou com o Escudo!";
    } else {
        int damage = boss.attack;
        player.hp -= damage;
        if (player.hp < 0) player.hp = 0;
        
        sprintf(messageBuffer, "Chefe ataca! Voce levou %d de dano!", damage);
        battleMessage = messageBuffer;
    }
}

// Ação do jogador (Usar item)
void UseItem(int index) {
    if (itemUsed[index]) {
        battleMessage = "Este item ja foi usado!";
        return; // Não gasta o turno
    }

    ItemType item = inventory[index];

    switch (item) {
        case ITEM_POTION:
            player.hp += 50;
            if (player.hp > player.maxHp) player.hp = player.maxHp;
            battleMessage = "Voce usou Pocao! Curou 50 HP!";
            itemUsed[index] = true; // <-- Marcado como usado
            break;
        case ITEM_SWORD:
            boss.hp -= 30;
            battleMessage = "Voce usou Espada! Causou 30 de dano!";
            // NÂO marcar como usado
            break;
        case ITEM_BOMB:
            boss.hp -= 70;
            battleMessage = "Voce usou Bomba! Causou 70 de dano!";
            itemUsed[index] = true; // <-- Marcado como usado
            break;
        case ITEM_SHIELD:
            playerIsShielded = true;
            battleMessage = "Voce usou Escudo! Proximo ataque sera bloqueado.";
            // NÂO marcar como usado
            break;
        default:
            battleMessage = "Item invalido?";
            break;
    }
    
    if (boss.hp < 0) boss.hp = 0;

    // Prepara o turno do chefe
    battleState = BATTLE_BOSS_TURN;
    bossTurnTimer = 1.5f; // Delay de 1.5 segundos
}

// Inicializa/Reinicia as variáveis do jogo
void InitGame(void) {
    currentState = GAME_STATE_EXPLORE;
    currentStage = 0;
    inventoryCount = 0;
    itemMessageTimer = 0.0f;
    lastItemCollected = ITEM_NONE;

    // Resetar inventário
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        inventory[i] = ITEM_NONE;
        itemUsed[i] = false;
    }

    // Status do Jogador
    player.hp = 100;
    player.maxHp = 100;

    // Status do Chefe
    boss.hp = 200;
    boss.maxHp = 200;
    boss.attack = 25; // Dano do chefe

    // Status da Batalha
    battleState = BATTLE_PLAYER_TURN;
    selectedItemIndex = 0;
    playerIsShielded = false;
    battleMessage = "Batalha contra o Chefe! Escolha seu item.";
}

//------------------------------------------------------------------------------------
// Funções de Update (Lógica)
//------------------------------------------------------------------------------------

// Lógica da Exploração
void UpdateExplore(void) {
    // Se o timer da mensagem de item estiver ativo, apenas o diminua.
    if (itemMessageTimer > 0) {
        itemMessageTimer -= GetFrameTime();
        
        // Se o timer acabou E chegamos ao fim da exploração, vá para a batalha
        if (itemMessageTimer <= 0 && currentStage >= 4) {
            currentState = GAME_STATE_BATTLE;
            battleState = BATTLE_PLAYER_TURN;
        }
        return; // Não processe input enquanto a mensagem estiver na tela
    }

    // Timer zerado, processar input
    int choice = -1; // -1 = sem escolha, 0 = escolha 1, 1 = escolha 2

    if (IsKeyPressed(KEY_ONE)) {
        choice = 0;
    }
    if (IsKeyPressed(KEY_TWO)) {
        choice = 1;
    }

    if (choice != -1) {
        // Adiciona o item ao inventário
        lastItemCollected = GetItemForChoice(currentStage, choice); // Salva o item
        inventory[inventoryCount] = lastItemCollected;
        inventoryCount++;
        
        // Avança para o próximo estágio
        currentStage++;
        
        // Ativa o timer da mensagem
        itemMessageTimer = 2.0f; // 2 segundos
    }
}

// Lógica da Batalha
void UpdateBattle(void) {
    // Checagem de vitória/derrota (acontece antes de qualquer turno)
    if (boss.hp <= 0) {
        currentState = GAME_STATE_ENDING_GOOD;
        return;
    }
    if (player.hp <= 0) {
        currentState = GAME_STATE_ENDING_BAD;
        return;
    }

    // Turno do Jogador
    if (battleState == BATTLE_PLAYER_TURN) {
        if (IsKeyPressed(KEY_RIGHT)) {
            selectedItemIndex = (selectedItemIndex + 1) % INVENTORY_SIZE;
        }
        if (IsKeyPressed(KEY_LEFT)) {
            selectedItemIndex = (selectedItemIndex - 1 + INVENTORY_SIZE) % INVENTORY_SIZE;
        }

        if (IsKeyPressed(KEY_ENTER)) {
            UseItem(selectedItemIndex);
        }
    }
    // Turno do Chefe (com delay)
    else if (battleState == BATTLE_BOSS_TURN) {
        bossTurnTimer -= GetFrameTime();
        if (bossTurnTimer <= 0) {
            BossAttack();
            battleState = BATTLE_PLAYER_TURN; // Devolve o turno ao jogador
        }
    }
}

// Lógica da Tela Final
void UpdateEnding(void) {
    // Reinicia o jogo
    if (IsKeyPressed(KEY_ENTER)) {
        InitGame();
    }
}

//------------------------------------------------------------------------------------
// Funções de Draw (Gráficos)
//------------------------------------------------------------------------------------

// NOVOS: Funções para desenhar os "sprites"
void DrawPlayerSprite(int posX, int posY) {
    // Cabeça
    DrawCircle(posX + 10, posY - 10, 10, WHITE);
    // Torso
    DrawRectangle(posX, posY, 20, 40, BLUE);
    // Braços
    DrawRectangle(posX - 5, posY + 5, 5, 20, BLUE);
    DrawRectangle(posX + 20, posY + 5, 5, 20, BLUE);
    // Pernas
    DrawRectangle(posX + 2, posY + 40, 6, 20, BLUE);
    DrawRectangle(posX + 12, posY + 40, 6, 20, BLUE);
}

void DrawBossSprite(int posX, int posY) {
    // Cabeça
    DrawCircle(posX + 20, posY - 15, 15, DARKGRAY);
    // Torso (maior)
    DrawRectangle(posX, posY, 40, 60, RED);
    // Braços
    DrawRectangle(posX - 10, posY + 10, 10, 30, RED);
    DrawRectangle(posX + 40, posY + 10, 10, 30, RED);
    // Pernas
    DrawRectangle(posX + 5, posY + 60, 10, 30, RED);
    DrawRectangle(posX + 25, posY + 60, 10, 30, RED);
}


// Desenha a UI de Exploração
void DrawExplore(void) {
    ClearBackground(BLACK);

    // --- NOVO: Checar se a mensagem de item deve aparecer ---
    if (itemMessageTimer > 0) {
        // Limpa a tela e mostra a mensagem de coleta
        sprintf(messageBuffer, "Voce coletou: %s!", GetItemName(lastItemCollected));
        
        int textWidth = MeasureText(messageBuffer, 30);
        DrawText(messageBuffer, SCREEN_WIDTH / 2 - textWidth / 2, SCREEN_HEIGHT / 2 - 40, 30, GREEN);
        
        const char* msg2 = "Carregando proximo cenario...";
        int textWidth2 = MeasureText(msg2, 20);
        DrawText(msg2, SCREEN_WIDTH / 2 - textWidth2 / 2, SCREEN_HEIGHT / 2 + 20, 20, GRAY);
        
        return; // Pula o desenho normal da exploração
    }
    
    // Textos variam por estágio
    const char* storyText = "";
    const char* choice1Text = "";
    const char* choice2Text = "";

    switch (currentStage) {
        case 0:
            storyText = "Voce esta na entrada da masmorra. O caminho se bifurca.";
            choice1Text = "1. Seguir pela caverna umida.";
            choice2Text = "2. Atravessar a ponte velha.";
            break;
        case 1:
            storyText = "Mais a fundo, voce encontra um salao com dois pedestais.";
            choice1Text = "1. Tocar no pedestal da chama.";
            choice2Text = "2. Tocar no pedestal da sombra.";
            break;
        case 2:
            storyText = "Um guarda fantasma bloqueia o caminho. Voce precisa distrai-lo.";
            choice1Text = "1. Jogar uma pedra no corredor leste.";
            choice2Text = "2. Usar o sino quebrado no corredor oeste.";
            break;
        case 3:
            storyText = "A porta do chefe esta a sua frente. Duas alavancas a guardam.";
            choice1Text = "1. Puxar a alavanca de Prata.";
            choice2Text = "2. Puxar a alavanca de Bronze.";
            break;
    }

    DrawText(storyText, 40, 150, 20, RAYWHITE);
    DrawText(choice1Text, 60, 250, 20, YELLOW);
    DrawText(choice2Text, 60, 300, 20, YELLOW);
    
    DrawText("Pressione [1] ou [2] para escolher...", 40, 450, 20, GRAY);
}

// Desenha a UI de Batalha
void DrawBattle(void) {
    // Buffers locais para guardar o texto formatado do HP
    char bossHpText[64];
    char playerHpText[64];
    
    ClearBackground(DARKGRAY);

    // --- Desenhar Chefe ---
    // --- ATUALIZADO: Usando a função de sprite ---
    DrawBossSprite(SCREEN_WIDTH / 2 - 20, 50); // Posição (x, y) do torso
    //DrawText("CHEFE", SCREEN_WIDTH / 2 - 30, 95, 20, WHITE); // Opcional
    
    // HP do Chefe
    DrawRectangle(SCREEN_WIDTH / 2 - 150, 140, 300, 20, LIGHTGRAY);
    DrawRectangle(SCREEN_WIDTH / 2 - 150, 140, (int)(300.0f * ((float)boss.hp / boss.maxHp)), 20, RED);
    sprintf(bossHpText, "HP: %d / %d", boss.hp, boss.maxHp);
    DrawText(bossHpText, SCREEN_WIDTH / 2 - 60, 140, 20, WHITE);


    // --- Desenhar Jogador ---
    // --- ATUALIZADO: Usando a função de sprite ---
    DrawPlayerSprite(SCREEN_WIDTH / 2 - 10, 260); // Posição (x, y) do torso
    //DrawText("PLAYER", SCREEN_WIDTH / 2 - 30, 310, 20, WHITE); // Opcional
    
    // HP do Jogador
    DrawRectangle(SCREEN_WIDTH / 2 - 150, 350, 300, 20, LIGHTGRAY);
    DrawRectangle(SCREEN_WIDTH / 2 - 150, 350, (int)(300.0f * ((float)player.hp / player.maxHp)), 20, GREEN);
    sprintf(playerHpText, "HP: %d / %d", player.hp, player.maxHp);
    DrawText(playerHpText, SCREEN_WIDTH / 2 - 60, 350, 20, WHITE);

    // --- Mensagem de Batalha ---
    DrawRectangle(0, SCREEN_HEIGHT - 180, SCREEN_WIDTH, 60, BLACK);
    DrawText(battleMessage, 20, SCREEN_HEIGHT - 170, 20, WHITE);

    // --- Inventário / Opções ---
    DrawText("INVENTARIO (Use SETAS e ENTER):", 20, SCREEN_HEIGHT - 100, 20, RAYWHITE);
    
    int itemPosX = 20;
    int itemWidth = 180;
    
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        Color itemColor = WHITE;
        
        // Item selecionado
        if (i == selectedItemIndex) {
            DrawRectangle(itemPosX - 5, SCREEN_HEIGHT - 70, itemWidth + 10, 40, YELLOW);
            itemColor = BLACK;
        }
        
        // Item já usado (agora só afeta poção/bomba)
        if (itemUsed[i]) {
            itemColor = GRAY;
        }
        
        DrawText(GetItemName(inventory[i]), itemPosX, SCREEN_HEIGHT - 60, 15, itemColor);
        itemPosX += itemWidth + 20; // Espaçamento
    }
}

// Desenha a Tela Final
void DrawEnding(bool playerWon) {
    ClearBackground(BLACK);
    
    if (playerWon) {
        DrawText("VITORIA!", 250, 250, 50, GREEN);
        DrawText("O Chefe foi derrotado e a masmorra esta segura.", 150, 320, 20, RAYWHITE);
    } else {
        DrawText("GAME OVER", 250, 250, 50, RED);
        DrawText("Voce foi derrotado...", 280, 320, 20, RAYWHITE);
    }
    
    DrawText("Pressione [ENTER] para jogar novamente.", 200, 500, 20, GRAY);
}

//------------------------------------------------------------------------------------
// Loop Principal do Jogo
//------------------------------------------------------------------------------------
int main(void) {
    // Inicialização
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Raylib RPG de Turnos");
    InitGame();
    SetTargetFPS(60);

    // Loop principal
    while (!WindowShouldClose()) {
        // -----------------
        // Update (Lógica)
        // -----------------
        switch (currentState) {
            case GAME_STATE_EXPLORE:
                UpdateExplore();
                break;
            case GAME_STATE_BATTLE:
                UpdateBattle();
                break;
            case GAME_STATE_ENDING_GOOD:
            case GAME_STATE_ENDING_BAD:
                UpdateEnding();
                break;
        }

        // -----------------
        // Draw (Gráficos)
        // -----------------
        BeginDrawing();

        switch (currentState) {
            case GAME_STATE_EXPLORE:
                DrawExplore();
                break;
            case GAME_STATE_BATTLE:
                DrawBattle();
                break;
            case GAME_STATE_ENDING_GOOD:
                DrawEnding(true); // Tela de vitória
                break;
            case GAME_STATE_ENDING_BAD:
                DrawEnding(false); // Tela de derrota
                break;
        }

        EndDrawing();
    }

    // Finalização
    CloseWindow();
    return 0;
}
