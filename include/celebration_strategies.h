#ifndef _CELEBRATION_STRATEGIES_H
#define _CELEBRATION_STRATEGIES_H

#include "swaylock.h"

// Strategy initialization function signature
typedef void (*strategy_init_fn)(struct celebration_strategy *strategy,
                                   int screen_width, int screen_height);

// Available strategies
void init_jackpot_burst_strategy(struct celebration_strategy *strategy,
                                   int screen_width, int screen_height);
void init_corner_chaos_strategy(struct celebration_strategy *strategy,
                                  int screen_width, int screen_height);
void init_fountain_strategy(struct celebration_strategy *strategy,
                             int screen_width, int screen_height);
void init_diagonal_test_strategy(struct celebration_strategy *strategy,
                                   int screen_width, int screen_height);

// Cleanup function
void cleanup_strategy(struct celebration_strategy *strategy);

#endif
