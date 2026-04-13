[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/47yOQO2Q)
## CSC 435 Programming Assignment 4 (Fall 2025)
**Jarvis College of Computing and Digital Media - DePaul University**

**Student**: Riddhiman Raguraman (rraguram@depaul.edu) 
**Solution programming language**: C++

<h3 align="center">How to run the program</h3>

### Step - 1

Ensure you have cmake if not run this code

	sudo apt install -y cmake
	
And then ensure you have gcc version 14+ if not run this code
	
	sudo apt install -y gcc-14 g++-14

### Step - 2

Clone this repository 
	
	git clone https://github.com/transcendental-software/csc-435-pa4-RiddhimanRaguraman.git

### Step - 3

Traverse to the app-cpp folder

    cd app-cpp/ 

### Step - 4

Run this cmake code to build the project
	
	cmake -S . -B build
	
This would create a build folder for the binaries now build the file
	
	cmake --build build

### Step - 5

This should create build/pa1 now Run the file-retrivel-engine server first with a dummy port number
	
	./build/file-retrieval-server 12345

### Step - 6 

Now run the Clients

    ./build/file-retrieval-client
    > connect 127.0.0.1 12345
    Connection successful!
    > get_info
    client ID: 1

### Step - 7

alternatively you can also run benchmark to see the results 

    ./build/file-retrieval-benchmark 127.0.0.1 12345 2 ../datasets/dataset1_client_server/2_clients/client_1 ../datasets/dataset1_client_server/2_clients/client_2

### Step - 8
Use the Commands index, search and quit to operate on any of the Clients (side arrows and up arrow key would also work just like any cli environment)


### Step - 9 (Optional)

Once done with the program run the Cleanme.sh to clean up the build files 

	./CleanMe.sh
	
