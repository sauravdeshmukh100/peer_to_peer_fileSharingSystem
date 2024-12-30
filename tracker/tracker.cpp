// tracker.cpp
#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <vector>
#include <string>
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <sstream>
#include <cstdlib>
#include "tracker_functions.h"
#include "read_tracker.h"

// Maps to store users and groups
map<string, pair<string, bool>> users; // user_id -> hashed password
map<string, Group> groups;             // group_id -> Group

vector<std::pair<std::string, int>> trackers;

void masterTracker(int port, const std::string &logFile, const std::string &subTrackerIP, int subTrackerPort)
{
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        std::cerr << "Error binding to port" << std::endl;
        return;
    }

    if (listen(serverSocket, 5) < 0)
    {
        std::cerr << "Error listening on socket" << std::endl;
        return;
    }

    std::cout << "Master tracker running on port " << port << std::endl;

    int connectionCount = 0;

    while (true)
    {
        int clientSocket = accept(serverSocket, nullptr, nullptr);

        // Load balancing: Alternate between master and sub-tracker
        // if (connectionCount % 2 == 1)
        // {
        //     // Forward to sub-tracker
        //     int subSocket = socket(AF_INET, SOCK_STREAM, 0);
        //     sockaddr_in subAddr = {0};
        //     subAddr.sin_family = AF_INET;
        //     subAddr.sin_port = htons(subTrackerPort);
        //     inet_pton(AF_INET, subTrackerIP.c_str(), &subAddr.sin_addr);

        //     if (connect(subSocket, (struct sockaddr *)&subAddr, sizeof(subAddr)) == 0)
        //     {
        //         sendLogFile(subSocket, logFile); // Synchronize log
        //         close(subSocket);
        //         writeToLog(logFile, "Client redirected to sub-tracker");
        //     }
        // }
        // else
        // {

        if (clientSocket < 0)
        {
            std::cerr << "Error accepting connection" << std::endl;
            continue;
        }

        cout << "clientsocket is " << clientSocket << endl;

        std::thread th1(handleClient, clientSocket, "master");
        th1.detach(); // Detach the thread to run independently
        // }

        // connectionCount++;
    }

    close(serverSocket);
}

int connectTomasterTracker(const string &ip, int port)
{
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (clientSocket < 0)
    {
        cout << "Error creating client socket" << endl;
        ;
        return -1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serverAddress.sin_addr);

    // Connect to master tracker
    if (connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        cout << "Error connecting to tracker having ip " + ip + " and port " + to_string(port) << endl;
        return -1;
    }

    return clientSocket;
}

void subTracker(int port, const std::string &logFile)
{
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, 5);

    std::cout << "Sub-tracker running on port " << port << std::endl;

    while (true)
    {

        // build connection with master tracker

        // if connection successfull then mastersocket will be +ve number else -1

        if (mastersocket < 0)
        {
            mastersocket = connectTomasterTracker(trackers[0].first, trackers[0].second);
            if (mastersocket < 0)
            {
                cout << "can't connect to master" << endl;
            }

            else
            {
                cout<<"connected to master"<<endl;
                std::thread th2(handlemaster, std::ref(mastersocket));
                th2.detach(); // Detach the thread to run independently
            }
        }

        // else  create new thread to recieve data from master

        int clientSocket = accept(serverSocket, nullptr, nullptr);

        if (clientSocket < 0)
        {
            std::cerr << "Error accepting connection" << std::endl;
            continue;
        }

        cout << "clientsocket is " << clientSocket << endl;

        std::thread th1(handleClient, clientSocket, "subtracker");
        th1.detach(); // Detach the thread to run independently
    }
    close(serverSocket);
    // close(mastersocket);
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cerr << "Usage: ./tracker <tracker_info.txt> <tracker_no>" << std::endl;
        return -1;
    }

    std::string trackerInfoFile = argv[1];
    int trackerNo = std::stoi(argv[2]);

    // Read tracker info from file
    std::ifstream trackerInfo(trackerInfoFile);
    if (!trackerInfo.is_open())
    {
        std::cerr << "Error: Unable to open tracker info file." << std::endl;
        return -1;
    }

    string ip;
    int port;

    while (trackerInfo >> ip >> port)
    {
        trackers.emplace_back(ip, port);
    }
    trackerInfo.close();

    if (trackerNo < 0 || trackerNo >= trackers.size())
    {
        std::cerr << "Invalid tracker number." << std::endl;
        return -1;
    }

    // Determine role based on tracker number
    logFile = "log" + std::to_string(trackerNo) + ".txt";

    if (trackerNo == 0)
    {
        // Master tracker
        if (trackers.size() < 2)
        {
            std::cerr << "Insufficient tracker information for sub-tracker." << std::endl;
            return -1;
        }
        std::string subTrackerIP = trackers[1].first;
        int subTrackerPort = trackers[1].second;
        masterTracker(trackers[0].second, logFile, subTrackerIP, subTrackerPort);
    }
    else
    {
        // Sub-tracker
        subTracker(trackers[trackerNo].second, logFile);
    }

    return 0;
}
