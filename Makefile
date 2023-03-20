all: client server

client: client.cpp client.h
	g++ client.cpp -lm -o client

server: server.cpp client.h
	g++ server.cpp -lm -o server

clean:
	rm *.out *.exe || echo "delete executables"
	rm ./client || echo "delete client"
	rm ./server || echo "delete server"
