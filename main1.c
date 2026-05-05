#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

typedef struct {
    uint64_t state;
} xorshift64_t;

static inline uint64_t xorshift64_next(xorshift64_t *rng) {
    uint64_t x = rng->state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    rng->state = x;
    return x;
}

static inline float xorshift64_float(xorshift64_t *rng) {
    return (xorshift64_next(rng) >> 11) * (1.0f / 9007199254740992.0f);
}

#define NOISE_TABLE_SIZE 256
typedef struct {
    int perm[512];
    float grad[256][2];
} perlin_noise_t;

static float fade(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static float grad_dot(float *grad, float x, float y) {
    return grad[0] * x + grad[1] * y;
}

static void perlin_init(perlin_noise_t *noise, uint64_t seed) {
    xorshift64_t rng = {seed};

    for (int i = 0; i < 256; i++) {
        noise->perm[i] = i;
        float angle = 2.0f * 3.14159265f * xorshift64_float(&rng);
        noise->grad[i][0] = cosf(angle);
        noise->grad[i][1] = sinf(angle);
    }

    for (int i = 255; i > 0; i--) {
        int j = xorshift64_next(&rng) % (i + 1);
        int tmp = noise->perm[i];
        noise->perm[i] = noise->perm[j];
        noise->perm[j] = tmp;
    }

    for (int i = 0; i < 256; i++) {
        noise->perm[256 + i] = noise->perm[i];
    }
}

static float perlin_2d(perlin_noise_t *noise, float x, float y) {
    int xi = (int)floorf(x) & 255;
    int yi = (int)floorf(y) & 255;

    float xf = x - floorf(x);
    float yf = y - floorf(y);

    float u = fade(xf);
    float v = fade(yf);

    int aa = noise->perm[noise->perm[xi] + yi];
    int ab = noise->perm[noise->perm[xi] + yi + 1];
    int ba = noise->perm[noise->perm[xi + 1] + yi];
    int bb = noise->perm[noise->perm[xi + 1] + yi + 1];

    float g_aa = grad_dot(noise->grad[aa], xf, yf);
    float g_ba = grad_dot(noise->grad[ba], xf - 1.0f, yf);
    float g_ab = grad_dot(noise->grad[ab], xf, yf - 1.0f);
    float g_bb = grad_dot(noise->grad[bb], xf - 1.0f, yf - 1.0f);

    float x1 = lerp(g_aa, g_ba, u);
    float x2 = lerp(g_ab, g_bb, u);

    return lerp(x1, x2, v);
}

static void hsv_to_rgb(float h, float s, float v, uint8_t *r, uint8_t *g, uint8_t *b) {
    float c = v * s;
    float hp = fmodf(h * 6.0f, 6.0f);
    float x = c * (1.0f - fabsf(fmodf(hp, 2.0f) - 1.0f));
    float m = v - c;

    float rf, gf, bf;
    if (hp < 1.0f) { rf = c; gf = x; bf = 0.0f; }
    else if (hp < 2.0f) { rf = x; gf = c; bf = 0.0f; }
    else if (hp < 3.0f) { rf = 0.0f; gf = c; bf = x; }
    else if (hp < 4.0f) { rf = 0.0f; gf = x; bf = c; }
    else if (hp < 5.0f) { rf = x; gf = 0.0f; bf = c; }
    else { rf = c; gf = 0.0f; bf = x; }

    *r = (uint8_t)((rf + m) * 255.0f);
    *g = (uint8_t)((gf + m) * 255.0f);
    *b = (uint8_t)((bf + m) * 255.0f);
}


typedef struct {
    float x, y;
    float radius;
    float falloff;
    float hue;
    float saturation;
    float brightness;
} gradient_layer_t;

typedef struct {
    int width, height;
    int num_layers;
    gradient_layer_t *layers;
    perlin_noise_t noise;
    xorshift64_t rng;
} image_generator_t;


static void generate_image(image_generator_t *gen, uint8_t *pixels) {
    for (int py = 0; py < gen->height; py++) {
        for (int px = 0; px < gen->width; px++) {
            float r = 0.0f, g = 0.0f, b = 0.0f;
            float total_weight = 0.0f;

            float noise_val = perlin_2d(&gen->noise,
                                       px / 100.0f,
                                       py / 100.0f);
            noise_val = (noise_val + 1.0f) * 0.5f;

            for (int i = 0; i < gen->num_layers; i++) {
                gradient_layer_t *layer = &gen->layers[i];

                float dx = px - layer->x;
                float dy = py - layer->y;
                float dist = sqrtf(dx * dx + dy * dy);

                float weight = powf(fmaxf(0.0f, 1.0f - dist / layer->radius),
                                   layer->falloff);
                weight *= (1.0f + noise_val * 0.3f);

                float h = fmodf(layer->hue + noise_val * 0.1f, 1.0f);
                float s = fmaxf(0.0f, fminf(1.0f, layer->saturation + (noise_val - 0.5f) * 0.2f));
                float v = fmaxf(0.0f, fminf(1.0f, layer->brightness + (noise_val - 0.5f) * 0.15f));

                uint8_t tr, tg, tb;
                hsv_to_rgb(h, s, v, &tr, &tg, &tb);

                r += tr * weight;
                g += tg * weight;
                b += tb * weight;
                total_weight += weight;
            }

            if (total_weight > 0.001f) {
                r /= total_weight;
                g /= total_weight;
                b /= total_weight;
            }

            int idx = (py * gen->width + px) * 3;
            pixels[idx + 0] = (uint8_t)fmaxf(0.0f, fminf(255.0f, r));
            pixels[idx + 1] = (uint8_t)fmaxf(0.0f, fminf(255.0f, g));
            pixels[idx + 2] = (uint8_t)fmaxf(0.0f, fminf(255.0f, b));
        }
    }
}

static int write_ppm6(const char *filename, int width, int height, uint8_t *pixels) {
    FILE *f = fopen(filename, "wb");
    if (!f) return 0;

    fprintf(f, "P6\n%d %d\n255\n", width, height);
    fwrite(pixels, 1, width * height * 3, f);
    fclose(f);
    return 1;
}


typedef struct {
    float h, s, v;
} hsv_color_t;

typedef struct {
    hsv_color_t colors[8];
    int count;
    const char *name;
} palette_t;

static palette_t palettes[] = {
    {
        .name = "Vibrant Neon",
        .colors = {
            {0.85f, 1.0f, 1.0f},
            {0.50f, 1.0f, 1.0f},
            {0.17f, 1.0f, 1.0f},
            {0.05f, 1.0f, 0.9f},
            {0.30f, 1.0f, 1.0f},
        },
        .count = 5
    },
    {
        .name = "Deep Moody",
        .colors = {
            {0.75f, 0.8f, 0.4f},
            {0.65f, 0.7f, 0.3f},
            {0.50f, 0.6f, 0.35f},
            {0.55f, 0.65f, 0.3f},
        },
        .count = 4
    },
    {
        .name = "Smooth Transitions",
        .colors = {
            {0.0f, 0.8f, 0.9f},
            {0.35f, 0.9f, 0.85f},
            {0.65f, 0.8f, 0.9f},
            {0.15f, 0.7f, 0.8f},
        },
        .count = 4
    }
};

#define PALETTE_COUNT 3

static image_generator_t* create_generator(int width, int height, uint64_t seed) {
    image_generator_t *gen = malloc(sizeof(*gen));
    gen->width = width;
    gen->height = height;
    gen->rng.state = seed;

    int palette_idx = xorshift64_next(&gen->rng) % PALETTE_COUNT;
    palette_t *palette = &palettes[palette_idx];

    gen->num_layers = 2 + (xorshift64_next(&gen->rng) % 3);
    gen->layers = malloc(gen->num_layers * sizeof(gradient_layer_t));

    for (int i = 0; i < gen->num_layers; i++) {
        gradient_layer_t *layer = &gen->layers[i];

        layer->x = xorshift64_float(&gen->rng) * width;
        layer->y = xorshift64_float(&gen->rng) * height;

        layer->radius = 300.0f + xorshift64_float(&gen->rng) * 1200.0f;

        layer->falloff = 1.0f + xorshift64_float(&gen->rng) * 2.0f;

        hsv_color_t *color = &palette->colors[xorshift64_next(&gen->rng) % palette->count];
        layer->hue = color->h;
        layer->saturation = color->s;
        layer->brightness = color->v;
    }

    perlin_init(&gen->noise, seed ^ 0xDEADBEEF);

    return gen;
}

static void destroy_generator(image_generator_t *gen) {
    free(gen->layers);
    free(gen);
}

int main(int argc, char *argv[]) {
    int width = 1920, height = 1080;
    uint64_t seed = time(NULL);

    if (argc > 1) seed = strtoull(argv[1], NULL, 10);
    if (argc > 2) width = atoi(argv[2]);
    if (argc > 3) height = atoi(argv[3]);

    printf("Generating %dx%d gradient image (seed: %lu)\n", width, height, seed);

    image_generator_t *gen = create_generator(width, height, seed);
    uint8_t *pixels = malloc(width * height * 3);

    generate_image(gen, pixels);

    char filename[256];
    snprintf(filename, sizeof(filename), "gradient_%lu.ppm", seed);

    if (write_ppm6(filename, width, height, pixels)) {
        printf("Saved to %s\n", filename);
    } else {
        printf("Failed to write file\n");
    }

    free(pixels);
    destroy_generator(gen);
    return 0;
}
