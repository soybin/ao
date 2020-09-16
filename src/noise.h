#pragma once

unsigned char* worley(unsigned int seed, int width, int height, int depth);

unsigned char* perlin(unsigned int seed, int width, int height, int depth);

unsigned char* worley_perlin(unsigned int seed, int width, int height, int depth);
