// Sleeps for 10 ticks and then prints out pid. 

#include "types.h"
#include "user.h"
#include "stat.h"

int main(int argc, char *argv[]) {
    sleep(10);
    printf(1, "%d\n", getpid());
    exit();
}
