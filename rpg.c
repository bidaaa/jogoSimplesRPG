#include "raylib.h"
#include <stdio.h>  // Para sprintf (formatar texto)
#include <stdbool.h> // Para bool, true, false
#include <stdlib.h> // Para rand, srand
#include <time.h>   // Para time (semente RNG)

//------------------------------------------------------------------------------------
// Constantes e Definições de Tela
//------------------------------------------------------------------------------------
#define SCREEN_WIDTH 1000
#define SCREEN_HEIGHT 700

//------------------------------------------------------------------------------------
// Estruturas de Dados do Jogo
//------------------------------------------------------------------------------------

// Estados principais do jogo
typedef enum {
    GAME_STATE_EXPLORE,
    GAME_STATE_BATTLE,
    GAME_STATE_ENDING_GOOD,
    GAME_STATE_ENDING_BAD,
    GAME_STATE_ENDING_ESCAPE
} GameState;

// Itens que o jogador pode coletar
typedef enum {
    ITEM_NONE,
    ITEM_POTION,      // Cura o jogador (Uso único)
    ITEM_SWORD,       // Causa dano médio ao chefe (Reutilizável)
    ITEM_BOMB,        // Causa dano alto ao chefe (Uso único)
    ITEM_COIN,        // Distrai o chefe para fugir (Uso único)
    ITEM_ARMOR        // Reduz dano recebido (Reutilizável)
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
static bool playerHasArmor;
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
        case ITEM_COIN:   return "Moeda (Pode \ndistrair o chefe)";
        case ITEM_ARMOR:  return "Armadura (Reduz dano)";
        default:          return "Vazio";
    }
}

// Define qual item o jogador recebe com base no estágio e na escolha (0 para esquerda/cima, 1 para direita/baixo)
ItemType GetItemForChoice(int stage, int choice) {
    if (stage == 0) {
        return (choice == 0) ? ITEM_POTION : ITEM_POTION;
    }
    if (stage == 1) {
        return (choice == 0) ? ITEM_SWORD : ITEM_BOMB;
    }
    if (stage == 2) {
        // Nesta cena o jogador pode coletar uma moeda (choice == 1)
        return (choice == 0) ? ITEM_ARMOR : ITEM_COIN;
    }
    if (stage == 3) {
        return (choice == 0) ? ITEM_POTION : ITEM_POTION;
    }
    return ITEM_NONE;
}

// Ação do chefe
void BossAttack() {
    if (playerHasArmor) {
        // Com armadura ativa, o dano é reduzido em 50%
        int min = (boss.attack - 5) / 2;
        if (min < 1) min = 1;
        int max = (boss.attack + 5) / 2;
        int damage = min + (rand() % (max - min + 1));
        player.hp -= damage;
        if (player.hp < 0) player.hp = 0;
        
        sprintf(messageBuffer, "Chefe ataca com armadura ativa! Voce levou %d de dano.", damage);
        battleMessage = messageBuffer;
    } else {
        // Dano aleatorio do chefe (varia em torno do valor base)
        int min = boss.attack - 5;
        if (min < 1) min = 1;
        int max = boss.attack + 5;
        int damage = min + (rand() % (max - min + 1));
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
            // A Espada nao e um item de utilitario: ela apenas aumenta o dano do ataque.
            battleMessage = "Espada: aumenta seu dano. Use ATACAR [A].";
            return; // nao gasta o turno nem prepara o turno do chefe
        case ITEM_BOMB:
            // Bomba causa dano alto e aleatorio
            {
                int min = 60;
                int max = 90;
                int dmg = min + (rand() % (max - min + 1));
                boss.hp -= dmg;
                if (boss.hp < 0) boss.hp = 0;
                sprintf(messageBuffer, "Voce usou Bomba! Causou %d de dano!", dmg);
                battleMessage = messageBuffer;
                itemUsed[index] = true; // <-- Marcado como usado
            }
            break;
        case ITEM_COIN:
            // Usar a moeda tem 50% de chance de distrair o chefe
            itemUsed[index] = true;
            if (rand() % 2 == 0) {
                // Sucesso: distrai o chefe e fuga
                battleMessage = "Voce usou Moeda! Distraiu o chefe e fugiu!";
                currentState = GAME_STATE_ENDING_ESCAPE;
                return; // Transicao imediata para o final de fuga
            } else {
                // Falha: moeda nao distrai, chefe contra-ataca
                battleMessage = "Voce usou Moeda! Mas o chefe nao se distraiu...";
                // Prepara o turno do chefe para contra-atacar
                battleState = BATTLE_BOSS_TURN;
                bossTurnTimer = 1.5f;
                return; // Gasta o turno
            }
        case ITEM_ARMOR:
            playerHasArmor = true;
            battleMessage = "Voce equipou Armadura! Proximos ataques causarao menos dano.";
            // NÂO marcar como usado (permanece ativo)
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

// Verifica se o jogador possui uma espada no inventario
bool PlayerHasSword(void) {
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        if (inventory[i] == ITEM_SWORD) return true;
    }
    return false;
}

// Ação de ataque do jogador (botao ATACAR)
void PlayerAttack(void) {
    // Dano aleatorio do jogador; espada aumenta o intervalo
    int damage = 0;
    if (PlayerHasSword()) {
        int min = 20;
        int max = 40;
        damage = min + (rand() % (max - min + 1));
        boss.hp -= damage;
        if (boss.hp < 0) boss.hp = 0;
        sprintf(messageBuffer, "Voce atacou com a espada! Causou %d de dano!", damage);
    } else {
        int min = 5;
        int max = 15;
        damage = min + (rand() % (max - min + 1));
        boss.hp -= damage;
        if (boss.hp < 0) boss.hp = 0;
        sprintf(messageBuffer, "Voce atacou desarmado! Causou %d de dano!", damage);
    }
    battleMessage = messageBuffer;

    // Prepara o turno do chefe
    battleState = BATTLE_BOSS_TURN;
    bossTurnTimer = 1.5f;
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
    player.hp = 120;
    player.maxHp = 120;

    // Status do Chefe
    boss.hp = 200;
    boss.maxHp = 200;
    boss.attack = 22; // Dano do chefe

    // Status da Batalha
    battleState = BATTLE_PLAYER_TURN;
    selectedItemIndex = 0;
    playerHasArmor = false;
    battleMessage = "Batalha contra o Chefe! Escolha seu item.";

    // Inicializar semente do RNG para danos aleatorios
    srand((unsigned int)time(NULL));
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

        // Ataque direto (botao ATACAR)
        if (IsKeyPressed(KEY_A)) {
            PlayerAttack();
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
    DrawCircle(posX + 10, posY - 10, 10, (Color){255, 200, 150, 255});
    DrawCircleLines(posX + 10, posY - 10, 10, BLACK);
    // Olhos
    DrawCircle(posX + 6, posY - 12, 2, BLACK);
    DrawCircle(posX + 14, posY - 12, 2, BLACK);
    // Torso
    DrawRectangle(posX, posY, 20, 40, (Color){0, 150, 255, 255});
    DrawRectangleLines(posX, posY, 20, 40, (Color){0, 100, 200, 255});
    // Braços
    DrawRectangle(posX - 5, posY + 5, 5, 20, (Color){255, 200, 150, 255});
    DrawRectangle(posX + 20, posY + 5, 5, 20, (Color){255, 200, 150, 255});
    // Pernas
    DrawRectangle(posX + 2, posY + 40, 6, 20, (Color){50, 50, 50, 255});
    DrawRectangle(posX + 12, posY + 40, 6, 20, (Color){50, 50, 50, 255});
}

void DrawBossSprite(int posX, int posY) {
    // Aura/Sombra
    DrawCircle(posX + 20, posY + 30, 50, (Color){100, 0, 0, 100});
    // Cabeça
    DrawCircle(posX + 20, posY - 15, 15, (Color){80, 20, 20, 255});
    DrawCircleLines(posX + 20, posY - 15, 15, (Color){200, 50, 50, 255});
    // Olhos malévolos
    DrawCircle(posX + 14, posY - 18, 3, (Color){255, 100, 0, 255});
    DrawCircle(posX + 26, posY - 18, 3, (Color){255, 100, 0, 255});
    // Torso (maior)
    DrawRectangle(posX, posY, 40, 60, (Color){200, 0, 0, 255});
    DrawRectangleLines(posX, posY, 40, 60, (Color){100, 0, 0, 255});
    // Braços
    DrawRectangle(posX - 10, posY + 10, 10, 30, (Color){150, 0, 0, 255});
    DrawRectangle(posX + 40, posY + 10, 10, 30, (Color){150, 0, 0, 255});
    // Pernas
    DrawRectangle(posX + 5, posY + 60, 10, 30, (Color){100, 0, 0, 255});
    DrawRectangle(posX + 25, posY + 60, 10, 30, (Color){100, 0, 0, 255});
}


// Desenha a UI de Exploração
void DrawExplore(void) {
    ClearBackground((Color){20, 20, 40, 255});

    // --- NOVO: Checar se a mensagem de item deve aparecer ---
    if (itemMessageTimer > 0) {
        // Fundo com efeito
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){0, 0, 0, 200});
        
        // Caixa de coleta
        DrawRectangle(SCREEN_WIDTH / 2 - 300, SCREEN_HEIGHT / 2 - 100, 600, 200, (Color){50, 50, 100, 255});
        DrawRectangleLines(SCREEN_WIDTH / 2 - 300, SCREEN_HEIGHT / 2 - 100, 600, 200, (Color){100, 200, 255, 255});
        
        // Mensagem de coleta
        sprintf(messageBuffer, "Voce coletou: %s!", GetItemName(lastItemCollected));
        int textWidth = MeasureText(messageBuffer, 30);
        DrawText(messageBuffer, SCREEN_WIDTH / 2 - textWidth / 2, SCREEN_HEIGHT / 2 - 50, 30, (Color){100, 255, 150, 255});
        
        const char* msg2 = "Carregando proximo cenario...";
        int textWidth2 = MeasureText(msg2, 18);
        DrawText(msg2, SCREEN_WIDTH / 2 - textWidth2 / 2, SCREEN_HEIGHT / 2 + 40, 18, (Color){150, 150, 200, 255});
        
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

    DrawText(storyText, 40, 150, 22, (Color){200, 220, 255, 255});
    
    // Caixas de escolha
    DrawRectangle(40, 230, 450, 80, (Color){50, 80, 150, 200});
    DrawRectangleLines(40, 230, 450, 80, (Color){100, 150, 255, 255});
    DrawText(choice1Text, 60, 255, 18, (Color){200, 255, 100, 255});
    
    DrawRectangle(40, 320, 450, 80, (Color){50, 80, 150, 200});
    DrawRectangleLines(40, 320, 450, 80, (Color){100, 150, 255, 255});
    DrawText(choice2Text, 60, 345, 18, (Color){200, 255, 100, 255});
    
    DrawText("Pressione [1] ou [2] para escolher...", 40, 550, 16, (Color){150, 200, 255, 255});
}

// Desenha a UI de Batalha
void DrawBattle(void) {
    // Buffers locais para guardar o texto formatado do HP
    char bossHpText[64];
    char playerHpText[64];
    
    ClearBackground((Color){30, 30, 50, 255});

    // --- Desenhar Chefe ---
    DrawBossSprite(SCREEN_WIDTH / 2 - 20, 50);
    
    // HP do Chefe com borda
    DrawRectangle(SCREEN_WIDTH / 2 - 150, 140, 300, 25, (Color){50, 50, 80, 255});
    DrawRectangleLines(SCREEN_WIDTH / 2 - 150, 140, 300, 25, (Color){100, 100, 150, 255});
    DrawRectangle(SCREEN_WIDTH / 2 - 150, 140, (int)(300.0f * ((float)boss.hp / boss.maxHp)), 25, (Color){255, 50, 50, 255});
    sprintf(bossHpText, "HP: %d / %d", boss.hp, boss.maxHp);
    DrawText(bossHpText, SCREEN_WIDTH / 2 - 65, 142, 20, WHITE);

    // --- Desenhar Jogador ---
    DrawPlayerSprite(SCREEN_WIDTH / 2 - 10, 260);
    
    // HP do Jogador com borda
    DrawRectangle(SCREEN_WIDTH / 2 - 150, 350, 300, 25, (Color){50, 50, 80, 255});
    DrawRectangleLines(SCREEN_WIDTH / 2 - 150, 350, 300, 25, (Color){100, 100, 150, 255});
    DrawRectangle(SCREEN_WIDTH / 2 - 150, 350, (int)(300.0f * ((float)player.hp / player.maxHp)), 25, (Color){50, 200, 100, 255});
    sprintf(playerHpText, "HP: %d / %d", player.hp, player.maxHp);
    DrawText(playerHpText, SCREEN_WIDTH / 2 - 65, 352, 20, WHITE);

    // --- Mensagem de Batalha ---
    DrawRectangle(0, SCREEN_HEIGHT - 180, SCREEN_WIDTH, 70, (Color){20, 20, 40, 255});
    DrawRectangleLines(0, SCREEN_HEIGHT - 180, SCREEN_WIDTH, 70, (Color){100, 150, 200, 255});
    DrawText(battleMessage, 25, SCREEN_HEIGHT - 160, 16, (Color){150, 255, 200, 255});

    // --- Inventário / Opções ---
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
        // Nao mostrar a Espada e Armadura nos utilitarios (apenas modificam acao)
        if (inventory[i] == ITEM_SWORD || inventory[i] == ITEM_ARMOR) displayName = "Vazio";
        DrawText(displayName, itemPosX + 5, SCREEN_HEIGHT - 58, 12, itemColor);
        itemPosX += itemWidth + 20;
    }

    // Desenhar botao Atacar com estilo
    int btnW = 130;
    int btnH = 45;
    int btnX = SCREEN_WIDTH - btnW - 20;
    int btnY = SCREEN_HEIGHT - 85;
    DrawRectangle(btnX, btnY, btnW, btnH, (Color){180, 50, 50, 255});
    DrawRectangleLines(btnX, btnY, btnW, btnH, (Color){255, 100, 100, 255});
    DrawText("ATACAR [A]", btnX + 10, btnY + 10, 16, WHITE);
}

// Desenha a Tela Final
void DrawEnding(bool playerWon) {
    ClearBackground((Color){20, 20, 40, 255});
    
    // Fundo decorativo
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

// Desenha o final de fuga (moeda)
void DrawEscapeEnding(void) {
    ClearBackground((Color){20, 20, 40, 255});
    
    // Fundo decorativo
    DrawRectangle(SCREEN_WIDTH / 2 - 400, SCREEN_HEIGHT / 2 - 200, 800, 400, (Color){40, 40, 80, 200});
    DrawRectangleLines(SCREEN_WIDTH / 2 - 400, SCREEN_HEIGHT / 2 - 200, 800, 400, (Color){255, 200, 100, 255});
    
    DrawText("FUGA!", 300, 200, 60, (Color){255, 200, 100, 255});
    DrawText("Voce distraiu o chefe com a moeda e conseguiu fugir da masmorra.", 80, 300, 20, (Color){255, 220, 150, 255});
    DrawText("Este final conta como uma fuga — nem vitoria nem derrota.", 100, 340, 18, (Color){200, 180, 120, 255});
    DrawText("Pressione [ENTER] para jogar novamente.", 200, 520, 20, (Color){150, 200, 255, 255});
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
            case GAME_STATE_ENDING_ESCAPE:
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
            case GAME_STATE_ENDING_ESCAPE:
                DrawEscapeEnding(); // Tela de fuga
                break;
        }

        EndDrawing();
    }

    // Finalização
    CloseWindow();
    return 0;
}
