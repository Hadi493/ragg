#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define WIDTH 1920
#define HEIGHT 1080

float fract(float x) { return x - floorf(x); }
float mix(float a, float b, float t) { return a + t * (b - a); }
float length(float x, float y) { return sqrtf(x * x + y * y); }

float rand_f(float min, float max) {
    return min + ((float)rand() / (float)RAND_MAX) * (max - min);
}

float hash(float x, float y) {
    float h = sinf(x * 12.9898f + y * 78.233f) * 43758.5453123f;
    return fract(h);
}

float noise(float x, float y) {
    float ix = floorf(x);
    float iy = floorf(y);
    float fx = fract(x);
    float fy = fract(y);
    float ux = fx * fx * (3.0f - 2.0f * fx);
    float uy = fy * fy * (3.0f - 2.0f * fy);
    return mix(mix(hash(ix, iy), hash(ix + 1.0f, iy), ux),
               mix(hash(ix, iy + 1.0f), hash(ix + 1.0f, iy + 1.0f), ux), uy);
}

float fbm(float x, float y, float c, float s) {
    float v = 0.0f;
    float a = 0.5f;
    for (int i = 0; i < 6; i++) {
        v += a * noise(x, y);
        float tx = x;
        x = (tx * c - y * s) * 2.02f;
        y = (tx * s + y * c) * 2.03f;
        a *= 0.5f;
    }
    return v;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return EXIT_FAILURE;
    }

    unsigned int seed = (unsigned int)time(NULL);
    srand(seed);

    FILE *fp = fopen(argv[1], "wb");
    if (!fp) {
        fprintf(stderr, "error: failed to open file %s\n", argv[1]);
        return EXIT_FAILURE;
    }
    fprintf(fp, "P6\n%d %d\n255\n", WIDTH, HEIGHT);

    float zoom = rand_f(0.5f, 2.5f);
    float warp_strength = rand_f(2.0f, 10.0f);
    float angle = rand_f(0.0f, 6.28f);
    float cos_a = cosf(angle);
    float sin_a = sinf(angle);
    float off_x = rand_f(0, 1000);
    float off_y = rand_f(0, 1000);

    float pal[4][3];
    for(int i=0; i<4; i++) {
        pal[i][0] = rand_f(0, 255);
        pal[i][1] = rand_f(0, 255);
        pal[i][2] = rand_f(0, 255);
    }

    float aspect = (float)WIDTH / HEIGHT;

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            float u = (((float)x / WIDTH) * aspect * zoom) + off_x;
            float v = (((float)y / HEIGHT) * zoom) + off_y;

            float qx = fbm(u, v, cos_a, sin_a);
            float qy = fbm(u + 1.1f, v + 1.1f, cos_a, sin_a);

            float rx = fbm(u + warp_strength * qx, v + warp_strength * qy, cos_a, sin_a);
            float ry = fbm(u + warp_strength * qx + 2.0f, v + warp_strength * qy + 3.0f, cos_a, sin_a);

            float val = fbm(u + warp_strength * rx, v + warp_strength * ry, cos_a, sin_a);

            float t = val + length(rx, ry) * 0.2f;
            float r, g, b;

            if (t < 0.33f) {
                float f = t / 0.33f;
                r = mix(pal[0][0], pal[1][0], f);
                g = mix(pal[0][1], pal[1][1], f);
                b = mix(pal[0][2], pal[1][2], f);
            } else if (t < 0.66f) {
                float f = (t - 0.33f) / 0.33f;
                r = mix(pal[1][0], pal[2][0], f);
                g = mix(pal[1][1], pal[2][1], f);
                b = mix(pal[1][2], pal[2][2], f);
            } else {
                float f = fminf(1.0f, (t - 0.66f) / 0.34f);
                r = mix(pal[2][0], pal[3][0], f);
                g = mix(pal[2][1], pal[3][1], f);
                b = mix(pal[2][2], pal[3][2], f);
            }

            float grain = rand_f(-8.0f, 8.0f);

            fputc((unsigned char)fminf(255, fmaxf(0, r + grain)), fp);
            fputc((unsigned char)fminf(255, fmaxf(0, g + grain)), fp);
            fputc((unsigned char)fminf(255, fmaxf(0, b + grain)), fp);
        }
    }

    fclose(fp);
    printf("Variation %u: Scale %.2f, Warp %.2f\n", seed, zoom, warp_strength);
    return EXIT_SUCCESS;
}
