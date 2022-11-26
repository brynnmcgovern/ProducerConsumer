/**
 * 1. Brynn McGovern
 *    2370579
 *    bmcgovern@chapman.edu
 *    CPSC 380
 *    Assignment 5: Thread Synchronization
 * 2. Implements two threads to read and write to shared memory, and semaphores and a mutex
      to ensure no race conditions happen.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <errno.h>
#include <stdint.h>
#include <semaphore.h>
#include <time.h>
#include <pthread.h>



typedef struct{
  int seqNo;
  unsigned short checksum;
  uint32_t timeStamp;
  uint8_t data[22];

}Item;

char* shm_name = "buffer";
int shm_fd;
Item  *shm_ptr;
pthread_t producers;
pthread_t consumers;
pthread_mutex_t mutex;
sem_t *empty, *full;
int shmSize;
int counter = 0;
int size = 0;
struct stat buf;



/**
  CreateSharedMemory()
  Method to create shared memory
*/
void CreateSharedMemory(){


  shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0644);
  if (shm_fd == -1) {
      fprintf(stderr, "Error unable to create shared memory, '%s, errno = %d (%s)\n", shm_name,
              errno, strerror(errno));
      exit(0);
  }

  /* configure the size of the shared memory segment */
  if (ftruncate(shm_fd, shmSize) == -1) {
       fprintf(stderr, "Error configure create shared memory, '%s, errno = %d (%s)\n", shm_name,
              errno, strerror(errno));
       shm_unlink(shm_name);
       exit(0);
  }

  printf("shared memory create success, shm_fd = %d\n", shm_fd);
}

/**
  checksum()
  @param char *addr, uint32_t count
  @return uint16_t
  This method calculates a checksum and returns it
*/
uint16_t checksum(char *addr, uint32_t count)
{
    register uint32_t sum = 0;

    uint16_t *buf = (uint16_t *) addr;

    // Main summing loop
    while(count > 1)
    {
        sum = sum + *(buf)++;
        count = count - 2;
    }

    // Add left-over byte, if any
    if (count > 0)
        sum = sum + *addr;

    // Fold 32-bit sum to 16 bits
    while (sum>>16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return(~sum);

}

/**
  CreateItem()
  @return Item
  Method to generate an item and return it
*/
Item CreateItem(){
  Item item;
  uint16_t cksum;
  int j = 0;
  time_t seconds;
  seconds = time(NULL);


  item.seqNo = j;
  item.timeStamp = seconds;
  for(int i = 0; i < 22; ++i){
    item.data[i] = rand() % 256;
  }
  cksum = checksum(&item.data[0], 21);

  item.checksum = cksum;

  ++j;

  return item;
}

/**
  producer()
  This method writes to shared memory and uses counting semaphores and the mutex to make sure
  there's no race condition
*/
void* producer() {


  shm_ptr = mmap(0, 32, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);


    while(1) {
        Item tempItem = CreateItem();
        tempItem.seqNo = counter;


        sem_wait(empty);
        pthread_mutex_lock(&mutex);

        while (counter == shmSize)
            ; // waiting

        if(counter < shmSize) {

            shm_ptr[counter] = tempItem;
            printf("Produced: %d\n", counter);
            counter ++;





        }

        sleep(1);
        pthread_mutex_unlock(&mutex);
        sem_post(full);
    }
}

/**
  consumer()
  This method reads from shared memory and checks to make sure the data wasn't corrupted via the
  item's checksum
*/
void* consumer() {
    unsigned short cksum1, cksum2;
    Item tempItem;



    while(1) {
        sem_wait(full);
        pthread_mutex_lock(&mutex);



        while (counter == 0)
            ; // waiting

        if(counter > 0) {

             tempItem = shm_ptr[counter-1];

            cksum1 = tempItem.checksum;

            cksum2 = checksum(&tempItem.data[0], 21);
            if (cksum1 != cksum2) {
               printf("Checksum mismatch: expected %02x, received %02x \n", cksum2, cksum1);
               exit(0);

            }

            printf("Checksums match !!! \n");

            counter --;

        }

        sleep(1);

        pthread_mutex_unlock(&mutex);
        sem_post(empty);
    }
}

int main(int argc, char **argv){

  sem_unlink(&empty);
  sem_unlink(&full);

  shm_unlink(shm_name);

  //sem_unlink(semaphore names); //dont forget to take this out before turning it in
  shmSize = atoi(argv[1]);
  if(shmSize < 0){
    printf("Error: Size of buffer cannot be negative. ");
    return -1;
  }


  pthread_mutex_init(&mutex, NULL);
  empty = sem_open("/empty", O_CREAT, 0644, shmSize);
  full = sem_open("/full", O_CREAT, 0644, 0);

  size = shmSize * 32;
  CreateSharedMemory();



  pthread_create(&producers, NULL, producer, NULL);
  pthread_create(&consumers, NULL, consumer, NULL);

  pthread_exit(NULL);

  return 0;


}
