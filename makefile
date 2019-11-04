all: Admin.exe Client.exe

Admin.exe : admin.cpp
	g++ -o Admin.exe admin.cpp -lpthread

Client.exe : Client.cpp
	g++ -o Client.exe Client.cpp