#include <stdlib.h>
#include <stdio.h>
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

// Purpose of message sent
#define UPDATEDB 4

// The message struct
typedef struct message {
    long type; // Type of message (who needs it)
    int account_number; // Account number
    int pin; // Pin
    float amount; // Amount (multipurpose (e.g. used for balance, funds, ...))
    int purpose; // Used to indicate response from processone
} message_t;

int main(void) {
    // Initalize some variables
    // Text below is used to take input
    char text_one[100];
    char text_two[100];
    char text_three[100];
    int account_number; // Used to store account number received
    int pin; // Used to store pin received
    float funds; // Used to store funds received
    int exit_sys = 0; // Used to exit system if required (extra step)
    key_t key; // Used for key to msg queue
    int msgid_db; // Used for iq to msg queue

    // Create key and use key to create message queue
    key = ftok("msgqueue", 1); // Create key for msgqueue
    if(key == -1) { // Check if ftok worked
        perror("ftok error");
        exit(EXIT_FAILURE);
    }
    msgid_db = msgget(key, IPC_CREAT | 0666); // Create msgqueue
    if(msgid_db == -1) { // Check if mssget worked
        perror("msgget error");
        exit(EXIT_FAILURE);
    }

    while(1) {
        while(1) { // Loop till valid input
            // Get valid input (valid as in 5 digit number) or check if user wants to exit
            printf("Please enter a 5 digit account number below for a new one or to update an existing one (e.g. 12345 or 00099) (or \"X\" to end):\n");
            fgets(text_one, sizeof(text_one), stdin);
            char *one = strchr(text_one, '\n');
            if(one != NULL) {
                *one = '\0';
            }
            char *one_X;
            account_number = strtol(text_one, &one_X, 10);
            if (*one_X != '\0') { // Check if a number
                if(strcmp(text_one, "X") == 0) { // User wants to exit
                    exit_sys = 1; // Set to allow exit
                    break; // Leave loop to exit
                }
                printf("Invalid input as it must be a number\n");
                continue;
            }
            if (strlen(text_one) != 5) { // Input is not 5 digits
                printf("Invalid input as it must 5 digits (e.g. 12345)\n");
                continue;
            }
            break; // Leave as valid input
        }
        if(exit_sys) { // Wants to exit
            break; // Leave loop
        }
        while(1) { // Loop till valid input
            // Get valid input (valid as in 3 digit number)
            printf("Please enter a 3 digit pin below (e.g. 123 or 007):\n");
            fgets(text_two, sizeof(text_two), stdin);
            char *two;
            pin = strtol(text_two, &two, 10);
            if (*two != '\0' && *two != '\n') { // Check if not number
                printf("Invalid input as it must be a number\n");
                continue;
            }
            if (strlen(text_two) != 4) { // Check if not 3 digits
                printf("Invalid input as it must 3 digits (e.g. 123)\n");
                continue;
            }
            break; // Leave as valid input
        }
        while(1) { // Loop till valid input
            // Get valid input (valid as in a number)
            printf("Please enter the funds available in this account (e.g. 123.45 or 71)\n");
                fgets(text_three, sizeof(text_three), stdin);
                char *three;
                funds = strtof(text_three, &three);
                if(*three != '\0' && *three != '\n') { // Not a number
                    printf("Invalid input, please enter a number (e.g. 123.45)\n");
                    continue;
                }
            break; // Leave as valid input
        }
        // Initiate and fill the message to be sent to db server (processone)
        message_t message_sent;
        message_sent.type = 2; // Want to send to processone
        message_sent.account_number = account_number;
        message_sent.pin = pin;
        message_sent.amount = funds;
        message_sent.purpose = UPDATEDB; // Purpose is to update database
        if(msgsnd(msgid_db, &message_sent, sizeof(message_sent)-sizeof(long), 0) == -1) { // Send the message to the message queue
            perror("msgsnd error");
            exit(EXIT_FAILURE);
        }
    }
    printf("System Exited\n");
    return 0;
}
