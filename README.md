# 🎮 Jogo RPG de Turnos - Masmorra do Chefe

Um jogo RPG baseado em turnos desenvolvido em **C com Raylib**. O jogador passa por 4 estágios de exploração coletando itens antes de enfrentar um chefe final em uma batalha dinâmica e desafiadora.

## 📋 Características

### 🎯 Mecânicas Principais

- **Exploração Interativa**: 4 estágios com escolhas binárias que definem seu inventário
- **Sistema de Combate por Turnos**: Batalha contra um chefe com dano aleatório
- **5 Itens Únicos**: Cada um com efeito diferente na batalha
- **3 Finais Diferentes**: Vitória, Derrota ou Fuga
- **Balanceamento Dinâmico**: Armadura reduz dano, Espada aumenta dano de ataque

### 🎨 Visual

- Sprites coloridos e animados para jogador e boss
- Interface intuitiva com caixas de seleção destacadas
- Barras de HP dinâmicas em tempo real
- Mensagens de ação em tempo real
- Temas de cores temáticos por seção

## 🎮 Como Jogar

### Controles

**Exploração:**
- `1` - Escolha a primeira opção
- `2` - Escolha a segunda opção

**Batalha:**
- `SETA ESQUERDA/DIREITA` - Navegar pelo inventário
- `ENTER` - Usar item selecionado
- `A` - Atacar com arma/desarmado
- `ENTER` (Final) - Reiniciar jogo

### 📦 Itens Disponíveis

| Item | Descrição | Tipo | Disponibilidade |
|------|-----------|------|-----------------|
| **Poção** | Cura 50 HP | Consumível | Estágios 0 e 3 |
| **Espada** | Aumenta dano (20-40 vs 5-15) | Passivo | Estágio 1, Choice 1 |
| **Bomba** | Dano alto (60-90 HP) | Consumível | Estágio 1, Choice 2 |
| **Armadura** | Reduz dano recebido 50% | Passivo | Estágio 2, Choice 1 |
| **Moeda** | 50% chance de escapar | Consumível | Estágio 2, Choice 2 |

### 🗺️ Caminho Recomendado (Todos os Itens)

1. **Estágio 0**: Pressione `1` → Poção
2. **Estágio 1**: Pressione `1` → Espada
3. **Estágio 2**: Pressione `1` → Armadura
4. **Estágio 3**: Pressione `2` → Poção

*Alternativa com Moeda: No Estágio 2, pressione `2` → Moeda (50% de chance de fuga)*

## ⚔️ Sistema de Batalha

### Status Inicial
- **Jogador**: 150 HP
- **Chefe**: 200 HP
- **Dano do Chefe**: Base 22 (varia ±5, sem armadura: 17-27 | com armadura: 8-13)

### Danos
- **Sem Espada**: 5-15 dano
- **Com Espada**: 20-40 dano
- **Bomba**: 60-90 dano
- **Com Armadura**: Dano recebido reduzido em 50%

### Finais Possíveis
1. **VITÓRIA** ✅ - Derrotar o chefe (HP ≤ 0)
2. **GAME OVER** ❌ - Jogador derrotado (HP ≤ 0)
3. **FUGA** 🏃 - Usar moeda com sucesso (50% de chance)

## 🚀 Instalação e Execução

### Pré-requisitos
- GCC (compilador C)
- Raylib instalado no sistema

### Compilação

```bash
gcc -o rpg rpg.c -lraylib -lm -lpthread -ldl -lrt -lX11
```

### Execução

```bash
./rpg
```

### Teste Rápido
```bash
gcc -o rpg rpg.c -lraylib -lm -lpthread -ldl -lrt -lX11 && ./rpg
```

## 📁 Estrutura do Projeto

```
jogoSimplesRPG/
├── rpg.c           # Código-fonte principal
├── README.md       # Este arquivo
└── (binário rpg após compilação)
```

## 🎨 Especificações Técnicas

- **Resolução**: 1000×700 pixels
- **FPS**: 60
- **Linguagem**: C
- **Biblioteca Gráfica**: Raylib
- **Sistema de RNG**: Srand com seed baseada em tempo

## 🔧 Funcionalidades Implementadas

- ✅ Sistema de exploração com 4 estágios
- ✅ Coleta dinâmica de itens
- ✅ Batalha por turnos com IA do chefe
- ✅ Dano aleatório para ambos os lados
- ✅ Sistema de armadura (redução de dano)
- ✅ Sistema de espada (aumento de dano)
- ✅ Bomba com dano alto
- ✅ Moeda com 50% de chance de escapar
- ✅ 3 finais diferentes
- ✅ Interface visual aprimorada
- ✅ Mensagens de ação em tempo real

## 📝 Exemplo de Gameplay

```
1. Exploração: Colete Poção → Espada → Armadura → Poção
2. Batalha: Equipe Armadura, use Bomba, depois ataque com Espada
3. Final: Derrote o chefe ou use Moeda para escapar
```

## 🎯 Dicas de Jogo

- A **Armadura** é essencial para reduzir o dano (50% de redução)
- A **Espada** triplica seu dano (20-40 vs 5-15)
- A **Bomba** é seu maior dano único (60-90)
- A **Moeda** é um risco: 50% de fuga ou pode resultar em derrota
- Combine **Armadura + Ataque com Espada** para uma estratégia equilibrada

## 🐛 Troubleshooting

**Erro de compilação com Raylib:**
```bash
# Verifique se Raylib está instalado
pkg-config --modversion raylib

# Ou instale-o
sudo apt-get install libraylib-dev  # Linux Debian/Ubuntu
```

**Janela não abre:**
- Verifique se há conflitos de display
- Tente rodar em um terminal diferente
- Verifique permissões de execução: `chmod +x rpg`

## 👨‍💻 Autor

Desenvolvido como um projeto de RPG educacional em C com Raylib.

## 📄 Licença

Projeto livre para uso e modificação.

---

**Aproveite o jogo! 🎮✨**