#include <iostream>
#include <stdio.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <string.h>
#include <unistd.h>
#include <random>
#include <time.h>

using namespace std;

#define MILLION 1000000L
#define MAX_BUFFER 20

int mapCommNameToIndex(string commName);

void printStamp(char *msg,char *commName);

typedef union semun {
   int val; /* val for SETVAL */
   struct semid_ds *buf; /* Buffer for IPC_STAT and IPC_SET */
   unsigned short *array; /* Buffer for GETALL and SETALL */
   struct seminfo *__buf; /* Buffer for IPC_INFO and SEM_INFO*/
} senum;

struct timespec ts;


int main(int argc, char **argv)
{
    //Common variables used
    char *commName;
    double priceMean;
    double stdD;
    int sleepTime;
    int N;
    char msg[100];
    string price;
    float comm_val;
    int ret_val;

    //take input from stdin
    commName = argv[1];
    priceMean = stod(argv[2]);
    stdD = stod(argv[3]);
    sleepTime = atoi(argv[4]);
    N = atoi(argv[5]);
    normal_distribution<double>   distrib(priceMean,stdD);
    default_random_engine generator;

    senum semBufferControl;

    //init semOP struct
    struct sembuf semOpBuff;
    struct sembuf semOpMutex;
    struct sembuf semOpCount;

    //init mutex semaphore key
    key_t semMutexKey = ftok("semMutexFile",55);

    //init buffer semaphore key
    key_t semBufferKey = ftok("semBufferFile",52);

    //init counter semaphore key
    key_t semCountKey = ftok("semCountFile",53);

    //init mutex semaphore id
    int semMutexId = semget(semMutexKey,1,IPC_CREAT | IPC_EXCL | 0666);
    if(semMutexId >= 0)
    {
        //mutex semaphore doesn't exist and initialize the semaphore
        senum semMutexControl;
        semMutexControl.val = 1;
        semctl(semMutexId,0,SETVAL,semMutexControl);
    }
    else if(semMutexId == -1)
    {
        //mutex semaphore does exist
        semMutexId = semget(semMutexKey,1,0);
    }

    //init buffer semaphore id
    int semBufferId = semget(semBufferKey,1,IPC_CREAT | IPC_EXCL | 0666);
    if(semBufferId >= 0)
    {
        // senum semBufferControl;
        semBufferControl.val = N;
        semctl(semBufferId,0,SETVAL,semBufferControl);
    }
    else if(semBufferId == -1)
    {
        semBufferId = semget(semBufferKey,1,0);
    }

    //init counter semaphore 
    int semCountId = semget(semCountKey,1,IPC_CREAT | IPC_EXCL | 0666);
    if(semCountId == -1)
    {
        semCountId = semget(semCountKey,1,0);
    }

    //generate unique key for shared mem
    key_t key = ftok("shmfile",65);
    printf("My key is %d\n",key);
    //return identifier shmid
    int shmid = shmget(key,1024,0666|IPC_CREAT);

    while(true)
    {

    //semWait(B)
    semOpBuff.sem_num = 0;
    semOpBuff.sem_op = -1;
    ret_val = semop(semBufferId,&semOpBuff,1);
    if(ret_val == -1)
    {
        //error occurs while waiting
        perror("Semaphore Locked in buffer semaphoree (semWait): ");
        return -1;
    }
    
    //Generating new value
    comm_val = distrib(generator);
    snprintf(msg,sizeof(msg),"generating a new value %.2f",comm_val);
    printStamp(msg,commName);
    price = to_string(mapCommNameToIndex(commName)) + "," + to_string(comm_val) + "\n";
    printStamp("Trying to get mutex on shared buffer",commName);
    //semWait(M)
    semOpMutex.sem_num = 0;
    semOpMutex.sem_op = -1;
    ret_val = semop(semMutexId,&semOpMutex,1);
    if(ret_val == -1)
    {
        //error occurs while waiting
        perror("Semaphore Locked in mutex semaphoree (semWait): ");
        return -1;
    }

    //critical section
    //attach to the shared memory
    char *str = (char*)shmat(shmid,(void*)0,0);
    snprintf(msg,sizeof(msg),"placing %.2f on shared buffer",comm_val);
    printStamp(msg,commName);
    //add new item to the buffer
    strcat(str,price.c_str());
    //deattach from the shared memory
    shmdt(str);
    //semSignal(M)
    semOpMutex.sem_num = 0;
    semOpMutex.sem_op = 1;
    ret_val = semop(semMutexId,&semOpMutex,1);
    if(ret_val == -1)
    {
        //error occurs while signaling
        perror("Semaphore Locked in mutex semaphoree (semSignal): ");
        return -1;
    }
    //semSignal(C)
    semOpCount.sem_num = 0;
    semOpCount.sem_op = 1;
    ret_val = semop(semCountId,&semOpCount,1);
    if(ret_val == -1)
    {
        //error occurs while signaling
        perror("Semaphore Locked in count semaphoree (semSignal): ");
        return -1;
    }
    snprintf(msg,sizeof(msg),"Sleeping for %.2f",sleepTime);
    printStamp(msg,commName);
    usleep(sleepTime*1000);
    }    
}

//Print timestamp messages during the producer's actions
void printStamp(char *msg, char *commName)
{
    //Print stamp message as a producer
    time_t t;
    t = time(NULL);
    struct tm tm = *localtime(&t);
    int res;
    res = clock_gettime(CLOCK_REALTIME,&ts);
    fprintf(stderr,"[%d/%d/%d/ %d:%d:%d.%d] %s: %s\n",tm.tm_mon+1,tm.tm_mday,tm.tm_year+1900,tm.tm_hour,tm.tm_min,tm.tm_sec,(ts.tv_nsec/10000),commName,msg);
}

int mapCommNameToIndex(string commName)
{
    if(commName == "ALUMINIUM")
    {
        return 0;
    }
    else if(commName == "COPPER")
    {
        return 1;
    }
    else if(commName == "COTTON")
    {
        return 2;
    }
    else if(commName == "CRUDEOIL")
    {
        return 3;
    }
    else if(commName == "GOLD")
    {
        return 4;
    }
    else if(commName == "LEAD")
    {
        return 5;
    }
    else if(commName == "METHANOL")
    {
        return 6;
    }
    else if(commName == "NATURALGAS")
    {
        return 7;
    }
    else if(commName == "NICKEL")
    {
        return 8;
    }
    else if(commName == "SILVER")
    {
        return 9;
    }
    else if(commName == "ZINC")
    {
        return 10;
    }
    else
    {
        return 11;
    }
}


