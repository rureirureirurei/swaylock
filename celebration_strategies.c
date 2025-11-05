#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "swaylock.h"
#include "celebration_strategies.h"

#define M_PI 3.14159265358979323846

// Storage for dynamically generated strategies
static struct particle_spawn_def *allocated_defs = NULL;

void cleanup_strategy(struct celebration_strategy *strategy) {
	if (allocated_defs) {
		free(allocated_defs);
		allocated_defs = NULL;
	}
	strategy->spawn_defs = NULL;
	strategy->particle_count = 0;
}

// ============================================================================
// STRATEGY 1: Jackpot Burst - Explosion from center
// ============================================================================
void init_jackpot_burst_strategy(struct celebration_strategy *strategy,
                                   int screen_width, int screen_height) {
	cleanup_strategy(strategy);

	int particle_count = 40;
	allocated_defs = malloc(sizeof(struct particle_spawn_def) * particle_count);

	double center_x = screen_width / 2.0;
	double center_y = screen_height / 2.0;

	// Emojis for jackpot
	const char *jackpot_emojis[] = {
		"\xF0\x9F\x8D\x92", // 🍒 Cherry
		"\xF0\x9F\x92\x8E", // 💎 Diamond
		"\xE2\xAD\x90",     // ⭐ Star
	};

	for (int i = 0; i < particle_count; i++) {
		// Evenly distributed around circle
		double angle = (i / (double)particle_count) * 2 * M_PI;

		// Random speed variation
		double speed = 400 + (rand() % 400); // 400-800 px/s

		allocated_defs[i].spawn_x = center_x;
		allocated_defs[i].spawn_y = center_y;
		allocated_defs[i].velocity_x = cos(angle) * speed;
		allocated_defs[i].velocity_y = sin(angle) * speed;
		allocated_defs[i].rotation_speed = ((rand() % 8) - 4); // -4 to +4 rad/s
		allocated_defs[i].spawn_frame = 0; // All spawn at once

		// Mostly cherries, some diamonds/stars
		int emoji_choice = rand() % 10;
		if (emoji_choice < 7) {
			strcpy(allocated_defs[i].emoji, jackpot_emojis[0]); // Cherry
		} else if (emoji_choice < 9) {
			strcpy(allocated_defs[i].emoji, jackpot_emojis[1]); // Diamond
		} else {
			strcpy(allocated_defs[i].emoji, jackpot_emojis[2]); // Star
		}
	}

	strategy->spawn_defs = allocated_defs;
	strategy->particle_count = particle_count;
	strategy->total_frames = 300; // 5 seconds at 60fps
}

// ============================================================================
// STRATEGY 2: Corner Chaos - Emojis from all 4 corners
// ============================================================================
void init_corner_chaos_strategy(struct celebration_strategy *strategy,
                                  int screen_width, int screen_height) {
	cleanup_strategy(strategy);

	int particles_per_corner = 8;
	int particle_count = particles_per_corner * 4;
	allocated_defs = malloc(sizeof(struct particle_spawn_def) * particle_count);

	const char *emoji = "\xF0\x9F\x8D\x92"; // Cherry

	// Four corners
	double corners[4][2] = {
		{0, 0},                              // Top-left
		{screen_width, 0},                   // Top-right
		{0, screen_height},                  // Bottom-left
		{screen_width, screen_height}        // Bottom-right
	};

	int idx = 0;
	for (int corner = 0; corner < 4; corner++) {
		for (int i = 0; i < particles_per_corner; i++) {
			// Aim towards center with variation
			double target_x = screen_width / 2.0 + (rand() % 200 - 100);
			double target_y = screen_height / 2.0 + (rand() % 200 - 100);

			double dx = target_x - corners[corner][0];
			double dy = target_y - corners[corner][1];
			double dist = sqrt(dx*dx + dy*dy);
			double speed = 500 + (rand() % 300);

			allocated_defs[idx].spawn_x = corners[corner][0];
			allocated_defs[idx].spawn_y = corners[corner][1];
			allocated_defs[idx].velocity_x = (dx / dist) * speed;
			allocated_defs[idx].velocity_y = (dy / dist) * speed;
			allocated_defs[idx].rotation_speed = ((rand() % 6) - 3);
			allocated_defs[idx].spawn_frame = i * 3; // Stagger by 3 frames
			strcpy(allocated_defs[idx].emoji, emoji);
			idx++;
		}
	}

	strategy->spawn_defs = allocated_defs;
	strategy->particle_count = particle_count;
	strategy->total_frames = 300;
}

// ============================================================================
// STRATEGY 3: Fountain - Continuous upward stream
// ============================================================================
void init_fountain_strategy(struct celebration_strategy *strategy,
                             int screen_width, int screen_height) {
	cleanup_strategy(strategy);

	int particle_count = 60;
	allocated_defs = malloc(sizeof(struct particle_spawn_def) * particle_count);

	double spawn_x = screen_width / 2.0;
	double spawn_y = screen_height; // Bottom of screen

	const char *emoji = "\xF0\x9F\x8D\x92"; // Cherry

	for (int i = 0; i < particle_count; i++) {
		// Upward with slight horizontal variation
		double vx = (rand() % 200) - 100; // -100 to +100
		double vy = -(600 + (rand() % 400)); // -600 to -1000 (upward)

		allocated_defs[i].spawn_x = spawn_x + (rand() % 40 - 20);
		allocated_defs[i].spawn_y = spawn_y;
		allocated_defs[i].velocity_x = vx;
		allocated_defs[i].velocity_y = vy;
		allocated_defs[i].rotation_speed = ((rand() % 8) - 4);
		allocated_defs[i].spawn_frame = i * 2; // One every 2 frames
		strcpy(allocated_defs[i].emoji, emoji);
	}

	strategy->spawn_defs = allocated_defs;
	strategy->particle_count = particle_count;
	strategy->total_frames = 240; // 4 seconds
}

// ============================================================================
// STRATEGY 4: Diagonal Test - Simple diagonal (the original)
// ============================================================================
void init_diagonal_test_strategy(struct celebration_strategy *strategy,
                                   int screen_width, int screen_height) {
	cleanup_strategy(strategy);

	int particle_count = 5;
	allocated_defs = malloc(sizeof(struct particle_spawn_def) * particle_count);

	const char *emoji = "\xF0\x9F\x8D\x92"; // Cherry

	for (int i = 0; i < particle_count; i++) {
		allocated_defs[i].spawn_x = 50.0;
		allocated_defs[i].spawn_y = 50.0;
		allocated_defs[i].velocity_x = 400.0;
		allocated_defs[i].velocity_y = 200.0;
		allocated_defs[i].rotation_speed = 3.0;
		allocated_defs[i].spawn_frame = i * 5; // Every 5 frames
		strcpy(allocated_defs[i].emoji, emoji);
	}

	strategy->spawn_defs = allocated_defs;
	strategy->particle_count = particle_count;
	strategy->total_frames = 180;
}
