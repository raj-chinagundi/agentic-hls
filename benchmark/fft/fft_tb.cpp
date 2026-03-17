#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define FFT_SIZE 1024
#define twoPI 6.28318530717959

void fft(double real[FFT_SIZE], double img[FFT_SIZE],
         double real_twid[FFT_SIZE / 2], double img_twid[FFT_SIZE / 2]);

int main() {
    static double real[FFT_SIZE], img[FFT_SIZE];
    static double real_twid[FFT_SIZE / 2], img_twid[FFT_SIZE / 2];
    int i, j;

    for (i = 0; i < FFT_SIZE / 2; i++) {
        real_twid[i] = cos(twoPI * i / FFT_SIZE);
        img_twid[i]  = -sin(twoPI * i / FFT_SIZE);
    }

    for (i = 0; i < 1000; i++) {
        for (j = 0; j < FFT_SIZE; j++) {
            real[j] = cos(twoPI * j / FFT_SIZE);
            img[j]  = 0.0;
        }
        fft(real, img, real_twid, img_twid);
    }

    printf("real[1]: %f\n", real[1]);
    return 0;
}
