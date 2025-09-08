#ifndef SHARED_H
#define SHARED_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

// --- Shared Enums ---
typedef enum { MODE_TYPING, MODE_COMMAND } AppMode;
typedef enum { MOD_ASK, MOD_FSK, MOD_PSK } ModulationType;
typedef enum { VIEW_TIME_DOMAIN, VIEW_IQ_PLOT , VIEW_POWER_SPECTRUM } ViewMode;
typedef enum { WINDOW_HANN, WINDOW_HAMMING, WINDOW_RECTANGULAR } WindowType;

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
extern double spectrum_center_freq;
extern double spectrum_span;
extern int fft_size;
extern WindowType current_window_type;
extern int spectrum_power;
extern double hovered_frequency;
extern double hovered_power;
extern int mouse_x;
extern int mouse_y;

// --- Shared Helper Function Prototypes ---
int get_symbol_at_index(int symbol_index, const char* message, int message_len, int bits_per_sym);
double sinc(double x);
double raised_cosine(double t, double T_s, double beta);

#endif // SHARED_H