#ifndef SHARED_H
#define SHARED_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

// --- Shared Enums ---
typedef enum { MODE_TYPING, MODE_COMMAND } AppMode;
typedef enum { MOD_ASK, MOD_FSK, MOD_PSK } ModulationType;
typedef enum { VIEW_TIME_DOMAIN, VIEW_IQ_PLOT , VIEW_POWER_SPECTRUM } ViewMode;

// --- Extern Global Variable Declarations ---
extern int SCREEN_WIDTH;
extern int SCREEN_HEIGHT;
extern double amplitude;
extern double frequency;
extern int pixelsPerBit;
extern double snr_db;
extern double rolloff_factor;
extern int bitsPerSymbol;
extern double time_offset;
extern double sampling_rate;
extern double pixels_per_second;

// --- Shared Helper Function Prototypes ---
int get_symbol_at_index(int symbol_index, const char* message, int message_len, int bits_per_sym);
double sinc(double x);
double raised_cosine(double t, double T_s, double beta);

#endif // SHARED_H