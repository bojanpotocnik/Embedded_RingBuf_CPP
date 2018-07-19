#include <cstdio>
#include <cstdlib>
#include "RingBufCPP.h"


int main() {
    srand(0);

    RingBufCPP<int, 10> q;
    int tmp = -1;

    for (uint16_t i = 0; i < 100; i++) {
        tmp = 1000000 + i * 1000 + q.size();

        if (q.add(tmp)) {
            printf("%d) Added %d\n", i,  tmp);
        }
        else {
            printf("%d) Buffer is full\n", i);
            for (size_t j = 0; ; j++) {
                auto peek_val = q.peek(j);
                if (!peek_val) {
                    break;
                }
                printf("Peeked %d = %d\n", j, *peek_val);
            }

            q.pull(&tmp);
            printf("%d) Buffer is full. Pulled %d\n", i, tmp);
            break;
        }

        if ((i == 4) || (i == 6)) {
            for (size_t j = 0; ; j++) {
                auto peek_val = q.peek(j);
                if (!peek_val) {
                    break;
                }
                printf("%d) Peeked %d = %d\n", i, j, *peek_val);
            }

            int b;
            q.pull(&b);
            printf("%d) Pulled %d\n", i, b);
        }
    }

    for (size_t j = 0; ; j++) {
        auto peek_val = q.peek(j);
        if (!peek_val) {
            break;
        }
        printf("Peeked %d = %d\n", j, *peek_val);
    }

    while (!q.empty()) {
        int pulled;
        q.pull(&pulled);
        printf("Got %d\n", pulled);
    }
}
