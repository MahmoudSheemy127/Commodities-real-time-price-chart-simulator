#include <iostream>
#include <stdio.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <string.h>
#include <unistd.h>
#include <random>
#include <vector>

#define NUM_OF_COMMODITIES 11
#define MAX_BUFFER 20


using namespace std;


//Global functions 
string mapCommIndexToName(int commIndex);
float calcAvg(vector<float> prices);
void printCommodities();
void printTableStructure();
void getLastCommodityPrice(char* str, char* lastItemString);
void consume(char* lastItemString);


//Price Indicator enum
typedef enum priceIndicator{
    HIGH,
    LOW,
    NEUTRAL
} priceIndicator;


typedef union semun {
   int val; /* val for SETVAL */
   struct semid_ds *buf; /* Buffer for IPC_STAT and IPC_SET */
   unsigned short *array; /* Buffer for GETALL and SETALL */
   struct seminfo *__buf; /* Buffer for IPC_INFO and SEM_INFO*/
} senum;


//Commodity struct
typedef struct Comm{
    string Name;
    vector<float> prices;
    float avg;
    float prevPrice;
}Comm;


//Array of commodities 
Comm commodities[NUM_OF_COMMODITIES];

int main(int argc, char **argv)
{
    int i;
    int N = atoi(argv[1]);
    //initialize commodities array
    int ret_val;
    for(i=0;i<NUM_OF_COMMODITIES;i++)
    {
        commodities[i].Name = mapCommIndexToName(i);
        commodities[i].avg = 0.00;
        commodities[i].prevPrice = 0.00;
    }
    printf("\e[1;1H\e[2J");
    printTableStructure();

    //shared memory initialize
    //generate unique key for shared mem
    key_t key = ftok("shmfile",65);
    //return identifier shmid
    int shmid = shmget(key,1024,0666|IPC_CREAT);

    //initialize semaphores
    //init semOP struct
    struct sembuf semOpBuff;
    struct sembuf semOpMutex;
    struct sembuf semOpCount;

    senum semBufferControl;

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

        semBufferControl.val = MAX_BUFFER;
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
    while(true)
    {
        //semWait(C)
        semOpCount.sem_num = 0;
        semOpCount.sem_op = -1;
        ret_val = semop(semCountId,&semOpCount,1);
        if(ret_val == -1)
        {
            //error occurs while waiting
            perror("Semaphore Locked in count semaphoree (semWait): ");
            return -1;
        }

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

        //Critical section
        //access main memory
        char *str = (char*)shmat(shmid,(void*)0,0);
        char lastItemString[20];
        //get first item
        getLastCommodityPrice(str,lastItemString);
        //deattach from memory
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
        //semSignal(b)
        semOpBuff.sem_num = 0;
        semOpBuff.sem_op = 1;
        ret_val = semop(semBufferId,&semOpBuff,1);
        if(ret_val == -1)
        {
            //error occurs while waiting
            perror("Semaphore Locked in buffer semaphoree (semSignal): ");
            return -1;
        }
        consume(lastItemString);
    }
}

//print out all commodities
void printCommodities(){
    
    int i;
    for(i=0;i<NUM_OF_COMMODITIES;i++)
    {
        printf("\033[%d;3H",(4+i));
        cout << commodities[i].Name;
        printf("\033[%d;20H",(4+i));
        printf("0.00");
        printf("\033[%d;30H",(4+i));
        printf("0.00");
    }
    printf("\033[%d;1H",(5+i));

}

void consume(char *lastItemString)
{
    //parse the price str selected
    //update the commodity price
    int currentIndex,priceIndicator,averagePriceIndicator;
    float currentPrice,currentAvg,prevPrice,prevAvg;
    char *ptr = strtok(lastItemString,",");
    currentIndex = atoi(ptr);
    ptr = strtok(NULL,",");
    currentPrice = stof(ptr);
    prevPrice = commodities[currentIndex].prevPrice;
    commodities[currentIndex].prevPrice = currentPrice;

    if(commodities[currentIndex].prices.size() >= 4)
    {
        commodities[currentIndex].prices.erase(commodities[currentIndex].prices.begin());
    }
    commodities[currentIndex].prices.push_back(currentPrice);

    currentAvg = calcAvg(commodities[currentIndex].prices);
    prevAvg = commodities[currentIndex].avg;
    commodities[currentIndex].avg = currentAvg;

    priceIndicator = currentPrice > prevPrice ? HIGH : (currentPrice < prevPrice ? LOW : NEUTRAL);
    averagePriceIndicator = currentAvg > prevAvg ? HIGH : (currentAvg < prevAvg ? LOW : NEUTRAL); 

    //print changes in table
    printf("\033[%d;20H",(4+currentIndex));
    if(priceIndicator == HIGH)
    {
        printf("\033[;32m%7.2lf↑\033[0m",currentPrice);                        
    }
    else if(priceIndicator == LOW)
    {
        printf("\033[1;31m%7.2lf↓\033[0m\n",currentPrice);

    }
    else if(priceIndicator == NEUTRAL) {
        printf("\033[;34m%7.2lf\033[0m",currentPrice);
    }
    printf("\033[%d;30H",(4+currentIndex));
    if(averagePriceIndicator == HIGH)
    {
        printf("\033[;32m%7.2lf↑\033[0m",currentAvg);                        
    }
    else if(averagePriceIndicator == LOW)
    {
        printf("\033[1;31m%7.2lf↓\033[0m\n",currentAvg);
    }
    else {
        printf("\033[;34m%7.2lf\033[0m",currentAvg);
    }
}

//print the structure of the table
void printTableStructure()
{
    int i;
    cout << "+-------------------------------------+" << endl;
    cout << "| Currency        | Price  | AvgPrice |" << endl;
    cout << "+-------------------------------------+" << endl;
    for(i=0;i<NUM_OF_COMMODITIES;i++)
    {
    cout << "|                 |        |          | " << endl;
    }
    cout << "+-------------------------------------+" << endl;
    for(i=0;i<NUM_OF_COMMODITIES;i++)
    {
        printf("\033[%d;3H",(4+i));
        cout << commodities[i].Name;
        printf("\033[%d;20H",(4+i));
        printf("\033[;34m0.00\033[0m");
        printf("\033[%d;30H",(4+i));
        printf("\033[;34m0.00\033[0m");
    }
    printf("\033[%d;1H",(5+i));
}

//Calculate the average of the previous x prices
float calcAvg(vector<float> prices)
{
    float pricesSum = 0;
    for(int i=0; i<prices.size();i++)
    {
        pricesSum += prices[i];
    }
    return (pricesSum/prices.size());
}


void getLastCommodityPrice(char* str,char *lastItemString)
{
    //get last element from the buffer
    char sub[strlen(str)];
    memset(sub,0,strlen(str));
    char *ptr;
    ptr = strtok(str,"\n");
    strcpy(lastItemString,ptr);
    ptr = strtok(NULL,"\n");
    while(ptr != NULL)
    {
        strcat(sub,ptr);
        strcat(sub,"\n");
        ptr = strtok(NULL,"\n");
    }
    strcpy(str,sub);
    
}


//Map specific index to a commodity name  (used while printing)
string mapCommIndexToName(int commIndex)
{
    if(commIndex == 0)
    {
        return "ALUMINIUM";
    }
    else if(commIndex == 1)
    {
        return "COPPER";
    }
    else if(commIndex == 2)
    {
        return "COTTON";
    }
    else if(commIndex == 3)
    {
        return "CRUDEOIL";
    }
    else if(commIndex == 4)
    {
        return "GOLD";
    }
    else if(commIndex == 5)
    {
        return "LEAD";
    }
    else if(commIndex == 6)
    {
        return "METHANOL";
    }
    else if(commIndex == 7)
    {
        return "NATURALGAS";
    }
    else if(commIndex == 8)
    {
        return "NICKEL";
    }
    else if(commIndex == 9)
    {
        return "SILVER";
    }
    else if(commIndex == 10)
    {
        return "ZINC";
    }
    else
    {
        return "NONE";
    }
}
