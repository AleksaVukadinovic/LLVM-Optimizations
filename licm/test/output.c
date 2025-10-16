void test(int *A, int n) {
    int x = n * 2;       // hoisted outside the loop
    for (int i = 0; i < 10; i++) {
        *A = x;
    }
}
