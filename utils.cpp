#include "utils.hpp"
#include <string>

void barrier_wait(barrier_t *b, int *local_sense) {
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

void gather(std::atomic_int threads[], int gather_id, int thread_id,
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
          0.2126 * R + 0.7152 * G + 0.0722 * B; // perceived brightness
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
