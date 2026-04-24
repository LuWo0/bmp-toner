#include "utils.hpp"
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <input.bmp> <output.bmp>\n", argv[0]);
    return 1;
  }

  if (!is_bmp_file(argv[1]) || !is_bmp_file(argv[2])) {
    fprintf(stderr, "Usage: %s <input.bmp> <output.bmp>\n", argv[0]);
    return 1;
  }

  FILE *file = fopen(argv[1], "rb");

  if (!file) {
    perror("File does not exist\n");
    return 1;
  }

  BMPFileHeader fh;
  BMPInfoLoader fih;

  fread(&fh.bfType, sizeof(short), 1, file);
  fread(&fh.bfSize, sizeof(int), 1, file);
  fread(&fh.bfReserved1, sizeof(short), 1, file);
  fread(&fh.bfReserved2, sizeof(short), 1, file);
  fread(&fh.bfOffBits, sizeof(int), 1, file);

  fread(&fih, sizeof(fih), 1, file);

  int width = fih.biWidth;
  int height = abs(fih.biHeight);
  int bytes_per_row = ((width * 3 + 3) / 4) * 4;
  int rows_per_thread = height / THREAD_SIZE;

  if (fih.biSizeImage == 0) {
    fih.biSizeImage = bytes_per_row * height;
  }

  Byte *data = (Byte *)malloc(fih.biSizeImage);

  fread(data, fih.biSizeImage, 1, file);

  barrier_t b;
  b.n = THREAD_SIZE;
  atomic_store(&b.count, 0);
  atomic_store(&b.sense, 0);
  std::atomic_int arrived[THREAD_SIZE] = {};
  double sums[THREAD_SIZE] = {};
  double Lavg = 0.0;

  pthread_t threads[THREAD_SIZE];
  TParams tparams[THREAD_SIZE];

  for (int i = 0; i < THREAD_SIZE; i++) {
    tparams[i] = {data,
                  width,
                  height,
                  bytes_per_row,
                  i * rows_per_thread,
                  i == THREAD_SIZE - 1 ? height : (i + 1) * rows_per_thread,
                  i,
                  sums,
                  &Lavg,
                  (long long)width * height,
                  &b,
                  arrived};

    pthread_create(&threads[i], NULL, worker, &tparams[i]);
  }
  for (int i = 0; i < THREAD_SIZE; i++) {
    pthread_join(threads[i], NULL);
  }

  FILE *result = fopen(argv[2], "wb");

  fwrite(&fh.bfType, sizeof(short), 1, result);
  fwrite(&fh.bfSize, sizeof(int), 1, result);
  fwrite(&fh.bfReserved1, sizeof(short), 1, result);
  fwrite(&fh.bfReserved2, sizeof(short), 1, result);
  fwrite(&fh.bfOffBits, sizeof(int), 1, result);

  fwrite(&fih, sizeof(fih), 1, result);
  fwrite(data, fih.biSizeImage, 1, result);

  fclose(result);
  fclose(file);
  free(data);

  return 0;
}
