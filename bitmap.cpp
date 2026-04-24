#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <string>

#define EXPOSURE_KEY 0.18
#define THREAD_SIZE 4

#ifdef USE_BARRIER
#define WAIT(step) barrier_wait(tparams->b, &local_sense)
#else
#define WAIT(step) gather(tparams->arrived, (step), tparams->id, THREAD_SIZE)
#endif

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

typedef unsigned char Byte;

typedef struct {
  Byte *data;

  int width;
  int height;
  int bytes_per_row;
  int start;
  int end;
  int id;

  double *sums;
  double *Lavg_out;

  long long num_of_pixels;

  barrier_t *b;

  std::atomic_int *arrived;
} TParams;

static void barrier_wait(barrier_t *b, int *local_sense) {
  int s = *local_sense;

  int arrived = atomic_fetch_add(&b->count, 1) + 1;

  if (arrived == b->n) {
    atomic_store(&b->count, 0);
    atomic_store(&b->sense, 1 - s);
  } else {
    while (atomic_load(&b->sense) == s) {
    }
  }

  *local_sense = 1 - s;
}

static void gather(std::atomic_int threads[], int gather_id, int thread_id,
                   int thread_count) {
  threads[thread_id].fetch_add(1);
  while (1) {
    int breakout = 1;

    for (int i = 0; i < thread_count; i++) {
      if (threads[i].load() < gather_id) {
        breakout = 0;
      }
    }
    if (breakout == 1)
      break;
  }
}

void *worker(void *p) {
  TParams *tparams = (TParams *)p;
  double sum = 0.0;
  int local_sense = 0;

  for (int y = tparams->start; y < tparams->end; y++) {
    for (int x = 0; x < tparams->width; x++) {
      int offset = y * tparams->bytes_per_row + x * 3;
      double B = tparams->data[offset + 0] / 255.0;
      double G = tparams->data[offset + 1] / 255.0;
      double R = tparams->data[offset + 2] / 255.0;
      double luminance =
          0.2126 * R + 0.7152 * G + 0.0722 * B; // percieved brightness
      double logL = log(luminance + 1);
      sum += logL;
    }
  }
  tparams->sums[tparams->id] = sum;

  // gather or barrier_wait
  WAIT(1);

  if (tparams->id == 0) {
    double total = 0.0;
    for (int i = 0; i < THREAD_SIZE; i++) {
      total += tparams->sums[i];
    }
    *tparams->Lavg_out = exp(total / tparams->num_of_pixels) - 1.0;
  }

  // gather or barrier_wait
  WAIT(2);
  double Lavg = *tparams->Lavg_out;
  for (int y = tparams->start; y < tparams->end; y++) {
    for (int x = 0; x < tparams->width; x++) {
      int offset = y * tparams->bytes_per_row + x * 3;
      double B = tparams->data[offset + 0] / 255.0;
      double G = tparams->data[offset + 1] / 255.0;
      double R = tparams->data[offset + 2] / 255.0;

      double luminance =
          0.2126 * R + 0.7152 * G + 0.0722 * B; // percieved brightness

      // Lm = (a / Lavg) · L
      double lm = (EXPOSURE_KEY / Lavg) * luminance;

      // Ld = Lm / (1 + Lm)
      double ld = lm / (1 + lm);

      // scale = Ld / L (if L > 0, else 0)
      double scale = luminance > 0 ? (ld / luminance) : 0.0;

      // Clamping values between 0 - 255
      Byte B_prime = fmin(fmax(B * scale * 255.0, 0.0), 255.0);
      Byte G_prime = fmin(fmax(G * scale * 255.0, 0.0), 255.0);
      Byte R_prime = fmin(fmax(R * scale * 255.0, 0.0), 255.0);
      tparams->data[offset + 0] = B_prime;
      tparams->data[offset + 1] = G_prime;
      tparams->data[offset + 2] = R_prime;
    }
  }

  // gather or barrier_wait
  WAIT(3);
  return NULL;
}

bool is_bmp_file(const char *fn) {
  std::string file_name(fn);
  return file_name.size() >= 4 &&
         file_name.compare(file_name.size() - 4, 4, ".bmp") == 0;
}

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

  unsigned char *data = (unsigned char *)malloc(fih.biSizeImage);

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
