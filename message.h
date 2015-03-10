struct message {
  int message_id;
  int source_id;
  int destination_id;
  char c_string[80];
  long queueing_time;
  long current_time;
};                          // to declare structure of message which is common to user space and kernel space
