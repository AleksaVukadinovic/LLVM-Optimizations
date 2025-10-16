void test(int *A, int n) {
    for (int i = 0; i < 10; i++) {
        int x = n * 2;   // loop-invariant, but recomputed every time
        *A = x;
    }
}

