#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/errno.h>

#include "message.h"

#define BUS_IN_Q        "/dev/bus_in_q"
#define BUS_OUT_Q1      "/dev/bus_in_q1"
#define BUS_OUT_Q2      "/dev/bus_in_q2"
#define BUS_OUT_Q3      "/dev/bus_in_q3"
#define DURATION        10                          // duration (in secs) of thread execution
#define SENDER_T_CNT    3                          // number of sender threads
#define RECEIVER_T_CNT  3                          // number of receiver threads

pthread_t sender_t[SENDER_T_CNT], receiver_t[RECEIVER_T_CNT], bus_daemon;
int counter;
long quet;
int errno;
pthread_mutex_t lock;        // lock for counter


/* function to generate random string */
// http://stackoverflow.com/questions/15767691/whats-the-c-library-function-to-generate-random-string
void random_string_gen(char* string)
{
  char charset[] = "0123456789"
                     "abcdefghijklmnopqrstuvwxyz"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  int length = (rand() % 70) + 10;
  while (length-- > 0) {
    int index = rand() % 62;
    *string++ = charset[index];
  }
  *string = '\0';
}


// Each sender thread will run this function to create and write messages.
void* sender_func(void *arg)
 {
  int fd, ret, sleep_time;
  char* random_string = NULL;
  time_t end_time = time(NULL) + DURATION;    // run thread for certain duration
  struct message *new_msg;
  new_msg = (struct message *) malloc(sizeof(struct message));

  while (time(NULL) < end_time) 
  {
    pthread_mutex_lock(&lock);              // acquire lock for message_id
    new_msg->message_id = counter;
    counter += 1;
    pthread_mutex_unlock(&lock);            // release lock for message_id

    // create new message
    new_msg->source_id = *((int *) arg);
    new_msg->destination_id = rand() % 3;    // random destination (0-2)
    random_string = (char *)malloc(sizeof(char) * 80);
    random_string_gen(random_string);
    strcpy(new_msg->c_string, random_string);
    new_msg->queueing_time=0;
    new_msg->current_time=0;

    //printf("\n Source Id = %d \t Message Id = %d \t Destination Id= %d\n", new_msg->source_id, new_msg->message_id, new_msg-     >destination_id);

    // to write to bus_in_q
    fd = open(BUS_IN_Q, O_RDWR);
    if (fd == -1) 
    {
      printf(
          "file %s either does not exit or is currently used by an another user\n",
          BUS_IN_Q);
      exit(-1);
    }
    ret = write(fd, (void *) new_msg, sizeof(struct message));
    if (ret == 0)
      printf("\n Message sent is:Source Id = %d \t Message Id = %d \t Destination Id= %d\t String= %s\n",
          new_msg->source_id, new_msg->message_id, new_msg->destination_id, new_msg->c_string);

    while (time(NULL) < end_time && ret == -1) {
      printf("error occurred while first writing");
      errno = EINVAL;
      close(fd);

      sleep_time = (rand() % 10) + 1;
      usleep(sleep_time * 1000);

      fd = open(BUS_IN_Q, O_RDWR);
      ret = write(fd, (void *) new_msg, sizeof(struct message));
      if (ret == 0)
        printf("\nMessage sent is: Source Id = %d \t Message Id = %d \t Destination Id= %d\n",
            new_msg->source_id, new_msg->message_id, new_msg->destination_id);

    }
    close(fd);
    sleep_time = (rand() % 10) + 1;
    usleep(sleep_time * 1000);                // nap before sending next message
  }
  free(new_msg);
  return NULL;
}

// Bus daemon function to read messages for shared queue and transfer it to respective destination queues.

void* bus_daemon_func(void *arg) 
 {
  int fd, ret;
  int fd1, ret1;
  int x, sleep_time;
  time_t end_time = time(NULL) + DURATION + 1;
  struct message *new_msg;
  new_msg = (struct message*) malloc(sizeof(struct message));

  while (time(NULL) < end_time || ret == 0) 
  {
    fd = open(BUS_IN_Q, O_RDWR);
    if (fd == -1) {
      printf(
          "file %s either does not exist or is currently used by another user \n",
          BUS_IN_Q);
      exit(-1);
    }
    ret = read(fd, (void *) new_msg, sizeof(struct message));
    /*if (ret == 0)
      printf(
          "Message read by bus daemon. Source:%d\t message:%d\t Destination:%d\t Message:%s\n",
          new_msg->source_id, new_msg->message_id, new_msg->destination_id,
          new_msg->c_string);
     */
    while (time(NULL) < end_time && ret == -1) 
   {
      //printf("queue is empty\n");
      close(fd);
      errno = EINVAL;
      sleep_time = (rand() % 10) + 1;
      usleep(sleep_time * 1000);
      fd = open(BUS_IN_Q, O_RDWR);
      ret = read(fd, (void *) new_msg, sizeof(struct message));
      /* if (ret == 0)
        printf(
            "Message read by bus daemon. Source:%d\t message:%d\t Destination:%d\t Message:%s\n",
            new_msg->source_id, new_msg->message_id, new_msg->destination_id,
            new_msg->c_string);
      */

    }
    close(fd);
    // SEND MESSAGE TO DESTINATION
    x = new_msg->destination_id;
    switch (x)  
   {
    case 0:
      fd1 = open(BUS_OUT_Q1, O_RDWR);
      if (fd1 == -1) 
        {
        printf(
            "file %s either does not exist or is currently used by another user \n",
            BUS_OUT_Q1);
        exit(-1);
      }
      break;
    case 1:
      fd1 = open(BUS_OUT_Q2, O_RDWR);
      if (fd1 == -1) 
        {
        printf(
            "file %s either does not exist or is currently used by another user \n",
            BUS_OUT_Q2);
        exit(-1);
      }
      break;
    case 2:
      fd1 = open(BUS_OUT_Q3, O_RDWR);
      if (fd1 == -1) 
      {
        printf(
            "file %s either does not exist or is currently used by another user \n",
            BUS_OUT_Q3);
        exit(-1);
      }
      break;
    }

    ret1 = write(fd1, (void *) new_msg, sizeof(struct message));
    while (time(NULL) < end_time && ret1 == -1) 
      {
      //printf("error occurred while first writing");
      close(fd1);
      errno = EINVAL;
      sleep_time = (rand() % 10) + 1;
      usleep(sleep_time * 1000);
      switch (x)  
      {
      case 0:
        fd1 = open(BUS_OUT_Q1, O_RDWR);
        if (fd1 == -1) 
        {
          printf(
              "file %s either does not exist or is currently used by another user \n",
              BUS_OUT_Q1);
          exit(-1);
        }
        break;
      case 1:
        fd1 = open(BUS_OUT_Q2, O_RDWR);
        if (fd1 == -1) 
          {
          printf(
              "file %s either does not exist or is currently used by another user \n",
              BUS_OUT_Q2);
          exit(-1);
        }
        break;
      case 2:
        fd1 = open(BUS_OUT_Q3, O_RDWR);
        if (fd1 == -1) 
         {
          printf(
              "file %s either does not exist or is currently used by another user \n",
              BUS_OUT_Q3);
          exit(-1);
        }
        break;
      }
      // fd1 = open(BUS_OUT_Q1, O_RDWR);
      ret1 = write(fd1, (void *) new_msg, sizeof(struct message));
    }
    close(fd1);
  }

  free(new_msg);
  return NULL;
}

// Receiver function
void* receiver_func(void *arg) 
 {
  int fd, ret, sleep_time;
  int k = *((int *) arg) + 1;
  struct message *new_msg;
  new_msg = (struct message*) malloc(sizeof(struct message));
  time_t end_time = time(NULL) + DURATION +1;

  char device[20];
  sprintf(device, "/dev/bus_in_q%d", k); // puts string into buffer

  while (time(NULL) < end_time || ret == 0) 
   {
    fd = open(device, O_RDWR);
    if (fd == -1) {
      printf(
          "file %s either does not exist or is currently used by another user \n",
          device);
      exit(-1);
    }
    ret = read(fd, (void *) new_msg, sizeof(struct message));
    if (ret == 0)
    {
      quet = (new_msg->queueing_time) /2595;                               //accumulated queueing time/CPU cylces
      printf(
          "Message Received Source:%d\t message:%d\t Destination:%d\t Message:%s\t Queueing Time %ldus\n",
          new_msg->source_id, new_msg->message_id, new_msg->destination_id,
          new_msg->c_string, quet);
      close(fd);
    } else 
      {
      close(fd);
      errno = EINVAL;
      sleep_time = (rand() % 10) + 1;
      usleep(sleep_time * 1000);
    }

    // printf("End of receiver..\n");
    // close(fd);
  }
  free(new_msg);
  return NULL;

}

int main(void) 
  {
  int i = 0;
  int k = 0;
  int err, rec, rev;

  if (pthread_mutex_init(&lock, NULL) != 0) 
  {
    printf("\n mutex init failed\n");
    return 1;
  }

  while (i < SENDER_T_CNT) 
  {
    int *arg = malloc(sizeof(int));
    if (arg == NULL)
      printf("Couldn't allocate memory for thread arg.\n");
    *arg = i;

    err = pthread_create(&(sender_t[i]), NULL, &sender_func, arg);

    if (err != 0)
      printf("\ncan't create sender thread :[%s]", strerror(err));
    i++;
  }

  rec = pthread_create(&bus_daemon, NULL, &bus_daemon_func, NULL);
  if (rec != 0)
    printf("\ncan’t create bus daemon thread: [%s]", strerror(rec));

  while (k < RECEIVER_T_CNT) 
   {
    int *arg2 = malloc(sizeof(int));
    if (arg2 == NULL)
      printf("Countn't allocate memory for thread arg2.\n");
    *arg2 = k;
    rev = pthread_create(&(receiver_t[k]), NULL, &receiver_func, arg2);
    if (rev != 0)
      printf("\nCan not create receiver thread: [%s]", strerror(rev));
    k++;
  }

  int j = 0;
  while (j < SENDER_T_CNT)
    pthread_join(sender_t[j++], NULL);

  pthread_join(bus_daemon, NULL);

  int m = 0;
  while (m < RECEIVER_T_CNT)
    pthread_join(receiver_t[m++], NULL);

  pthread_mutex_destroy(&lock);
  return 0;
}

// REFERENCES:
// www.stackoverflow.com/questions/7684359/using-nanosleep-in-c
// www.thegeekstuff.com/2012/05/c-mutex-examples/
