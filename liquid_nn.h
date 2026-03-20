#ifndef LIQUID_NN_H
#define LIQUID_NN_H

#include "types.h"

#define NUM_NEURONS 8
#define INPUT_SIZE 3
#define FIXED_POINT_SCALE 1024  // 10 бит после запятой

typedef long fxp_t;

// Макросы для fixed-point
#define FXP_MUL(a, b) (((a) * (b)) / FIXED_POINT_SCALE)
#define FXP_DIV(a, b) (((a) * FIXED_POINT_SCALE) / (b))
#define FXP_FROM_FLOAT(f) ((fxp_t)((f) * FIXED_POINT_SCALE))

// Функции
void liquid_init(void);
void liquid_step(const fxp_t input[INPUT_SIZE]);
void update_weights_liquid(const fxp_t input[INPUT_SIZE], fxp_t target);
fxp_t liquid_output(void);
void kernel_liquid_predict(uint32_t pid, fxp_t last_time, fxp_t sys_freq, uint8_t is_active);

#endif
