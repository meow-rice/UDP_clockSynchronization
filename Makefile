client: client.c
	gcc client.c -lm -o client

clean:
	rm *.out *.exe || echo "delete executables"
	rm ./client || echo "delete client"
	rm ./server || echo "delete server"
