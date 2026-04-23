#include <cmath>
#include <cstdio>
#include <cstdlib>

#define EXPOSURE_KEY 0.18

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

typedef unsigned char Byte;

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  FILE *file = fopen("flowers.bmp", "rb");
  double sum = 0.0;

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

  if (fih.biSizeImage == 0) {
    fih.biSizeImage = bytes_per_row * height;
  }

  unsigned char *data = (unsigned char *)malloc(fih.biSizeImage);
  fread(data, fih.biSizeImage, 1, file);

  double average = 0;
  long long num_of_pixels = width * height;
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int offset = y * bytes_per_row + x * 3;
      double B_normal = data[offset + 0] / 255.0;
      double G_normal = data[offset + 1] / 255.0;
      double R_normal = data[offset + 2] / 255.0;
      double luminance = 0.2126 * R_normal + 0.7152 * G_normal +
                         0.0722 * B_normal; // percieved brightness
      double logL = log(luminance + 1);
      sum += logL;
    }
  }
  average = exp(sum / num_of_pixels) - 1.0;

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int offset = y * bytes_per_row + x * 3;
      double B_normal = data[offset + 0] / 255.0;
      double G_normal = data[offset + 1] / 255.0;
      double R_normal = data[offset + 2] / 255.0;

      double luminance = 0.2126 * R_normal + 0.7152 * G_normal +
                         0.0722 * B_normal; // percieved brightness

      // Lm = (a / Lavg) · L
      double lm = (EXPOSURE_KEY / average) * luminance;

      // Ld = Lm / (1 + Lm)
      double ld = lm / (1 + lm);

      // scale = Ld / L (if L > 0, else 0)
      double scale = luminance > 0 ? (ld / luminance) : 0.0;

      // Clamping values between 0 - 255
      Byte B_prime = fmin(fmax(B_normal * scale * 255.0, 0.0), 255.0);
      Byte G_prime = fmin(fmax(G_normal * scale * 255.0, 0.0), 255.0);
      Byte R_prime = fmin(fmax(R_normal * scale * 255.0, 0.0), 255.0);
      data[offset + 0] = B_prime;
      data[offset + 1] = G_prime;
      data[offset + 2] = R_prime;
    }
  }

  FILE *result = fopen("result.bmp", "wb");

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
