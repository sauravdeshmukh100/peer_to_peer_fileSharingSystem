// #include <iostream>
#include <map>
// #include <vector>
// #include <string>
// #include <netinet/in.h>
// #include <sys/socket.h>
// #include <unistd.h>
// #include <thread>
// #include <cstring>
// #include <sstream>


// #include <algorithm> // Include for  find
#include "read_tracker.h"
#include "tracker_functions.h"



// using namespace std;




// Maps to store users and groups
map<string, pair<string, bool>> users; // user_id -> hashed password
map<string, Group> groups;             // group_id -> Group

// Hashing function for passwords
//  string hashPassword(const  string& password) {
//     unsigned char hash[SHA_DIGEST_LENGTH];
//     SHA1(reinterpret_cast<const unsigned char*>(password.c_str()), password.length(), hash);
//      string hashedPassword;
//     for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
//         char buf[3];
//         snprintf(buf, sizeof(buf), "%02x", hash[i]);
//         hashedPassword += buf;
//     }
//     return hashedPassword;
// }




void runTracker(const std::string &ip, int port) {
    // Create a socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Failed to create socket." << std::endl;
        return;
    }

    // Set the SO_REUSEADDR option before binding
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to set SO_REUSEADDR option." << std::endl;
        close(serverSocket);
        return;
    }

    // Setup the address structure
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    // Use inet_pton for better IP handling
    if (inet_pton(AF_INET, ip.c_str(), &serverAddress.sin_addr) <= 0) {
        std::cerr << "Invalid IP address: " << ip << std::endl;
        close(serverSocket);
        return;
    }

    // Bind the socket to the IP and port
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "Binding failed on IP: " << ip << ", Port: " << port << std::endl;
        close(serverSocket);
        return;
    }

    // Listen for incoming connections
    if (listen(serverSocket, 5) < 0) {
        std::cerr << "Failed to listen on the socket." << std::endl;
        close(serverSocket);
        return;
    }

    std::cout << "Server is running on IP: " << ip << ", Port: " << port << std::endl;

    // Accept and handle client connections
    while (true) {
        int clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket < 0) {
            std::cerr << "Error accepting connection" << std::endl;
            continue;
        }

        std::thread th1(handleClient, clientSocket);
        th1.detach();  // Detach the thread to run independently
    }

    close(serverSocket);
}














int main(int argc, char *argv[])
{
    if (argc != 3) {
        std::cerr << "Usage: ./tracker <tracker_info_file> <tracker_no>" << std::endl;
        return 1;
    }

    std::string trackerInfoFile = argv[1]; // Tracker info file path
    int trackerNo = std::stoi(argv[2]);    // Tracker number

    // Read the tracker info from the file
    std::vector<TrackerInfo> trackers = readTrackerInfo(trackerInfoFile);
    if (trackers.empty()) {
        std::cerr << "No trackers found in tracker info file." << std::endl;
        return 1;
    }

    // Check if the tracker number is valid
    if (trackerNo < 0 || trackerNo >= trackers.size()) {
        std::cerr << "Invalid tracker number provided." << std::endl;
        return 1;
    }

    // Start the server with the selected tracker info
    TrackerInfo selectedTracker = trackers[trackerNo];
    runTracker(selectedTracker.ip, selectedTracker.port);    // vector<int> v(10,-1);
    

   
    return 0;
}
