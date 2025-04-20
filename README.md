# Photos Album
## Information about application
In this Linux project, I built an ATM system with concurrent processes and IPC. It includes three processes: ATM, DB server, and DB Editor. The DB server handles authentication, PIN encryption, balance inquiries, and updates. A message queue facilitates communication, and the database is stored as a CSV file. 
## Account Info
The csv file has all account info. When logging in make sure to use the pin on the csv file plus 1. This was done to mock an encryption process.
## How to run
gcc main.c -o main
gcc dbeditor.c -o dbeditor
gcc processone.c -o processone
To use main:
./main
To use dbeditor must run main to fork processone and must run dbeditor:
./main
./dbeditor
Note: After every run please make sure all processes related to this application are shut down or rerun won't work.