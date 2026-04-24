# Lab 3 — Parallel Two-Stage Tone Mapping

Tone-maps a 24-bit BMP using the Reinhard operator, split across 4 threads.
Each thread processes a horizontal stripe of the image. Threads synchronize
between stages with either a DIY gather or a sense-reversing barrier.

## Build

    make  
(Or directly: `g++ -pthread bitmap.cpp -o gather`
 / `g++ -pthread -DUSE_BARRIER bitmap.cpp -o barrier`.)

## Run

    ./gather   input.bmp output.bmp
    ./barrier  input.bmp output.bmp

Both binaries produce the same output; the only difference is which
synchronization primitive is used between the two stages.

## How it works

1. Stage 1 — each thread computes the partial sum of `log(L+1)` over its stripe.
2. Sync — all threads wait, then thread 0 computes `Lavg`.
3. Sync — all threads see `Lavg`.
4. Stage 2 — each thread tone-maps its stripe in place.
5. Sync — main joins and writes the output BMP.


