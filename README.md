# Photos Album
## Information about application
In this Linux project, I built an ATM system with concurrent processes and IPC. It includes three processes: ATM, DB server, and DB Editor. The DB server handles authentication, PIN encryption, balance inquiries, and updates. A message queue facilitates communication, and the database is stored as a CSV file.
## How to run
gcc main.c -o main
gcc dbeditor.c -o dbeditor
gcc processone.c -o processone
