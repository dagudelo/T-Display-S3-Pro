# Pac-Man Implementation Plan

## Target Hardware
- **Board**: T-Display-S3-AMOLED (ESP32-S3, 8MB PSRAM)
- **Display**: 536×240 AMOLED, used in portrait (240×536, rotation 1)
- **Input**: 2 buttons (GPIO0, GPIO21)
- **Framework**: Arduino via PlatformIO

## Display Layout (Portrait: 240×536)
```
┌──────────────────────┐   y=0
│  HUD (Score/Lives)   │   60px
├──────────────────────┤   y=60
│                      │
│   Maze 210×230px     │   10px tiles, 21×23 grid
│   centered at x=15   │
│                      │
├──────────────────────┤   y=290
│  Fruit Indicator     │   20px
│  Controls Hint       │   remaining
└──────────────────────┘   y=536
```

## File Structure
```
examples/pacman/
├── pacman.ino          # Main setup/loop, button handling, rendering orchestration
├── pins_config.h       # Pin definitions (copied from factory)
├── rm67162.h           # Display driver header (copied from factory)
├── rm67162.cpp         # Display driver (copied from factory)
├── maze.h              # ✅ Maze definition (DONE)
├── game.h              # Core types, enums, structs, constants, function declarations
├── game.cpp            # Game state management, level init, mode transitions
├── ghosts.h            # Ghost AI strategy declarations
├── ghosts.cpp          # Ghost AI: Blinky/Pinky/Inky/Clyde target selection, movement
├── render.h            # Rendering declarations
├── render.cpp          # Framebuffer-based drawing: maze, sprites, HUD
├── input.h             # 2-button direction control
├── input.cpp           # Button debouncing, direction buffering
└── PLAN.md             # This file
```

## Phase 1: Core Types & Data (game.h + game.cpp)

### Direction enum
```cpp
enum Direction : uint8_t { NONE, UP, DOWN, LEFT, RIGHT };
```

### GhostType enum
```cpp
enum GhostType : uint8_t { BLINKY, PINKY, INKY, CLYDE };
```

### GhostMode enum
```cpp
enum GhostMode : uint8_t { SCATTER, CHASE, FRIGHTENED, HOUSE, LEAVING_HOUSE, EATEN };
```

### Entity struct — base for Pac-Man and ghosts
```cpp
struct Entity {
    uint8_t col, row;       // tile position
    int16_t px, py;         // sub-tile pixel position
    Direction dir;          // current direction
    Direction next_dir;     // buffered next direction
    float speed;            // pixels per frame
};
```

### Ghost struct
```cpp
struct Ghost {
    Entity entity;
    GhostType type;
    GhostMode mode;
    uint8_t dot_counter;    // dots eaten before leaving house
    uint8_t dot_limit;      // dot limit to leave house
    bool in_house;
    uint8_t frightened_timer;
    uint8_t scatter_target_col, scatter_target_row;
};
```

### GameState struct
```cpp
struct GameState {
    uint8_t maze_dots[MAZE_ROWS][MAZE_COLS];  // live dot state (eaten or not)
    Entity pacman;
    Ghost ghosts[4];
    uint8_t level;
    uint32_t score;
    uint8_t lives;
    uint8_t dots_eaten;
    uint8_t total_dots;
    uint8_t ghost_eat_chain;       // consecutive ghosts eaten (0-3)
    uint16_t ghost_score_display;  // floating score popup timer
    uint8_t ghost_score_x, ghost_score_y;   // popup position
    uint32_t mode_timer;
    GhostMode global_ghost_mode;   // for non-frightened modes
    uint8_t mode_phase;            // which scatter/chase phase
    bool fruit_active;
    uint8_t fruit_type;            // which fruit (0=cherry...7=key)
    uint16_t fruit_timer;
    uint8_t fruit_col, fruit_row;
    bool fruit_eaten;
    bool game_over;
    bool level_complete;
    uint32_t frame_count;
    uint32_t pellet_timer;
};
```

### Level speed/mode timing tables
These follow the classic arcade timing charts:

**Ghost mode durations** (in seconds, per level):
- L1: Scatter 7s → Chase 20s → Scatter 7s → Chase 20s → Scatter 5s → Chase 20s → Scatter 5s → Chase ∞
- L2-4: Scatter 7→20→7→20→5→1033→1/60→∞ (Scatter eventually becomes 1 frame)
- L5+: Scatter 5→20→5→20→5→1037→1/60→∞

**Pac-Man speed** (% of base, per level):
- L1: 80%, L2-4: 90%, L5-20: 100%, L21+: 90%

**Ghost speed** (% of base, per level):
- L1: 75%, L2-4: 85%, L5-20: 95%, L21+: 95%

**Frightened ghost speed**: ~50% of base

**Tunnel speed**: ~40% of base for ghosts

**Frightened duration** (seconds, per level):
- L1: 6s, L2: 5s, L3: 4s, L4: 3s, L5: 2s, L6-8: 5s, L9-11: 2s, L12-14: 1s, L15-18: 1s, L19+: 0s (no frightened)

**Frightened flash count**: starts flashing at 2s remaining (varies by level)

## Phase 2: Ghost AI (ghosts.h + ghosts.cpp)

### Movement System
Each frame, every ghost calculates a target tile, then at each intersection chooses the valid direction that minimizes distance to that target. Ghosts cannot reverse direction (except when mode changes).

### Blinky (Shadow) — Red — Aggressive Chaser
- **Chase target**: Pac-Man's current tile
- **Cruise Elroy**: When dots remaining drop below threshold, Blinky speeds up:
  - Elroy 1: ≤20 dots remaining in L1 (≤30 L2, ≤40 L3-5, ≤50 L6-8, ≤60 L9-11, ≤70 L12-14, ≤80 L15-18, ≤90 L19+)
  - Elroy 2: ≤10 dots remaining in L1 (≤15 L2, ≤20 L3-5, ≤25 L6-8, ≤30 L9-11, ≤35 L12-14, ≤40 L15-18, ≤45 L19+)
- **Scatter corner**: Top-right (18, 1)

### Pinky (Speedy) — Pink — Ambusher
- **Chase target**: 4 tiles ahead of Pac-Man in his facing direction
  - Up: (Pac-Man.col, Pac-Man.row - 4) **AND** overflow left by 4 (classic bug): (Pac-Man.col - 4, Pac-Man.row - 4)
  - Down: (Pac-Man.col, Pac-Man.row + 4)
  - Left: (Pac-Man.col - 4, Pac-Man.row)
  - Right: (Pac-Man.col + 4, Pac-Man.row)
- **Scatter corner**: Top-left (1, 1)

### Inky (Bashful) — Cyan — Fickle
- **Chase target**: Vector from Blinky to 2 tiles ahead of Pac-Man, doubled
  - Calculate pivot = 2 tiles ahead of Pac-Man
  - Target = pivot + (pivot - Blinky.position) = 2×pivot - Blinky.position
  - This makes Inky's target swing wildly based on where Blinky is
- **Scatter corner**: Bottom-right (19, 21)

### Clyde (Pokey) — Orange — Feigning Ignorance
- **Chase target**: If distance(Pac-Man) ≥ 8 tiles → Pac-Man's tile. If distance < 8 tiles → scatter corner
- **Scatter corner**: Bottom-left (1, 21)

### Ghost House Exit Logic
Each ghost leaves the house based on a dot counter:
- **Blinky**: starts outside (doesn't enter house)
- **Pinky**: leaves immediately (dot_counter = 0, dot_limit = 0)
- **Inky**: leaves after 30 dots eaten (dot_limit = 30)
- **Clyde**: leaves after 60 dots eaten (dot_limit = 60)
- On later levels, dot limits decrease or become 0

After being eaten, ghosts return to house and exit after a short timer.

### Frightened Mode
- When Pac-Man eats a power pellet, all ghosts outside the house switch to FRIGHTENED
- Frightened ghosts move at 50% speed, choose random directions at intersections
- Ghosts flash white/blue for last 2 seconds of frightened duration
- If Pac-Man eats a frightened ghost:
  - Ghost enters EATEN mode, races back to house at 200% speed
  - Score: 200 → 400 → 800 → 1600 for consecutive ghosts per pellet

## Phase 3: Scoring (in game.cpp)

| Item | Points |
|------|--------|
| Dot | 10 |
| Power Pellet | 50 |
| Ghost 1 (per pellet) | 200 |
| Ghost 2 (per pellet) | 400 |
| Ghost 3 (per pellet) | 800 |
| Ghost 4 (per pellet) | 1,600 |

### Fruit Scoring by Level
| Fruit | Level(s) | Points |
|-------|----------|--------|
| Cherry | 1 | 100 |
| Strawberry | 2 | 300 |
| Orange | 3-4 | 500 |
| Apple | 5-6 | 700 |
| Melon | 7-8 | 1,000 |
| Galaxian | 9-10 | 2,000 |
| Bell | 11-12 | 3,000 |
| Key | 13+ | 5,000 |

- Fruits appear twice per level
- First fruit: after 70 dots eaten
- Second fruit: after 170 dots eaten
- Fruit stays for 9-10 seconds, then disappears
- Bonus life: 10,000 points

## Phase 4: Controls (input.h + input.cpp)

Two buttons for 4-directional control:
- **Button 1 (GPIO0)**: Rotate direction clockwise (UP→RIGHT→DOWN→LEFT→UP)
- **Button 2 (GPIO21)**: Rotate direction counter-clockwise (UP→LEFT→DOWN→RIGHT→UP)

Direction changes are buffered. Pac-Man only turns when reaching a tile intersection where the buffered direction is valid.

Press and hold either button on the title screen to start the game.

## Phase 5: Rendering (render.h + render.cpp)

Full framebuffer rendering using PSRAM:
- Allocate 240×536×2 = ~257KB framebuffer in PSRAM
- Render maze walls, dots, pellets, entities, HUD to framebuffer
- Push entire framebuffer to display via `lcd_PushColors`
- Fast per-pixel write macros for framebuffer access

### Wall Rendering
- Solid blue walls (color 0x001F)
- Walls are filled rectangles at each WALL tile

### Dot Rendering
- Small white dots (2×2 pixels centered in tile)

### Pellet Rendering  
- Larger white dots (4×4 pixels, blinking)

### Pac-Man Rendering
- Yellow circle with animated mouth
- Mouth angle oscillates: open → close cycle
- Mouth faces current direction

### Ghost Rendering
Each ghost is 8×8 pixels (centered in 10px tile):
- **Blinky**: Red body, white eyes, blue pupils
- **Pinky**: Pink body, white eyes, blue pupils
- **Inky**: Cyan body, white eyes, blue pupils
- **Clyde**: Orange body, white eyes, blue pupils

Frightened ghosts (all same):
- Blue body, simple face with wavy mouth
- Flash white/blue in last 2 seconds

Eaten ghost: Just eyes (no body)

### HUD Rendering
- Top-left: "SCORE" label + score value
- Top-right: "LEVEL" label + level number
- Below score: Lives display (Pac-Man icons)
- Fruit indicator below maze

## Phase 6: Game Loop (pacman.ino)

```
setup():
  - Init serial, display driver
  - Set rotation 1 (portrait)
  - Allocate framebuffer in PSRAM
  - Init buttons
  - Init game state (level 1, score 0, lives 3)
  - Draw title screen
  - Wait for button press to start

loop():
  - Read buttons, update buffered direction
  - if game_over: draw game over screen, wait for restart
  - if level_complete: advance level, reset dots
  - Run mode timer updates (scatter/chase phase transitions)
  - Move Pac-Man:
    - Try buffered direction at intersection
    - Check dot/pellet eating
    - Handle tunnel warp
  - For each ghost:
    - Update mode if timers expired
    - Calculate AI target tile
    - Move toward target (pick best direction at intersection)
    - Handle tunnel warp
    - Check collision with Pac-Man
    - Handle frightened/eaten states
  - Check fruit timer
  - Check level complete (all dots eaten)
  - Render frame to framebuffer
  - Push framebuffer to display
  - Delay for frame timing (~33ms for 30fps)
```

## Phase 7: main .ino + platformio.ini update

- Create `examples/pacman/pacman.ino` that `#include`s all headers and implements setup/loop
- Update `platformio.ini`: change `src_dir` to `examples/pacman`

## Implementation Order

1. ✅ **maze.h** — Maze grid, walkable checks, constants
2. **game.h** — All type definitions, constants, speed tables
3. **game.cpp** — State init, level setup, mode timing
4. **ghosts.h** — Ghost AI function declarations
5. **ghosts.cpp** — Full ghost AI (Blinky/Pinky/Inky/Clyde + movement)
6. **render.h + render.cpp** — Framebuffer drawing
7. **input.h + input.cpp** — Button handling
8. **pacman.ino** — Main file wiring everything together
9. **platformio.ini** — Update src_dir
10. **Build test** — Verify compilation
