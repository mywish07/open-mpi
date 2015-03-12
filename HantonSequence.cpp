#include "HantonSequence.h"

HaltonSequence::HaltonSequence(long startIndex) {
    index = startIndex;
    value = new double[2];
    q = new double* [2];
    digit = new int* [2];
    for (int i = 0; i < 2; i++) {
        q[i] = new double[len[i]];
        digit[i] = new int[len[i]];
    }

    for (int i = 0; i < 2; i++) {
        long k = index;
        value[i] = 0;

        for (int j = 0; j < len[i]; j++) {
            q[i][j] = (j == 0 ? 1.0 : q[i][j - 1]) / base[i];
            digit[i][j] = (int) (k % base[i]);
            k = (k - digit[i][j]) / base[i];
            value[i] += digit[i][j] * q[i][j];
        }
    }
};

double* HaltonSequence::nextPoint() {
    index++;
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < len[i]; j++) {
            digit[i][j]++;
            value[i] += q[i][j];
            if (digit[i][j] < base[i]) {
                break;
            }
            digit[i][j] = 0;
            value[i] -= (j == 0 ? 1.0 : q[i][j - 1]);
        }
    }
    return value;
}