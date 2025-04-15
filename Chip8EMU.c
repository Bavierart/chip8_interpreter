// CHIP-8 Emulator in C

// Inclusion of libraries
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
// Graphics API inclusion
#include <SDL2/SDL.h>

// Definition of graphics constants
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define SCALE 10

// SDL window and renderer
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;

// Keymap for CHIP-8 keypad
const SDL_Keycode keymap[16] = {
    SDLK_x, SDLK_1, SDLK_2, SDLK_3, // 0x0, 0x1, 0x2, 0x3
    SDLK_q, SDLK_w, SDLK_e, SDLK_a, // 0x4, 0x5, 0x6, 0x7
    SDLK_s, SDLK_d, SDLK_z, SDLK_c, // 0x8, 0x9, 0xA, 0xB
    SDLK_4, SDLK_r, SDLK_f, SDLK_v  // 0xC, 0xD, 0xE, 0xF
};

// Initialize SDL, create a window, and renderer
bool initialize_sdl()
{
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return false;
    }
    // Creating a window
    window = SDL_CreateWindow("CHIP-8 Emulator",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              SCREEN_WIDTH * SCALE, SCREEN_HEIGHT * SCALE,
                              SDL_WINDOW_SHOWN);
    // Checking if the window was created
    if (window == NULL)
    {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }
    // Creating a renderer to render to the window
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
    {
        fprintf(stderr, "Renderer creation failed! SDL Error: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

// Destroy the SDL window and renderer
void cleanup_sdl()
{
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

// CHIP-8 structure definition
struct chip8
{
    uint8_t memory[4096];  // Memory (4KB)
    uint8_t V[16];         // 16 registers (V0 to VF)
    uint16_t I;            // Index register
    uint16_t pc;           // Program counter
    uint8_t delay_timer;   // Delay timer
    uint8_t sound_timer;   // Sound timer
    uint8_t gfx[64 * 32];  // Graphics buffer (64x32 pixels)
    uint8_t key[16];       // Keypad state
    uint16_t stack[16];    // Stack for subroutine calls
    uint8_t sp;            // Stack pointer
};

// Handle input events and update the CHIP-8 keypad state
void handle_input(struct chip8 *Chip8)
{
    // Create an event variable to search for events
    SDL_Event event;
    // Searches for events constantly
    while (SDL_PollEvent(&event))
    {
        // Check for quit event
        if (event.type == SDL_QUIT)
        {
            exit(0);
        }
        // Check for keypresses
        else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
        {
            // Iterate through keys to check for the key pressed
            for (int i = 0; i < 16; i++)
            {
                // Store 1 if pressed and 0 if unpressed
                if (event.key.keysym.sym == keymap[i])
                {
                    Chip8->key[i] = (event.type == SDL_KEYDOWN) ? 1 : 0;
                }
            }
        }
    }
}

// Load a ROM file into CHIP-8 memory
bool load_rom(struct chip8 *Chip8, const char *filename)
{
    // Create a file pointer to open the file
    FILE *file = fopen(filename, "rb");
    printf("Attempting to load ROM: %s\n", filename);
    // If no file is found
    if (file == NULL)
    {
        fprintf(stderr, "Failed to open ROM file: %s\n", filename);
        return false;
    }
    // Check if the file is empty
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);
    // If the file is too large
    if (file_size > (sizeof(Chip8->memory) - 0x200))
    {
        fprintf(stderr, "ROM file is too large to fit in memory: %s\n", filename);
        fclose(file);
        return false;
    }

    // Check for how many bytes compose the ROM
    size_t bytes_read = fread(&Chip8->memory[0x200], 1, sizeof(Chip8->memory) - 0x200, file);
    // If ROM is empty
    if (bytes_read == 0)
    {
        fprintf(stderr, "Failed to read ROM file: %s\n", filename);
        fclose(file);
        return false;
    }
    // Close the file after reading it
    fclose(file);
    printf("Loaded ROM: %s (%zu bytes)\n", filename, bytes_read);
    return true;
}

int main(int argc, char *argv[])
{
    // Validate command-line arguments
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <ROM file>\n", argv[0]);
        return 1;
    }
    // Initialize SDL
    if (!initialize_sdl())
    {
        return 1;
    }

    // Declare and initialize the CHIP-8 structure
    struct chip8 Chip8;
    memset(&Chip8, 0, sizeof(Chip8)); // Zero out the structure
    Chip8.pc = 0x200;                // Program starts at address 0x200
    Chip8.sp = 0;                    // Initialize stack pointer

    // Check if the graphics buffer is properly allocated
    if (!Chip8.gfx)
    {
        fprintf(stderr, "Chip8.gfx is NULL! Memory not allocated properly.\n");
        exit(1);
    }

    // Load the ROM file
    if (!load_rom(&Chip8, argv[1]))
    {
        cleanup_sdl();
        return 1;
    }

    // Declare the opcode
    uint16_t opcode = 0x0000;

    // Main emulation loop
    uint32_t last_time = SDL_GetTicks();
    while (1)
    {
        // Update timers at ~60Hz
        uint32_t current_time = SDL_GetTicks();
        if (current_time - last_time >= 16) // ~60Hz
        {
            if (Chip8.delay_timer > 0)
                Chip8.delay_timer--;
            if (Chip8.sound_timer > 0)
                Chip8.sound_timer--;
            last_time = current_time;
        }

        // Handle input events
        handle_input(&Chip8);

        // Fetch and decode the next opcode
        if (Chip8.pc + 1 < 4096)
        {
            opcode = (Chip8.memory[Chip8.pc] << 8) | Chip8.memory[Chip8.pc + 1];
            Chip8.pc += 2;
        }

        uint8_t n = opcode & 0x000F;       // Last nibble
        uint8_t nn = opcode & 0x00FF;      // Last byte
        uint16_t nnn = opcode & 0x0FFF;    // Last three bytes
        uint8_t x = (opcode & 0x0F00) >> 8; // X register
        uint8_t y = (opcode & 0x00F0) >> 4; // Y register
        uint8_t verify = (opcode & 0xF000) >> 12; // Verification variable for opcode

        // Execute the opcode
        switch (verify)
        {
        case 0x0:
            // Handle 0x00E0 (clear screen) and 0x00EE (return from subroutine)
            if (opcode == 0x00E0)
            {
                // Clear the screen
                memset(Chip8.gfx, 0, sizeof(Chip8.gfx));
            }
            else if (opcode == 0x00EE)
            {
                    Chip8.sp--;
            }
            break;

        case 0x1:
            // Jump to address nnn
            Chip8.pc = nnn;
            break;

        case 0x2:
            // Call subroutine at address nnn
            if (Chip8.sp < 16)
            {
                Chip8.stack[Chip8.sp] = Chip8.pc; // Store current program counter
                Chip8.sp++;                       // Move stack pointer up
                Chip8.pc = nnn;                   // Jump to subroutine
            }
            else
            {
                fprintf(stderr, "Stack overflow error: Too many nested subroutine calls.\n");
                exit(1);
            }
            break;

        case 0x3:
            // Skip next instruction if V[x] == nn
            if (Chip8.V[x] == nn)
            {
                Chip8.pc += 2;
            }
            break;

        case 0x4:
            // Skip next instruction if V[x] != nn
            if (Chip8.V[x] != nn)
            {
                Chip8.pc += 2;
            }
            break;

        case 0x5:
            // Skip next instruction if V[x] == V[y]
            if (Chip8.V[x] == Chip8.V[y])
            {
                Chip8.pc += 2;
            }
            break;

        case 0x6:
            // Set V[x] to nn
            Chip8.V[x] = nn;
            break;

        case 0x7:
            // Add nn to V[x]
            Chip8.V[x] += nn;
            break;

        case 0x8:
            // Handle arithmetic and bitwise operations
            switch (n)
            {
            case 0:
                // Set V[x] to V[y]
                Chip8.V[x] = Chip8.V[y];
                break;

            case 1:
                // Set V[x] to V[x] OR V[y]
                Chip8.V[x] |= Chip8.V[y];
                break;

            case 2:
                // Set V[x] to V[x] AND V[y]
                Chip8.V[x] &= Chip8.V[y];
                break;

            case 3:
                // Set V[x] to V[x] XOR V[y]
                Chip8.V[x] ^= Chip8.V[y];
                break;

            case 4:
                // Declare sum variable inside the correct scope
                {
                    uint16_t sum = Chip8.V[x] + Chip8.V[y];
                    Chip8.V[0xF] = (sum > 255) ? 1 : 0; // Set carry flag
                    Chip8.V[x] = sum & 0xFF;           // Store the lower 8 bits of the result
                }
                break;

            case 5:
                // Set V[x] to V[x] - V[y], set VF to 0 if there's a borrow, 1 otherwise
                if (Chip8.V[x] >= Chip8.V[y])
                {
                    Chip8.V[0xF] = 1; // No borrow
                }
                else
                {
                    Chip8.V[0xF] = 0; // Borrow occurred
                }
                Chip8.V[x] -= Chip8.V[y];
                break;

            case 6:
                // Shift V[x] right by 1, store the least significant bit in VF
                Chip8.V[0xF] = Chip8.V[x] & 0x1;
                Chip8.V[x] >>= 1;
                break;

            case 7:
                // Set V[x] to V[y] - V[x], set VF to 0 if there's a borrow, 1 otherwise
                if (Chip8.V[y] >= Chip8.V[x])
                {
                    Chip8.V[0xF] = 1; // No borrow
                }
                else
                {
                    Chip8.V[0xF] = 0; // Borrow occurred
                }
                Chip8.V[x] = Chip8.V[y] - Chip8.V[x];
                break;

            case 0xE:
                // Shift V[x] left by 1, store the most significant bit in VF
                Chip8.V[0xF] = (Chip8.V[x] & 0x80) >> 7;
                Chip8.V[x] <<= 1;
                break;
            }
            break;

        case 0x9:
            // Skip next instruction if V[x] != V[y]
            if (Chip8.V[x] != Chip8.V[y])
            {
                Chip8.pc += 2;
            }
            break;

        case 0xA:
            // Set I to nnn
            Chip8.I = nnn;
            break;

        case 0xB:
            // Jump to address nnn + V[0]
            Chip8.pc = nnn + Chip8.V[0];
            break;

        case 0xC:
            // Set V[x] to a random number AND nn
            Chip8.V[x] = (rand() % 256) & nn;
            break;

        case 0xD:
            // Draw a sprite at (V[x], V[y]) with height n
            uint8_t x_coord = Chip8.V[(opcode & 0x0F00) >> 8] % SCREEN_WIDTH;
            uint8_t y_coord = Chip8.V[(opcode & 0x00F0) >> 4] % SCREEN_HEIGHT;
            uint8_t height = opcode & 0x000F;
            uint8_t pixel;

            Chip8.V[0xF] = 0; // Reset VF

            // Loop through each row of the sprite
            for (int row = 0; row < height; row++)
            {
                pixel = Chip8.memory[Chip8.I + row]; // Fetch sprite byte from memory
                // Loop through each column of the sprite
                for (int col = 0; col < 8; col++)
                {
                    // Check if the current bit is set
                    if ((pixel & (0x80 >> col)) != 0)
                    {
                        // Calculate the index in the graphics buffer
                        int index = (x_coord + col) + ((y_coord + row) * SCREEN_WIDTH);
                        if (Chip8.gfx[index] == 1)
                        {
                            Chip8.V[0xF] = 1;
                        }
                        Chip8.gfx[index] ^= 1;
                    }
                }
            }

            // Render the updated graphics
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Black background
            SDL_RenderClear(renderer);                     // Clear the screen

            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // White pixels
            // Draw the pixels
            for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
            {
                // Check if the pixel is set
                if (Chip8.gfx[i])
                {
                    SDL_Rect rect;
                    rect.x = (i % SCREEN_WIDTH) * SCALE;
                    rect.y = (i / SCREEN_WIDTH) * SCALE;
                    rect.w = SCALE;
                    rect.h = SCALE;
                    SDL_RenderFillRect(renderer, &rect);
                }
            }
            // Present the renderer
            SDL_RenderPresent(renderer);
            break;

        case 0xE:
            // Skip instructions based on key state
            if (nn == 0x9E)
            {
                // Skip next instruction if key with value of V[x] is pressed
                if (Chip8.key[Chip8.V[x]] != 0)
                {
                    Chip8.pc += 2;
                }
            }
            else if (nn == 0xA1)
            {
                // Skip next instruction if key with value of V[x] is not pressed
                if (Chip8.key[Chip8.V[x]] == 0)
                {
                    Chip8.pc += 2;
                }
            }
            break;

        case 0xF:
            // Handle miscellaneous instructions
            switch (nn)
            {
            case 0x07:
                // Set V[x] to the value of the delay timer
                Chip8.V[x] = Chip8.delay_timer;
                break;

            case 0x0A:
                // Declare key_pressed variable inside the correct scope
                {
                    int key_pressed = 0;
                    while (!key_pressed)
                    {
                        SDL_Event event;
                        while (SDL_PollEvent(&event))
                        {
                            if (event.type == SDL_QUIT)
                            {
                                exit(0); // Exit the program
                            }
                            if (event.type == SDL_KEYDOWN)
                            {
                                for (int i = 0; i < 16; i++)
                                {
                                    if (event.key.keysym.sym == keymap[i])
                                    {
                                        Chip8.V[x] = i;
                                        key_pressed = 1;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
                break;

            case 0x15:
                // Set the delay timer to V[x]
                Chip8.delay_timer = Chip8.V[x];
                break;

            case 0x18:
                // Set the sound timer to V[x]
                Chip8.sound_timer = Chip8.V[x];
                break;

            case 0x1E:
                // Add V[x] to I, set VF to 1 if there's a range overflow
                Chip8.V[0xF] = (Chip8.I + Chip8.V[x] > 0xFFF) ? 1 : 0;
                Chip8.I = (Chip8.I + Chip8.V[x]) & 0xFFF; // Wrap around to 12 bits
                break;

            case 0x29:
                // Set I to the location of the sprite for the character in V[x]
                Chip8.I = 0x050 + (Chip8.V[x] * 5);
                break;

            case 0x33:
                // Store the binary-coded decimal representation of V[x] in memory at I, I+1, and I+2
                Chip8.memory[Chip8.I] = Chip8.V[x] / 100;
                Chip8.memory[Chip8.I + 1] = (Chip8.V[x] / 10) % 10;
                Chip8.memory[Chip8.I + 2] = Chip8.V[x] % 10;
                break;

            case 0x55:
                // Store V[0] to V[x] in memory starting at I
                for (int i = 0; i <= x; i++)
                {
                    Chip8.memory[Chip8.I + i] = Chip8.V[i];
                }
                break;

            case 0x65:
                // Fill V[0] to V[x] with values from memory starting at I
                for (int i = 0; i <= x; i++)
                {
                    Chip8.V[i] = Chip8.memory[Chip8.I + i];
                }
                break;

            default:
                // Unknown opcode
                break;
            }
            break;

        default:
            // Unknown opcode
            fprintf(stderr, "Unknown opcode: 0x%X\n", opcode);
            exit(1); // Halt execution for unknown opcodes
            break;
        }

        // Delay to maintain ~60Hz
        SDL_Delay(16);
    }

    // Cleanup and exit
    cleanup_sdl();
    return 0;
}
