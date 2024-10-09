/*
Joseph Marques 101218139
Sam Micheal 101229942
SYSC4001 Assignment 3
This represents the DB server process
*/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/sem.h>

// The Database file name
#define DATABASEFILE "DataBase.csv"

// Purpose of message received
#define CHECKACCOUNTNUMBERANDPIN 0
#define CHECKBALANCE 1
#define WITHDRAW 2
#define ADJUSTFILE 3
#define UPDATEDB 4

// Purpose of message sent
#define OK -1
#define PINWRONG -2
#define BLOCKED -3
#define NEWBALANCE -4
#define NSF -5

// The message struct
typedef struct account_info {
   char account_number[6]; // Account number
   int pin; // Pin
   float funds_available; // Funds available
   int attempts; // Attempts to get into account
   struct account_info *next; // To point to next
} account_info_t;

// The message struct
typedef struct message {
   long type; // Type of message (who needs it)
   int account_number; // Account number
   int pin; // Pin
   float amount; // Amount (multipurpose (e.g. used for balance, funds, ...))
   int purpose; // Used to indicate response from processone
} message_t;

// Queue to keep track of all accounts and their info
typedef struct queue {
   account_info_t *front;
   account_info_t *rear;
   int size;
} queue_t;

// Create an empty queue
queue_t *create_queue(void) {
   // Create space then assign values
   queue_t *blankQueue = (queue_t *) malloc(sizeof(queue_t));
   assert(blankQueue != NULL);
   blankQueue->front = NULL;
   blankQueue->rear = NULL;
   blankQueue->size = 0;
   return blankQueue;
}

// Create and fill an account info struct
account_info_t *create_account_info_entry(char account_number[], int pin, float funds_available){ 
   // Create space then assign values
   account_info_t *current = (account_info_t *)malloc(sizeof(account_info_t));
   assert(current != NULL);
   strcpy(current->account_number, account_number);
   current->pin = pin;
   current->funds_available = funds_available;
   current->attempts = 0;
   current->next = NULL;
   return current;
}

// Add to end of queue
void enqueue(queue_t *current_queue, account_info_t *current_account_info) {
   if (current_queue->front == NULL || current_queue->rear == NULL) { // If no  items in the queue
       current_queue->front = current_account_info;
   }
   else {
       current_queue->rear->next = current_account_info;
   }
   current_queue->size += 1; // Increment size
   current_queue->rear = current_account_info;
}

// Read DataBase and create the queue with all the accounts and their info
void read_db_create_queue(queue_t *current_queue, const char* file_name) {
   FILE *file = fopen(file_name, "r");
   if(file == NULL) {
       perror("file error");
       exit(EXIT_FAILURE);
   }
   account_info_t *current_account_info;
   char line[1024];
   fgets(line, 1024, file); // Skip header line
   while (fgets(line, 1024, file)) {
       size_t len = strlen(line); // Remove trailing character at the end
       if (len > 0 && line[len-1] == '\n') {
           line[len-1] = '\0'; // Replace the newline character at the end \n with \0
       }
       char *token = strtok(line, ",");
       float info[3]; // Create an array
       int i = 0; // Used to index array
       char account_number_str[6];
       while (token) { // While there is still data continue
           if (i == 0) {
                strcpy(account_number_str, token); // Copy the account number as a string
            } else {
                info[i - 1] = atof(token); // Adjust the value to an int
            }
            token = strtok(NULL, ",");
            i++;
       }
       if(strncmp(account_number_str, "X", 1) != 0) {
            int convert = atoi(account_number_str); 
            sprintf(account_number_str, "%05d", convert);
       }
       account_info_t *current_account_info = create_account_info_entry(account_number_str, info[0], info[1]);
       if(current_account_info != NULL) {
           enqueue(current_queue, current_account_info); // Add to queue
       }
   }
   fclose(file);
}

// Update the database csv file
void update_file(queue_t *current_queue, const char* file_name) {
   FILE *file = fopen(file_name, "w"); 
   if(file == NULL) {
       perror("file error");
       exit(EXIT_FAILURE);
   }
   fprintf(file, "Account No.,Encoded PIN,Funds available\n"); // First row of file
   account_info_t *current = current_queue->front;
   int size = current_queue->size;
   while(size) { // Loop queue (accounts in queue) and print their info one by one
       fprintf(file, "%s,%d,%.2f\n", current->account_number, current->pin, current->funds_available);
       current = current->next; // Go to next account
       size -= 1;
   }
   fclose(file);
}

int main(int argc, char *argv[]) {
   // Check for arguments
   if(argc != 2) {
       perror("argc error");
       exit(EXIT_FAILURE);
   }

   // Initalize some variables
   queue_t *queue; // Used for queue
   int msgid_db = (int) strtol(argv[1], NULL, 10); // id for message queue which was passed as argument and now converted to int
   message_t message_received; // Used for info on message receieved
   message_t message_sent; // Used for info on message sent
   account_info_t account_info; // Used to hold info on current account working on
   char account_number[6]; // Used to carry converted int
   int new_account = 1; // Check if it was a new account accessed by db editor or an exisiting account

   // Create empty queue and file it with info
   queue = create_queue();
   read_db_create_queue(queue, DATABASEFILE);

   while(1) { // Wait till a message is going to be received
       if(msgrcv(msgid_db, &message_received, sizeof(message_received)-sizeof(long), 2, 0) != -1) { // Receive message of type 2 (indicating for processone)
           if(message_received.purpose == CHECKACCOUNTNUMBERANDPIN) { // Check if purpose of comment was to verify account
               account_info_t *current = queue->front; // Will be used to loop
               sprintf(account_number, "%05d", message_received.account_number); // Convert to string as input was a number
               while(current != NULL) { // Loop looking for account number
                   if(!strcmp(current->account_number, account_number)) { // Look for account number in database
                       // Take needed info in message and put in account_info to allow another place to save and use info quicker
                       strcpy(account_info.account_number, current->account_number);
                       account_info.pin = current->pin;
                       account_info.funds_available = current->funds_available;
                       account_info.next = NULL; // Next is not needed for this 
                       if(account_info.pin == (message_received.pin - 1)) { // Verify the pin with the encryption
                           message_received.purpose = OK; // If pin matches return ok
                       }
                       else { // Pin didnt match
                           message_received.purpose = PINWRONG; // Return that pin was wrong to atm
                           current->attempts += 1; // Increment attempts wrong
                           if(current->attempts >= 3) { // If 3 (or more) attempts wrong block account
                               current->account_number[0] = 'X'; // Change first char to X to block account
                               message_received.purpose = BLOCKED; // Indicate that account was blocked to atm
                               update_file(queue, DATABASEFILE); // Update the database csv file
                           }
                       }
                       break; // Account found and changes good so leave loop
                   }
                   current = current->next; // Next account
               }
           }
           else if(message_received.purpose == CHECKBALANCE) { // If needed to check balance
               message_received.amount = account_info.funds_available; // Return the current balance
           }
           else if(message_received.purpose == WITHDRAW) { // If needed to withdraw
               if(account_info.funds_available >= message_received.amount) { // Check if funds available
                   account_info.funds_available -= message_received.amount; // Do deduction to funds
                   account_info_t *current = queue->front; // Used to loop
                   while(current != NULL) { // Look for account in order to update it in the queue
                       if(!strcmp(current->account_number,account_info.account_number)) { // Look for account in database
                           current->funds_available = account_info.funds_available;
                           break; // Found so exit loop
                       }
                       current = current->next; // Next account
                   }
                   // Set amount and purpose of message to atm and update database file
                   message_received.amount = account_info.funds_available;
                   message_received.purpose = NEWBALANCE;
                   update_file(queue, DATABASEFILE);
               }
               else { // If not enough funds
                   message_received.purpose = NSF; // Return NSF to atm indicating they dont have the funds
               }
           }
           else if(message_received.purpose == ADJUSTFILE) { // Adjust file when exiting to ensure everything is correct (extra step but not needed)
                update_file(queue, DATABASEFILE);
           }
           else if(message_received.purpose == UPDATEDB) { // If update db message was receieved
                // Loop the queue and update what is needed
                account_info_t *current = queue->front;
                message_received.pin -= 1; // To encrypt
                sprintf(account_number, "%05d", message_received.account_number); // Convert to string as input was a number
                int size = queue->size;
                while(size) {
                    if(!strcmp(current->account_number, account_number)) { // Look for account in database (if user wanted to update existing account)
                        // Update exisiting pin and amount 
                        current->pin = message_received.pin; // Update exisiting pin
                        current->funds_available = message_received.amount;
                        new_account = 0; // It is not a new account being added rather an old one being updated
                        if(!strcmp(account_info.account_number, account_number)) { // If the account being updated is account currently being worked on
                            // Update exisiting pin and amount 
                            account_info.pin = message_received.pin;
                            account_info.funds_available = message_received.amount;
                        }
                        break; // Leave as found
                    }
                    size -= 1;
                    current = current->next; // Next account
                }
                if(new_account) { // There was a new account
                    // Take new account info and add it to queue
                    account_info_t *new_account_info = (account_info_t *)malloc(sizeof(account_info_t));
                    strcpy(new_account_info->account_number, account_number);
                    new_account_info->pin = message_received.pin;
                    new_account_info->funds_available = message_received.amount;
                    new_account_info->attempts = 0;
                    new_account_info->next = NULL;
                    enqueue(queue, new_account_info);
                }
                new_account = 1; // Change for it to be set for next use (check if an old or new account)
                update_file(queue, DATABASEFILE); // Update database file
           }
           if(message_received.purpose != UPDATEDB) { // If a message is needed to be sent to atm (meaning it isnt an UPDATE_DB message that was received)
                message_received.type = 1; // Change type to 1 as that is what atm takes
                if(msgsnd(msgid_db, &message_received, sizeof(message_received)-sizeof(long), 0) == -1) { // Send the message to the message queue
                    perror("msgsnd error");
                    exit(EXIT_FAILURE);
                }
            }
       }
   }
   return 0;
}
