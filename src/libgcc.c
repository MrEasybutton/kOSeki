unsigned long long __udivdi3(unsigned long long a, unsigned long long b) {
    unsigned long long res = 0;
    unsigned long long bit = 1;
    while (b < a && bit && !(b & (1ULL << 63))) {
        b <<= 1;
        bit <<= 1;
    }
    while (bit) {
        if (a >= b) {
            a -= b;
            res |= bit;
        }
        bit >>= 1;
        b >>= 1;
    }
    return res;
}

unsigned long long __umoddi3(unsigned long long a, unsigned long long b) {
    unsigned long long bit = 1;
    while (b < a && bit && !(b & (1ULL << 63))) {
        b <<= 1;
        bit <<= 1;
    }
    while (bit) {
        if (a >= b) {
            a -= b;
        }
        bit >>= 1;
        b >>= 1;
    }
    return a;
}

long long __divdi3(long long a, long long b) {
    int s_a = (a < 0);
    int s_b = (b < 0);
    unsigned long long ua = s_a ? -a : a;
    unsigned long long ub = s_b ? -b : b;
    unsigned long long res = __udivdi3(ua, ub);
    if (s_a ^ s_b) return -(long long)res;
    return (long long)res;
}