# Process Communication via Pipe,Signal,Sockets
Computer process interact with the Admin and the Clients. 

Each Client request to computer,(on sockets) once received, will be added to the ReadyQueue. Once a Client
sends the “nullfile” string, it will be removed from the ActiveClient list, and the connection is closed.

Admin requests(via pipe and signal) :x, T, Q commands from Admin.
x - execute one client request at a time by removing from ready queue.
T- Terminate all clients
Q- quit the system
