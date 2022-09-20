#include "kernel/types.h"
#include "user/user.h"

void print(int x) {
    printf("prime %d\n", x);
}

void
panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(1);
}

void receive_and_send(int *p) {
    // printf("create pid=%d\n", getpid());
    int x,y;
    close(p[1]);
    int n = read(p[0], &x, sizeof x);
    if (n != sizeof x)
        panic("read fail");
    print(x);

    int forked = 0;
    int new_p[2];
    while ( (n = read(p[0], &y, sizeof y)) ) {
        if (n != sizeof y) {
            printf("read n %d\n", n);
            panic("read fail2");
        }
        if (y % x == 0)
            continue;
        if (!forked) {
            forked = 1;
            pipe(new_p);
            if (fork() == 0) {
                receive_and_send(new_p);
            }
            close(new_p[0]);
        }
        n = write(new_p[1], &y, sizeof(y));
        if (n != sizeof(y))
            panic("write fail");
    }
    close(new_p[1]);
    close(p[0]);
    wait(0);
    // printf("exit pid:%d\n", getpid());
    exit(0);
}

int
main(int argc, char *argv[])
{
    int p[2];
    pipe(p);

    if (fork() > 0) {
        close(p[0]);
        for (int i = 2; i <= 35; i++) {
            // printf("send i %d\n", i);
            int n = write(p[1], &i, sizeof i);
            if (n != sizeof i)
                panic("write fail");
        }
        close(p[1]);
        wait(0);
        exit(0);
    } else {
        receive_and_send(p);
    }
    return 0;
}
