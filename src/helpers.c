// src/helpers.c

#include "shared.h"
#include <math.h>

// Sinc function: sin(pi*x) / (pi*x)
double sinc(double x) {
    if (fabs(x) < 1e-9) {
        return 1.0; // Limit of sinc(x) as x -> 0 is 1.0
    }
    return sin(M_PI * x) / (M_PI * x);
}

// Raised Cosine pulse shaping function
double raised_cosine(double t, double T_s, double beta) {
    if (fabs(t) > 4 * T_s) return 0; // Widen window slightly for accuracy

    if (fabs(t) < 1e-9) {
        return 1.0;
    }

    if (beta > 1e-9 && fabs(fabs(2.0 * beta * t / T_s) - 1.0) < 1e-9) {
        return M_PI / 4.0 * sinc(1.0 / (2.0 * beta));
    }

    double term1 = sin(M_PI * t / T_s) / (M_PI * t / T_s);
    double term2 = cos(M_PI * beta * t / T_s) / (1.0 - pow(2.0 * beta * t / T_s, 2.0));
    return term1 * term2;
}

// Fetches the integer value of a symbol from the message buffer
int get_symbol_at_index(int symbol_index, const char* message, int message_len, int bits_per_sym) {
    int start_bit_index = symbol_index * bits_per_sym;
    if ((start_bit_index / 8) >= message_len) return 0;

    int symbol_value = 0;
    for (int i = 0; i < bits_per_sym; ++i) {
        int current_bit_index = start_bit_index + i;
        int char_index = current_bit_index / 8;
        if (char_index >= message_len) continue;

        int bit_in_char = current_bit_index % 8;
        int bit = (message[char_index] >> (7 - bit_in_char)) & 1;
        symbol_value = (symbol_value << 1) | bit;
    }
    return symbol_value;
}