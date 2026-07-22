#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <SDL3/SDL.h>

#define SCALE 30

typedef struct {
	uint16_t NNN;
	uint8_t N;
	uint8_t X;
	uint8_t Y;
	uint8_t KK;
} instruction_t;

typedef struct {
	uint8_t memory[4096];
	uint8_t V[16];
	uint16_t I;
	uint8_t sound, delayTimer;
	uint16_t PC;
	uint16_t stack[16];
	uint16_t *stack_pointer;
	bool display[64 * 32];
	uint16_t opcode;
	bool keypad[16];
	instruction_t instruction;
} chip8_t;

bool running = true;
bool draw = true;

bool chipInit(chip8_t *chip8, const char rom[]) {
	srand(time(0));

	const uint8_t font[] = {
		0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
		0x20, 0x60, 0x20, 0x20, 0x70, // 1
		0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
		0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
		0x90, 0x90, 0xF0, 0x10, 0x10, // 4
		0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
		0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
		0xF0, 0x10, 0x20, 0x40, 0x40, // 7
		0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
		0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
		0xF0, 0x90, 0xF0, 0x90, 0x90, // A
		0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
		0xF0, 0x80, 0x80, 0x80, 0xF0, // C
		0xE0, 0x90, 0x90, 0x90, 0xE0, // D
		0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
		0xF0, 0x80, 0xF0, 0x80, 0x80  // F
	};

	// Load font
	memcpy(&chip8->memory[0x000], font, sizeof(font));

	// Load ROM
	FILE *file = fopen(rom, "rb");
	if (!file) {
		return false;
	}

	// Get size
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	rewind(file);

	fread(&chip8->memory[0x200], 1, size, file);

	fclose(file);

	chip8->PC = 0x200;
	chip8->stack_pointer = &chip8->stack[0];

	return true;
}

void execute(chip8_t *chip8) {
	// Get opcode and increment PC
	chip8->opcode = (chip8->memory[chip8->PC] << 8) | chip8->memory[chip8->PC + 1];
	chip8->PC += 2;

	chip8->instruction.NNN = (chip8->opcode & 0x0FFF); // NNN, lowest 12 bits
	chip8->instruction.N = (chip8->opcode & 0x000F); // N, lowest 4 bits
	chip8->instruction.X = ((chip8->opcode >> 8) & 0x000F); // X, lower 4 bits of high byte
	chip8->instruction.Y = ((chip8->opcode >> 4) & 0x000F); // Y, upper 4 bits of low byte
	chip8->instruction.KK = (chip8->opcode & 0x00FF);

	printf("Executing 0x%04X\n", chip8->opcode);

	// TODO: Stack over/underflow checks
	switch ((chip8->opcode >> 12) & 0x0F) {
		case 0x00:
			switch (chip8->opcode & 0x00FF) { 
				case 0xE0: // 00E0 (CLS): Clear the display.
					memset(chip8->display, false, sizeof(chip8->display));
					break;
				case 0xEE: // 00EE (RET): Return from a subroutine.
					chip8->stack_pointer--;
					chip8->PC = *chip8->stack_pointer;
					break;
				default:
					// Should only be 0nnn.
					printf("Instruction 0x%04X ignored.\n", chip8->opcode);
					break;
			}
			break;
		case 0x01: // 1nnn (JP addr): Jump to location nnn.
			chip8->PC = (chip8->instruction.NNN);
			break;
		case 0x02: // 2nnn (CALL addr): Call subroutine at nnn.
			*chip8->stack_pointer = chip8->PC;
			chip8->stack_pointer++;
			chip8->PC = (chip8->instruction.NNN);
			break;
		case 0x03: // 3xkk (SE Vx, byte): Skip next instruction if Vx = kk.
			if (chip8->V[chip8->instruction.X] == chip8->instruction.KK) {
				chip8->PC +=2;
			}
			break;
		case 0x04: // 4xkk (SNE Vx, byte): Skip next instruction if Vx != kk.
			if (chip8->V[chip8->instruction.X] != chip8->instruction.KK) {
				chip8->PC += 2;
			}
			break;
		case 0x05: // 5xy0 (SE Vx, Vy): Skip next instruction if Vx = Vy.
			if (chip8->V[chip8->instruction.X] == chip8->V[chip8->instruction.Y]) {
				chip8->PC += 2;
			}
			break;
		case 0x06: // 6xkk (LD Vx, byte): Set Vx = kk.
			chip8->V[chip8->instruction.X] = chip8->instruction.KK;
			break;
		case 0x07: // 7xkk (ADD Vx, byte): Set Vx = Vx + kk.
			chip8->V[chip8->instruction.X] += chip8->instruction.KK;
			break;
		case 0x08:
			switch (chip8->opcode & 0x0F) {
				case 0x00: // 8xy0 (LD Vx, Vy): Stores value of Vy in Vx.
					chip8->V[chip8->instruction.X] = chip8->V[chip8->instruction.Y];
					break;
				case 0x01: // 8xy1 (OR Vx, Vy): Perform bitwise OR on Vx and Vy and store result in Vx.
					chip8->V[chip8->instruction.X] |= chip8->V[chip8->instruction.Y]; 
					break;
				case 0x02: // 8xy2 (AND Vx, Vy): Perform bitwise AND on Vx and Vy and store result in Vx.
					chip8->V[chip8->instruction.X] &= chip8->V[chip8->instruction.Y];
					break;
				case 0x03: // 8xy3 (XOR Vx, Vy): Perform bitwise XOR on Vx and Vy and store the result in Vx.
					chip8->V[chip8->instruction.X] ^= chip8->V[chip8->instruction.Y];
					break;
				case 0x04: // 8xy4 (ADD Vx, Vy): Vx and Vy are added together and sets VF = 1 if the result is greater than 255 (8 bits).
					chip8->V[chip8->instruction.X] += chip8->V[chip8->instruction.Y];
					if (chip8->V[chip8->instruction.X] > 255) {
						chip8->V[0xF] = 1;
					}
					else {
						chip8->V[0xF] = 0;
					}
					break;
				case 0x05: // 8xy5 (SUB Vx, Vy): If Vx > Vy then VF = 1, then subtract Vy from Vx and store result in Vx.
					if (chip8->V[chip8->instruction.X] > chip8->V[chip8->instruction.Y]) {
						chip8->V[0xF] = 1;
					}
					else {
						chip8->V[0xF] = 0;
					}

					chip8->V[chip8->instruction.X] -= chip8->V[chip8->instruction.Y];
					break;
				case 0x06: // 8xy6 (SHR Vx {, Vy}): If least significant bit of Vx is 1, then VF = 1, also divide Vx by 2.
					chip8->V[0xF] = chip8->V[chip8->instruction.X] & 0x01;
					chip8->V[chip8->instruction.X] >>= 1;
					break;
				case 0x0E: // 8xyE (SHL Vx {, Vy}): If most significant bit of Vx is 1, then VF = 1, also multiply Vx by 2
					chip8->V[0xF] = chip8->V[chip8->instruction.X] & 0x01;
					chip8->V[chip8->instruction.X] <<= 1;
					break;
				default:
					printf("Invalid Instruction: 0x%04X\n", chip8->opcode);
					running = false;
					break;
			}
			break;
		case 0x09: // 9xy0 (SNE Vx, Vy): Skip next instruction if Vx != Vy.
			if (chip8->V[chip8->instruction.X] != chip8->V[chip8->instruction.Y]) {
				chip8->PC += 2;
			}
			break;
		case 0x0A: // Annn (LD I, addr): Set I = nnn.
			chip8->I = chip8->instruction.NNN;
			break;
		case 0x0B: // Bnnn (JP V0, addr): Jump to location nnn + V0.
			chip8->PC = chip8->instruction.NNN + chip8->V[0];
			break;
		case 0x0C: // Cxkk (RND Vx, byte): Set Vx = random byte AND kk.
			chip8->V[chip8->instruction.X] = ((rand() % 256) & (chip8->instruction.KK));
			break;
		case 0x0D: // Dxyn (DRW Vx, Vy, niblle): Display n-byte sprite starting at memory location I at (Vx, Vy), set VF = collision.
			// TODO: Just rewrite this
			chip8->V[0xF] = 0; // Reset VF
			uint8_t xCoord = chip8->V[chip8->instruction.X] % 64;
			uint8_t yCoord = chip8->V[chip8->instruction.Y] % 32;

			for (uint8_t i = 0; i < (chip8->opcode & 0x0F); i++) {
				uint8_t byte = chip8->memory[chip8->I + i];

				for (uint8_t j = 0; j < 8; j++) {
					uint8_t bit = ((byte >> (7 - j)) & 0x01);
					uint8_t x = (xCoord + j) % 64;
					uint8_t y = (yCoord + i) % 32;
					uint16_t idx = x + y * 64;

					if (!bit) {
						continue;
					}

					if (chip8->display[idx]) {
						chip8->V[0xF] = 1;
					}

					chip8->display[idx] ^= bit;
				}
			}

			draw = true;
			
			break;
		case 0x0E:
			switch (chip8->opcode & 0x00FF) {
				case 0x9E: // 0xEx9E (SKP Vx): Skip next instruction if key with the value of Vx is pressed.
					if (chip8->keypad[chip8->V[chip8->instruction.X]] == true) {
						chip8->PC += 2;
					}
					break;
				case 0xA1: // 0xExA1 (SKNP Vx): Skip next instruction if key with the value of Vx is not pressed.
					if (chip8->keypad[chip8->V[chip8->instruction.X]] == false) {
						chip8->PC += 2;
					}
					break;
				default:
					printf("Invalid Instruction: 0x%04X\n", chip8->opcode);
					running = false;
					break;
			}
			break;
		case 0x0F:
			switch (chip8->opcode & 0x00FF) {
				case 0x07: // Fx07 (LD Vx, DT): Set Vx = delay timer value.
					chip8->V[chip8->instruction.X] = chip8->delayTimer;
					break;
				case 0x15: // Fx15 (LD DT, Vx): Set delay timer = Vx.
					chip8->delayTimer = chip8->V[chip8->instruction.X];
					break;
				case 0x1E: // Fx1E (ADD I, Vx): Set I = I + Vx
					chip8->I += chip8->V[chip8->instruction.X];
					break;
				case 0x33: // Fx33 (LD B, Vx): 
					uint8_t value = chip8->V[chip8->instruction.X];

					chip8->memory[chip8->I] = value / 100;
					chip8->memory[chip8->I + 1] = (value / 10) % 10;
					chip8->memory[chip8->I + 2] = value % 10;
					break;
				case 0x55: // Fx55 (LD [I], Vx): Store registers V0 through Vx in memory starting at location I.
					{
					uint8_t x = chip8->instruction.X;
					for (uint8_t i = 0; i <= x; i++) {
						chip8->memory[chip8->I + i] = chip8->V[i];
					}
					break;
					}
				case 0x65: // Fx65 (LD Vx, [I]): Read registers V0 through Vx from memory starting at location I.
					{
					uint8_t x = chip8->instruction.X;
					for (uint8_t i = 0; i <= x; i++) {
						chip8->V[i] = chip8->memory[chip8->I + i];
					}
					break;
					}
				default:
					printf("Invalid Instruction: 0x%04X\n", chip8->opcode);
					running = false;
					break;
			}
			break;
		default:
			printf("Invalid Instruction: 0x%04X\n", chip8->opcode);
			running = false;
			break;
		break;
	}
}


int main(int argc, char **argv) {
	if (argc != 2) {
		printf("Usage: %s <ROM File>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	chip8_t chip8 = { 0 };

	// Initialize chip
	if (!chipInit(&chip8, argv[1])) {
		printf("Error\n");
		exit(EXIT_FAILURE);
	}

	SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");

	// Init SDL
	if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
		printf("Error initializing SDL: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	// Create window
	SDL_Window *window = SDL_CreateWindow(
		"Chip8",
		64 * SCALE,
		32 * SCALE,
		0
	);

	if (window == NULL) {
		printf("Error creating window: %s\n", SDL_GetError());
		SDL_Quit();
		exit(EXIT_FAILURE);
	}

	// Create Renderer
	SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
	if (renderer == NULL) {
		printf("Error initializing renderer: %s\n", SDL_GetError());
		SDL_Quit();
		exit(EXIT_FAILURE);
	}

	while (running) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				running = false;
			}

			if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
				// Is key pressed or released?
				bool pressed = (event.type == SDL_EVENT_KEY_DOWN);
				switch (event.key.key) {
					case SDLK_ESCAPE:
						if (pressed) {
							running = false;
						}
						break;
					case SDLK_1:
						chip8.keypad[0x1] = pressed;
						break;
					case SDLK_2:
						chip8.keypad[0x2] = pressed;
						break;
					case SDLK_3:
						chip8.keypad[0x3] = pressed;
						break;
					case SDLK_4:
						chip8.keypad[0xC] = pressed;
						break;
					case SDLK_Q:
						chip8.keypad[0x4] = pressed;
						break;
					case SDLK_W:
						chip8.keypad[0x5] = pressed;
						break;
					case SDLK_E:
						chip8.keypad[0x6] = pressed;
						break;
					case SDLK_R:
						chip8.keypad[0xD] = pressed;
						break;
					case SDLK_A:
						chip8.keypad[0x7] = pressed;
						break;
					case SDLK_S:
						chip8.keypad[0x8] = pressed;
						break;
					case SDLK_D:
						chip8.keypad[0x9] = pressed;
						break;
					case SDLK_F:
						chip8.keypad[0xE] = pressed;
						break;
					case SDLK_Z:
						chip8.keypad[0xA] = pressed;
						break;
					case SDLK_X:
						chip8.keypad[0x0] = pressed;
						break;
					case SDLK_C:
						chip8.keypad[0xB] = pressed;
						break;
					case SDLK_V:
						chip8.keypad[0xF] = pressed;
						break;
					default:
						break;
				}
			}
		}

		execute(&chip8);

		if (draw) {
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			SDL_RenderClear(renderer);

			SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
			for (int y = 0; y < 32; y++) {
				for (int x = 0; x < 64; x++) {
					if (chip8.display[x + y * 64]) {
						SDL_FRect rect = {x * SCALE, y * SCALE, SCALE, SCALE};
						SDL_RenderFillRect(renderer, &rect);
					}
				}
			}
			SDL_RenderPresent(renderer);
			draw = false;
		}
		SDL_Delay(3);
	}
	
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
