#include <assert.h>
#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>
#include "comm.h"
#include "log.h"
#include "loop.h"
#include "seat.h"
#include "swaylock.h"
#include "unicode.h"
#include "celebration_strategies.h"

// Forward declarations
static void cancel_animation(struct swaylock_state *state);
static void schedule_animation(struct swaylock_state *state);

// Active strategy instance
static struct celebration_strategy current_strategy = {0};

static void start_celebration(struct swaylock_state *state, int screen_width, int screen_height) {
	// Choose strategy - CHANGE THIS TO TRY DIFFERENT STRATEGIES!
	// Options:
	//   init_jackpot_burst_strategy     - 40 emojis explode from center
	//   init_corner_chaos_strategy      - Emojis from all 4 corners
	//   init_fountain_strategy           - Continuous upward stream
	//   init_diagonal_test_strategy      - Simple diagonal (original)

	init_jackpot_burst_strategy(&current_strategy, screen_width, screen_height);

	// Reset particle system
	state->celebration_particles.strategy = &current_strategy;
	state->celebration_particles.current_frame = 0;
	state->celebration_particles.active = true;
	state->celebration_particles.active_count = 0;

	// Clear all particles
	for (int i = 0; i < MAX_PARTICLES; i++) {
		state->celebration_particles.particles[i].active = false;
	}

	// Start animation timer
	schedule_animation(state);
}

void clear_buffer(char *buf, size_t size) {
	// Use volatile keyword so so compiler can't optimize this out.
	volatile char *buffer = buf;
	volatile char zero = '\0';
	for (size_t i = 0; i < size; ++i) {
		buffer[i] = zero;
	}
}

void clear_password_buffer(struct swaylock_password *pw) {
	clear_buffer(pw->buffer, pw->buffer_len);
	pw->len = 0;
}

static bool backspace(struct swaylock_password *pw) {
	if (pw->len != 0) {
		pw->len -= utf8_last_size(pw->buffer);
		pw->buffer[pw->len] = 0;
		return true;
	}
	return false;
}

static void append_ch(struct swaylock_password *pw, uint32_t codepoint) {
	size_t utf8_size = utf8_chsize(codepoint);
	if (pw->len + utf8_size + 1 >= pw->buffer_len) {
		// TODO: Display error
		return;
	}
	utf8_encode(&pw->buffer[pw->len], codepoint);
	pw->buffer[pw->len + utf8_size] = 0;
	pw->len += utf8_size;
}

static void set_input_idle(void *data) {
	struct swaylock_state *state = data;
	state->input_idle_timer = NULL;
	state->input_state = INPUT_STATE_IDLE;
	damage_state(state);
}

static void set_auth_idle(void *data) {
	struct swaylock_state *state = data;
	state->auth_idle_timer = NULL;
	state->auth_state = AUTH_STATE_IDLE;
	damage_state(state);
}

static void schedule_input_idle(struct swaylock_state *state) {
	if (state->input_idle_timer) {
		loop_remove_timer(state->eventloop, state->input_idle_timer);
	}
	state->input_idle_timer = loop_add_timer(
		state->eventloop, 1500, set_input_idle, state);
}

static void cancel_input_idle(struct swaylock_state *state) {
	if (state->input_idle_timer) {
		loop_remove_timer(state->eventloop, state->input_idle_timer);
		state->input_idle_timer = NULL;
	}
}

void schedule_auth_idle(struct swaylock_state *state) {
	if (state->auth_idle_timer) {
		loop_remove_timer(state->eventloop, state->auth_idle_timer);
	}
	state->auth_idle_timer = loop_add_timer(
		state->eventloop, 3000, set_auth_idle, state);
}

static void clear_password(void *data) {
	struct swaylock_state *state = data;
	state->clear_password_timer = NULL;
	state->input_state = INPUT_STATE_CLEAR;
	schedule_input_idle(state);
	clear_password_buffer(&state->password);
	state->has_emojis = false;
	state->has_old_emojis = false;
	state->emoji_animating = false;
	cancel_animation(state);
	damage_state(state);
}

static void schedule_password_clear(struct swaylock_state *state) {
	if (state->clear_password_timer) {
		loop_remove_timer(state->eventloop, state->clear_password_timer);
	}
	state->clear_password_timer = loop_add_timer(
			state->eventloop, 10000, clear_password, state);
}

static void cancel_password_clear(struct swaylock_state *state) {
	if (state->clear_password_timer) {
		loop_remove_timer(state->eventloop, state->clear_password_timer);
		state->clear_password_timer = NULL;
	}
}

static void animation_tick(void *data) {
	struct swaylock_state *state = data;
	state->animation_timer = NULL;

	if (state->emoji_animating || state->has_old_emojis ||
	    state->celebration_particles.active || state->celebration_particles.active_count > 0) {
		damage_state(state);
		// Schedule next frame (16ms = ~60 FPS)
		state->animation_timer = loop_add_timer(
			state->eventloop, 16, animation_tick, state);
	}
}

static void schedule_animation(struct swaylock_state *state) {
	if (state->animation_timer) {
		loop_remove_timer(state->eventloop, state->animation_timer);
	}
	state->animation_timer = loop_add_timer(
		state->eventloop, 16, animation_tick, state);
}

static void cancel_animation(struct swaylock_state *state) {
	if (state->animation_timer) {
		loop_remove_timer(state->eventloop, state->animation_timer);
		state->animation_timer = NULL;
	}
}

static void submit_password(struct swaylock_state *state) {
	if (state->args.ignore_empty && state->password.len == 0) {
		return;
	}
	if (state->auth_state == AUTH_STATE_VALIDATING) {
		return;
	}

	state->input_state = INPUT_STATE_IDLE;
	state->auth_state = AUTH_STATE_VALIDATING;
	cancel_password_clear(state);
	cancel_input_idle(state);

	if (!write_comm_request(&state->password)) {
		state->auth_state = AUTH_STATE_INVALID;
		schedule_auth_idle(state);
	}

	damage_state(state);
}

static void update_highlight(struct swaylock_state *state) {
	// Advance a random amount between 1/4 and 3/4 of a full turn
	state->highlight_start =
		(state->highlight_start + (rand() % 1024) + 512) % 2048;
}

static void randomize_slot_emojis(struct swaylock_state *state) {
	// Emoji set: Cherry 🍒, Peach 🍑, Star ⭐
	const char *emojis[3] = {
		"\xF0\x9F\x8D\x92", // 🍒 Cherry
		"\xF0\x9F\x8D\x91", // 🍑 Peach
		"\xE2\xAD\x90"      // ⭐ Star
	};

	// Save old emojis to fall off screen
	if (state->has_emojis) {
		for (int i = 0; i < 3; i++) {
			strcpy(state->old_slot_emojis[i], state->slot_emojis[i]);
			state->old_emoji_y_positions[i] = state->emoji_y_positions[i];
		}
		state->has_old_emojis = true;
	}

	// Randomize all 3 slots
	for (int i = 0; i < 3; i++) {
		int emoji_idx = rand() % 3;
		strcpy(state->slot_emojis[i], emojis[emoji_idx]);
		// Start emojis at different heights above screen
		state->emoji_y_positions[i] = -100.0 - (i * 30.0);
	}

	state->has_emojis = true;
	state->emoji_animating = true;
	state->emoji_target_y = 0.0; // Will be set properly during render
	schedule_animation(state);
}

void swaylock_handle_key(struct swaylock_state *state,
		xkb_keysym_t keysym, uint32_t codepoint) {

	switch (keysym) {
	case XKB_KEY_KP_Enter: /* fallthrough */
	case XKB_KEY_Return:
		submit_password(state);
		break;
	case XKB_KEY_Delete:
	case XKB_KEY_BackSpace:
		if (state->xkb.control) {
			clear_password_buffer(&state->password);
			state->input_state = INPUT_STATE_CLEAR;
			cancel_password_clear(state);
			state->has_emojis = false;
			state->has_old_emojis = false;
			state->emoji_animating = false;
			cancel_animation(state);
		} else {
			if (backspace(&state->password) && state->password.len != 0) {
				state->input_state = INPUT_STATE_BACKSPACE;
				schedule_password_clear(state);
				update_highlight(state);
			} else {
				state->input_state = INPUT_STATE_CLEAR;
				cancel_password_clear(state);
			}
		}
		schedule_input_idle(state);
		damage_state(state);
		break;
	case XKB_KEY_Escape:
		clear_password_buffer(&state->password);
		state->input_state = INPUT_STATE_CLEAR;
		cancel_password_clear(state);
		state->has_emojis = false;
		state->has_old_emojis = false;
		state->emoji_animating = false;
		cancel_animation(state);
		schedule_input_idle(state);
		damage_state(state);
		break;
	case XKB_KEY_Caps_Lock:
	case XKB_KEY_Shift_L:
	case XKB_KEY_Shift_R:
	case XKB_KEY_Control_L:
	case XKB_KEY_Control_R:
	case XKB_KEY_Meta_L:
	case XKB_KEY_Meta_R:
	case XKB_KEY_Alt_L:
	case XKB_KEY_Alt_R:
	case XKB_KEY_Super_L:
	case XKB_KEY_Super_R:
		state->input_state = INPUT_STATE_NEUTRAL;
		schedule_password_clear(state);
		schedule_input_idle(state);
		damage_state(state);
		break;
	case XKB_KEY_m: /* fallthrough */
	case XKB_KEY_d:
	case XKB_KEY_j:
		if (state->xkb.control) {
			submit_password(state);
			break;
		}
		// fallthrough
	case XKB_KEY_c: /* fallthrough */
	case XKB_KEY_u:
		if (state->xkb.control) {
			clear_password_buffer(&state->password);
			state->input_state = INPUT_STATE_CLEAR;
			cancel_password_clear(state);
			state->has_emojis = false;
			state->has_old_emojis = false;
			state->emoji_animating = false;
			cancel_animation(state);
			schedule_input_idle(state);
			damage_state(state);
			break;
		}
		// fallthrough
	case XKB_KEY_space:
		// TEST TRIGGER: Spacebar triggers celebration animation
		if (state->xkb.control) {
			start_celebration(state, 1920, 1080);
			damage_state(state);
			break;
		}
		/* fallthrough */
	default:
		if (codepoint) {
			append_ch(&state->password, codepoint);
			state->input_state = INPUT_STATE_LETTER;
			schedule_password_clear(state);
			schedule_input_idle(state);
			update_highlight(state);
			randomize_slot_emojis(state);
			damage_state(state);
		}
		break;
	}
}
