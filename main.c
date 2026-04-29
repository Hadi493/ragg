#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define WIDTH 1000
#define HEIGHT 1000

// --- Simplified and Compact Perlin Noise Implementation ---
typedef struct {
    int p[512];
} PerlinState;

// A standard, scaled-down Ken Perlin permutation (about 128 elements) for compact single-file use.
// This is sufficient for quality output.
static const int permutation[] = {
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,
    114,143,158,235,162,192,108,66,251,34,32,232,152,217,165,116,29,193,222,236,117,148,
    31,239,123,124,198,125,122,38,111,210,13,98,44,110,4,165,61,228,203,241,135,216,
    115,178,46,110,166,115,131,131,166,128,151,146,160,251,178,255,246
};

void perlin_init(PerlinState *ps, int seed) {
    srand(seed);
    int p[256];
    int num_perms = sizeof(permutation) / sizeof(permutation[0]);
    // Expand to 256 for basic use and repeat
    for (int i = 0; i < 256; i++) p[i] = permutation[i % num_perms];
    // Randomly shuffle to ensure variety, then double to avoid overflow
    for (int i = 0; i < 256; i++) {
        int r = rand() % 256;
        int t = p[i];
        p[i] = p[r];
        p[r] = t;
    }
    for (int i = 0; i < 256; i++) {
        ps->p[i] = ps->p[i+256] = p[i];
    }
}

// Fade function as defined by Perlin
static float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
// Standard linear interpolation
static float lerp(float t, float a, float b) { return a + t * (b - a); }
// Gradient function for 2D noise
static float grad(int hash, float x, float y) {
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : h == 12 || h == 14 ? x : 0.0f;
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

float noise(PerlinState *ps, float x, float y) {
    int X = (int)floorf(x) & 255;
    int Y = (int)floorf(y) & 255;
    x -= floorf(x);
    y -= floorf(y);
    float u = fade(x);
    float v = fade(y);

    int A = ps->p[X    ] + Y;
    int B = ps->p[X + 1] + Y;

    return lerp(v, lerp(u, grad(ps->p[ps->p[A    ]], x, y), grad(ps->p[ps->p[B    ]], x - 1.0f, y)),
                   lerp(u, grad(ps->p[ps->p[A + 1]], x, y - 1.0f), grad(ps->p[ps->p[B + 1]], x - 1.0f, y - 1.0f)));
}

// Fractal Brownian Motion (layered noise)
float fBm(PerlinState *ps, float x, float y, int octaves) {
    float value = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;
    for (int i = 0; i < octaves; i++) {
        value += amplitude * noise(ps, x * frequency, y * frequency);
        frequency *= 2.0f;
        amplitude *= 0.5f;
    }
    return value;
}

// --- High-End, Non-Clashing Fluid Color Palette Engine ---
typedef struct { float r, g, b; } Color;

// Standard RGB interpolation with a beautiful function
Color mix_colors(Color a, Color b, float t) {
    // Custom beautiful smooth interpolation (fade-based) to remove linear artifacts
    float smooth_t = fade(t);
    Color res = {
        lerp(smooth_t, a.r, b.r),
        lerp(smooth_t, a.g, b.g),
        lerp(smooth_t, a.b, b.b)
    };
    return res;
}

// Deeply stirred color ramp function (0-1 input value -> Color)
Color get_ramp_color(float t) {
    // A sophisticated multi-stop ramp mixing analogous and complementary tones (Ref 8 & 9)
    // Deep Indigo -> Cyan -> Green -> Gold -> Orange -> Magenta -> Purple -> Blue
    Color stops[] = {
        {0.1f, 0.0f, 0.2f},  // Deep Indigo
        {0.0f, 0.8f, 0.7f},  // Bright Cyan
        {0.4f, 0.9f, 0.1f},  // Lime Green
        {0.9f, 0.8f, 0.0f},  // Pure Gold
        {1.0f, 0.5f, 0.0f},  // True Orange
        {1.0f, 0.0f, 0.5f},  // Hot Magenta
        {0.5f, 0.0f, 0.8f},  // Deep Purple
        {0.0f, 0.1f, 0.3f},  // Mid Blue
    };
    int num_stops = sizeof(stops) / sizeof(stops[0]);

    // Simple smooth selection logic (no smoothstep here to compressed stops if needed, keep linear stop selection for simplicity but use the smooth fading above)
    t = fmodf(t, 1.0f);
    if (t < 0.0f) t += 1.0f;
    float scaled_t = t * (num_stops - 1);
    int stop_index = (int)floorf(scaled_t);
    float local_t = scaled_t - stop_index;

    Color c1 = stops[stop_index];
    Color c2 = stops[(stop_index + 1) % num_stops];

    return mix_colors(c1, c2, local_t);
}

// Final color correction to fit the screen
unsigned char to_byte(float x) {
    if (x < 0) return 0;
    if (x > 1) return 255;
    return (unsigned char)(x * 255.0f);
}

// --- Main Fluid Abstract Generator Program ---
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <output.ppm>\n", argv[0]);
        return 1;
    }

    // Seed randomness uniquely every single run for 1M+ results
    unsigned int seed = (unsigned int)time(NULL);
    srand(seed);

    FILE *fp = fopen(argv[1], "wb");
    if (!fp) return 1;

    // PPM Header (binary)
    fprintf(fp, "P6\n%d %d\n255\n", WIDTH, HEIGHT);

    PerlinState perlin;
    perlin_init(&perlin, seed);

    float scale = 4.0f;
    float aspect = (float)WIDTH / (float)HEIGHT;

    // Random phase, amplitude, and frequency for large sweeping waves
    float phase_u_wave = (rand() % 1000) / 1000.0f * 2.0f * M_PI;
    float phase_v_wave = (rand() % 1000) / 1000.0f * 2.0f * M_PI;
    float wave_amp_u = (rand() % 1000) / 2000.0f + 0.1f;  // 0.1 to 0.6
    float wave_amp_v = (rand() % 1000) / 2000.0f + 0.1f;  // 0.1 to 0.6
    float wave_freq_u = (rand() % 1000) / 500.0f + 1.0f;  // 1.0 to 3.0
    float wave_freq_v = (rand() % 1000) / 500.0f + 1.0f;  // 1.0 to 3.0

    // Randomize fine-detail noise folds (domain warping logic)
    float warp_amp_u = (rand() % 1000) / 200.0f + 0.05f; // 0.05 to 0.55
    float warp_amp_v = (rand() % 1000) / 200.0f + 0.05f; // 0.05 to 0.55
    float warp_freq_u = (rand() % 1000) / 250.0f + 1.0f; // 1.0 to 5.0
    float warp_freq_v = (rand() % 1000) / 250.0f + 1.0f; // 1.0 to 5.0

    // Random grain strength for the final graphic design texture
    float grain_strength = (rand() % 1000) / 100.0f; // 0.0 to 10.0 (very subtle texture)

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            float u = ((float)x / WIDTH) * aspect;
            float v = ((float)y / HEIGHT);

            // Screen Space domain warping to create fluid flow and deep stirring folds (Ref 8 & 9 combined feel)

            // 1. Large, sweeping base waves using sin/cos logic
            float dx_base = cosf(v * 2.0f * wave_freq_v + phase_v_wave) * wave_amp_u;
            float dy_base = sinf(u * 2.0f * wave_freq_u + phase_u_wave) * wave_amp_v;

            // 2. Controlled micro-folds using layered noise
            float dx_warp = fBm(&perlin, u * warp_freq_u, v * warp_freq_v, 2) * warp_amp_u;
            float dy_warp = fBm(&perlin, u * warp_freq_u + 10.0f, v * warp_freq_v + 10.0f, 2) * warp_amp_v;

            // Apply both warping vectors to grid coordinates to deeply stir the space
            float uw = u + dx_base + dx_warp;
            float vw = v + dy_base + dy_warp;

            // 3. Sample a high-detail noise field with the deeply warped coordinates
            float f = fBm(&perlin, uw * scale, vw * scale, 6);

            // Create a single combined input value for the beautiful color ramp
            // Combine Density, Warp_u, and Warp_v logic for complex color follow (Ref 8 style linear flow)
            float input = (uw + vw + f) / 3.0f;

            Color col = get_ramp_color(input);

            // Add Graphic Design "Film Grain" (subtle and beautiful modification)
            float g_mod = 1.0f + ((float)rand() / (float)RAND_MAX - 0.5f) * grain_strength / 100.0f;
            col.r *= g_mod; col.g *= g_mod; col.b *= g_mod;

            fputc(to_byte(col.r), fp);
            fputc(to_byte(col.g), fp);
            fputc(to_byte(col.b), fp);
        }
    }

    fclose(fp);
    printf("Fluid Abstract generated. Variation: %u\n", seed);
    return 0;
}
