#ifndef UTILS_HPP
#pragma once

#include <atomic>
#include <pthread.h>

#define THREAD_SIZE 4
#define EXPOSURE_KEY 0.18

#ifdef USE_BARRIER
#define WAIT(step) barrier_wait(tparams->b, &local_sense)
#else
#define WAIT(step) gather(tparams->arrived, (step), tparams->id, THREAD_SIZE)
#endif

typedef unsigned char Byte;

struct BMPFileHeader {
  unsigned short bfType;      // 'BM' 0x4D42
  unsigned int bfSize;        // File size in bytes
  unsigned short bfReserved1; // Must be 0
  unsigned short bfReserved2; // Must be 0
  unsigned int bfOffBits;     // offset to pixel data
};

struct BMPInfoLoader {
  unsigned int biSize; // Header size
  int biWidth;
  int biHeight;
  unsigned short biPlanes;
  unsigned short biBitCount;
  unsigned int biCompression;
  unsigned int biSizeImage;
  int biXPelsPerMeter;
  int biYPelsPerMeter;
  unsigned int biClrUsed;
  unsigned int biClrImportant;
};

typedef struct {
  int n;
  std::atomic_int count;
  std::atomic_int sense;
} barrier_t;

typedef struct {
  Byte *data;
  int width, height, bytes_per_row;
  int start, end, id;
  double *sums;
  double *Lavg_out;
  long long num_of_pixels;
  barrier_t *b;
  std::atomic_int *arrived;
} TParams;

void barrier_wait(barrier_t *b, int *local_sense);
void gather(std::atomic_int threads[], int gather_id, int thread_id,
            int thread_count);
void *worker(void *p);
bool is_bmp_file(const char *fn);

#endif // !UTILS_HPP
