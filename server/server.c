#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

static volatile int keepRunning = 1; // значение переменной может измениться в любой момент, поэтому отключаем оптимизации

void intHandler(int x) {
    (void)x; // unused
    keepRunning = 0;
    printf("\nExiting!\n");
}

static const key_t MEM_ID = 8296; // "name" of shared segment
static const size_t MEM_SIZE = 1024 * 64; // size of shared segment
static const key_t SEM_ID = 1296;

void sem(int semId, short c1 /* -101 */, short c2 /* -101 */) {
    if (!keepRunning) {
        return;
    }

    struct sembuf ops[2];
    ops[0].sem_num = 0;
    ops[0].sem_op = c1;
    ops[0].sem_flg = 0;

    ops[1].sem_num = 1;
    ops[1].sem_op = c2;
    ops[1].sem_flg = 0;

    semop(semId, ops, 2);
}

void print_msgs(char* beg) {
    int i;
    int* len_ptr = (int*)beg;
    int len = *len_ptr; // full message len (with terminating zeros)
    beg += sizeof(int);
    // replace all terminating zeros (msgs are splitted with them)
    for (i = 0; i < len; ++i) {
        if (beg[i] == '\0') {
            beg[i] = '\n';
        }
    }
    // add terminating zero
    beg[i] = '\0';
    printf("%s", beg);
    *len_ptr = 0;
}

void put_msg(char* beg, char* msg) {
    int msg_len = strlen(msg);
    int* len_ptr = (int*)beg;
    beg += sizeof(int) + *len_ptr;
    memcpy(beg, msg, msg_len + 1);
    *len_ptr += msg_len + 1;
}

int main()
{
    int shmId;
    char* shmBeg;
    int semId; // sem for recieving signal messages

    signal(SIGINT, intHandler); // Переопределим ctrl + c для корректного завершения

    if ((shmId = shmget(MEM_ID, MEM_SIZE, 0666 | IPC_CREAT | IPC_EXCL)) < 0) { // create segment
        perror("shmget failed");
        return 1;
    }

    if ((shmBeg = shmat(shmId, NULL, 0)) == (char*)-1) { // attaching shared memory segment to process
        shmctl(shmId, IPC_RMID, NULL);
        perror("shmat failed");
        return 1;
    }

    if ((semId = semget(SEM_ID, 2, 0666 | IPC_CREAT | IPC_EXCL)) == -1) { // создаём 2 семафора в 1 группе
        // 1 - отвечает за отношение client -> server (добавил сообщение в очередь)
        // 2 - отвечает сигнал server -> client (готов принимать следующее сообщение)
        shmdt(shmBeg);
        shmctl(shmId, IPC_RMID, NULL);
        perror("semget failed");
        return 1;
    }

    *((int*)shmBeg) = 0; // we will store message lengths in first 4 bytes of buffer

    sem(semId, 0, 1); // готов принимать сообщения
    printf("server is running:\nshmId: %d\nsemId: %d\n", shmId, semId);
    while (keepRunning) {
        sem(semId, -1, 0); // capture
        print_msgs(shmBeg);
        sem(semId, 0, 1); // release
    }

    shmdt(shmBeg);
    shmctl(shmId, IPC_RMID, NULL);
    semctl(semId, -1, IPC_RMID);
    return 0;
}
