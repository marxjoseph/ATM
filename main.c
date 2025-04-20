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
#include <signal.h>


// States for ATM
#define REQUESTACCOUNTNUMBERANDPINSTATE 0
#define CHOOSEOPERATIONSTATE 1
#define BALANCESTATE 2
#define WITHDRAWSTATE 3

// Purpose of message sent
#define CHECKACCOUNTNUMBERANDPIN 0
#define CHECKBALANCE 1
#define WITHDRAW 2
#define ADJUSTFILE 3

// Purpose of message received
#define OK -1
#define PINWRONG -2
#define BLOCKED -3
#define NEWBALANCE -4
#define NSF -5

// The message struct
typedef struct message {
    long type; // Type of message (who needs it)
    int account_number; // Account number
    int pin; // Pin
    float amount; // Amount (multipurpose (e.g. used for balance, funds, ...))
    int purpose; // Used to indicate response from processone
} message_t;

// Send and receive messages from the message queue
float message_queue(int msgid_db, int account_number, int pin, float amount, int purpose) {
    message_t message_sent;
    message_t message_received;
    message_sent.type = 2; // Want to send to processone
    // Enter in account info and purpose of each message 
    message_sent.account_number = account_number;
    message_sent.pin = pin;
    message_sent.amount = amount;
    message_sent.purpose = purpose; // Used to send messages like UPDATE_DB, etc (as a int)
    if(msgsnd(msgid_db, &message_sent, sizeof(message_sent)-sizeof(long), 0) == -1) { // Send the message to the message queue
        perror("msgsnd error");
        exit(EXIT_FAILURE);
    }
    while(1) { // Wait till a message is going to be received
        if(msgrcv(msgid_db, &message_received, sizeof(message_received)-sizeof(long), 1, 0) != -1) { // Read type 1 messages
            // Check what message has to be returned and return what needs to be returned
            if(purpose == CHECKBALANCE) { // If it wanted to check balance return amount
                return message_received.amount;
            }
            else if(purpose == WITHDRAW) { // If it wanted to withdraw check if there was enough funds and if there was return amount if not NSF
                if(message_received.purpose == NEWBALANCE) {
                    return message_received.amount;
                }
                return NSF;
            }
            return message_received.purpose; // Otherwise return the purpose of the message (e.g. PIN_WRONG)
        }
    }
    perror("msgrcv error");
    exit(EXIT_FAILURE);
}  

int main(void) {
    // Initalize some variables
    key_t key; // Used as key to msg queue
    int msgid_db; // Used as msg queue id
    pid_t pid; // Used for fork
    pid_t pid_c; // Used to kill child process when execution is finished
    int state; // Used to keep track of each state
    // All text is used for receiving inputs
    char text_one[100];
    char text_two[100];
    char text_three[100];
    char text_four[100];
    int account_number; // Used for account number to send in message
    int pin; // Used for pin to send in message
    float msg; // Used as message response
    int operation_choice; // Used to see if they want balance or withdraw
    float withdraw_amount; // Used to hold amount they want to withdraw
    int exit_sys = 0; // Used to exit program if requested

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

    // First state is requesting account number
    state = REQUESTACCOUNTNUMBERANDPINSTATE;

    // Conver msgid_db to string to pass as argument
    int digits = snprintf(NULL, 0, "%d", msgid_db);
    char strmsgid_db[digits + 1];
    snprintf(strmsgid_db, sizeof(strmsgid_db), "%d", msgid_db);

    // Create child process and run according process
    pid = fork();
    if(pid < 0) { // Check if fork worked
        perror("fork error");
        exit(EXIT_FAILURE);
    }
    else if(pid == 0) { // Check if running child process
        pid_c = execl("./processone", "processone", strmsgid_db, (char *) NULL);
        perror("execl error");
        exit(EXIT_FAILURE);
    }
    else { // if running parent process
        while(1) {
            if(state == REQUESTACCOUNTNUMBERANDPINSTATE) { // If in asking for account number and pin state
                // Ask user for some info
                while(1) { // Repeat if input is invalid
                    // Ask for an account number and make sure input is valid (valid meaning a number), and check if they want to exit
                    printf("Please enter your account number below (leading zeros not required) (or \"X\" to end):\n");
                    fgets(text_one, sizeof(text_one), stdin);
                    char *one = strchr(text_one, '\n');
                    if(one != NULL) {
                        *one = '\0';
                    }
                    char *one_X;
                    account_number = strtol(text_one, &one_X, 10);
                    if (*one_X != '\0') {
                        if(strcmp(text_one, "X") == 0) { // Want to exit
                            exit_sys = 1; // Set to exit
                            break;
                        }
                        printf("Invalid input as it must be a number\n");
                        continue;
                    }
                    break; // Input valid leave loop
                }
                if(exit_sys) { // Want to exit
                    message_queue(msgid_db, 0, 0, 0, ADJUSTFILE); // Add extra step to confirm database file is matched
                    break; // Leave infinite loop
                }
                while(1) { // Repeat if input is invalid
                    // Ask for pin and make sure input is valid (valid meaning a number)
                    printf("Please enter your PIN below:\n");
                    fgets(text_two, sizeof(text_two), stdin);
                    char *two;
                    pin = strtol(text_two, &two, 10);
                    if (*two != '\0' && *two != '\n') {
                        printf("Invalid input as it must be a number\n");
                        continue;
                    }
                    break; // Input valid leave loop
                }
                msg = message_queue(msgid_db, account_number, pin, 0, CHECKACCOUNTNUMBERANDPIN); // Send a message to verify if the info is valid
                // Check if state stage is needed
                if(msg == OK) { // Pin is ok
                    printf("Pin is ok\n");
                    state = CHOOSEOPERATIONSTATE; // Go to choose operation state
                }
                else if(msg == BLOCKED) { // Account needs to be blocked
                    printf("The account number used above has been blocked (3 attempts used on this account that failed)\n");
                }
                else { // Invalid info
                    printf("Pin or account number is wrong\n");
                }
            }
            else if(state == CHOOSEOPERATIONSTATE) { // If in asking what operation state
                // Ask the user what operation they would like to perform
                while(1) { // Repeat if input is invalid
                    printf("Choose one of the options below by typing in their corresponding numbers (1 for Balance, 2 for Withdraw):\n1: Balance\n2: Withdraw\n");
                    fgets(text_three, sizeof(text_three), stdin);
                    char *three;
                    operation_choice = strtol(text_three, &three, 10);
                    if(*three != '\0' && *three != '\n') { // Input was invalid (not 1 or 2) (but was not even a number)
                        printf("Invalid input, please enter 1 for Balance or 2 for Withdraw\n");
                        continue;
                    }
                    // Change state of user depending on what choice they made
                    if(operation_choice == 1) { // If they want balance
                        state = BALANCESTATE; // Go to balance state
                        break; // Valid input so leave
                    }
                    else if(operation_choice == 2) { // If they want withdraw
                        state = WITHDRAWSTATE; // Go to withdraw state
                        break; // Valid input so leave
                    }
                    else { // Input was invalid (not 1 or 2) (was a number)
                        printf("Invalid input, please enter a 1 for Balance or 2 for Withdraw\n");
                    }
                }
            }
            else if(state == BALANCESTATE) { // If in balance state
                // Output balance
                msg = message_queue(msgid_db, account_number, pin, 0, CHECKBALANCE); // Send a message to check the balance
                printf("The balance is: %.2f\n", msg);
                state = REQUESTACCOUNTNUMBERANDPINSTATE; // Go back to wait for next customer
            }
            else if(state == WITHDRAWSTATE) { // If in withdraw state
                while(1) { // Repeat till valid input (valid meaning a number)
                    printf("How much would you like to withdraw\n");
                    fgets(text_four, sizeof(text_four), stdin);
                    char *four;
                    withdraw_amount = strtof(text_four, &four);
                    if(*four != '\0' && *four != '\n') {
                        printf("Invalid input must be a number\n");
                        continue;
                    }
                    break; // Valid input
                }
                msg = message_queue(msgid_db, account_number, pin, withdraw_amount, WITHDRAW); // Send a message to withdraw
                if(msg == NSF) { // If not enough funds
                    printf("NSF (not sufficient funds)\n");
                }
                else { // If funds are good
                    printf("Your funds are ok and your new balance is: %.2f\n", msg);
                }
                state = REQUESTACCOUNTNUMBERANDPINSTATE; // Go back to wait for next customer
            }
        }
        // System exited so print system closed and kill child process
        printf("System Exited\n");
        kill(pid_c, SIGTERM);
    }
    return 0;
}
