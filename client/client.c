#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

static volatile int keepRunning = 1; // значение переменной может измениться в любой момент, поэтому отключаем оптимизации

char* shmBeg = NULL;

void intHandler(int x) {
    (void)x; // unused
    printf("\nExiting!\n");
    if (shmBeg && shmBeg != (char*)-1) {
        shmdt(shmBeg);
    }
    exit(0);
}

static const key_t MEM_ID = 8296; // "name" of shared segment
static const size_t MEM_SIZE = 1024 * 64; // size of shared segment
static const key_t SEM_ID = 1296;

void sem(int semId, short c1 /* -101 */, short c2 /* -101 */) {
    struct sembuf ops[2];
    ops[0].sem_num = 0;
    ops[0].sem_op = c1;
    ops[0].sem_flg = 0;

    ops[1].sem_num = 1;
    ops[1].sem_op = c2;
    ops[1].sem_flg = 0;
    semop(semId, ops, 2);
}

void put_msg(char* beg, char* msg) {
    int msg_len = strlen(msg);
    msg[msg_len--] = '\0'; // removing trailing '\n'
    int* len_ptr = (int*)beg;
    beg += sizeof(int) + *len_ptr;
    memcpy(beg, msg, msg_len + 1); // copy with(!) terminating zero
    *len_ptr += msg_len + 1;
}

int main()
{
    int shmId;
    int semId; // sem for recieving signal messages

    signal(SIGINT, intHandler); // Переопределим ctrl + c для корректного завершения

    if ((shmId = shmget(MEM_ID, MEM_SIZE, 0666 | IPC_CREAT)) < 0) { // получаем существующий сегмент
        perror("shmget failed");
        return 1;
    }

    if ((shmBeg = shmat(shmId, NULL, 0)) == (char*)-1) { // attaching shared memory segment to process
        perror("shmat failed");
        return 1;
    }

    if ((semId = semget(SEM_ID, 2, 0666 | IPC_CREAT)) == -1) {
        perror("semget failed");
        shmdt(shmBeg);
        return 1;
    }

    char buf[1024];
    printf("Enter your name: ");
    fgets(buf, sizeof(buf) - 2, stdin); // gets deprecated (and dangerous)!
    int nameLen = strlen(buf);
    buf[nameLen - 1] = ':';
    buf[nameLen] = ' ';
    int prefixLen = nameLen + 1;

    while (1) {
        printf("Message: ");
        fgets(buf + prefixLen, sizeof(buf) - prefixLen, stdin);
        sem(semId, 0, -1); // capture
        put_msg(shmBeg, buf);
        sem(semId, 1, 0); // release
    }
    return 0;
}
