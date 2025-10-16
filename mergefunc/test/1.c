#include <stdio.h>

float f() {
    int x = 10;
    return 10.0;
}

int g() {
    int x = 10;
    return 10;
}

float h() {
    int y = 10;
    return 10.0;
}

int main() {
    f();
    g();
    h();
    return 0;
}