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
// #include "read_tracker.h"

using namespace std;

// Maps to store users and groups
map<string, pair<string, bool>> users; // user_id -> hashed password
map<string, Group> groups;             // group_id -> Group

struct TrackerInfo
{
    string ip;
    int client_port;
    int sync_port;
};

vector<TrackerInfo> trackers;



// string logFile;      // Global log file name

void processLogEntry(const string &msg)
{
    // Placeholder: Update tracker state based on log entry
    cout << "Processing sync message: " << msg << endl;
}



std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::stringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

void runTracker(int clientPort, int syncPort, const string &logFile, int trackerNo, string otherIp, int otherSyncPort)
{
    // // Initialize state from log
    // readLog(logFile); don't need to intialise the state i will ask to other tracker rather 
    // cout << "Tracker " << trackerNo << " state initialized from " << logFile << endl;

    // Client server socket setup
    int clientServerSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in clientAddr = {0};
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_port = htons(clientPort);
    clientAddr.sin_addr.s_addr = INADDR_ANY;
    bind(clientServerSocket, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
    listen(clientServerSocket, 5);
    cout << "Tracker " << trackerNo << " listening on client port " << clientPort << endl;

     bool syncComplete = false;  // This flag will indicate if tracker-to-tracker sync is successful

     // Start a thread to handle tracker-to-tracker sync connections
     thread trackerSyncThread([=, &logFile, &syncComplete]() {
        while (true) {
            if (trackerNo == 0) {
                // Master Tracker: Listen for incoming sync connections from other trackers
                int trackerServerSocket = socket(AF_INET, SOCK_STREAM, 0);
                sockaddr_in syncAddr = {};
                syncAddr.sin_family = AF_INET;
                syncAddr.sin_addr.s_addr = INADDR_ANY;
                syncAddr.sin_port = htons(syncPort);
                bind(trackerServerSocket, (sockaddr*)&syncAddr, sizeof(syncAddr));
                listen(trackerServerSocket, 5);
                cout << "Sync listener (thread) waiting on port " << syncPort << " for other trackers\n";

                while (true) {
                    syncSocket = accept(trackerServerSocket, nullptr, nullptr);
                    if (syncSocket >= 0) {
                        cout << "Sync connection accepted from another tracker.\n";
                        syncComplete = true;  // Mark sync as complete
                        handleTrackerSync(syncSocket, logFile,false);
                        syncSocket = -1;  // Reset sync socket
                        // syncComplete = true;  // Mark sync as complete
                        cout<<"marked synccomplete as "<<syncComplete<<endl;
                    } else {
                        cout << "Sync connection failed. Retrying...\n";
                        this_thread::sleep_for(chrono::seconds(5));  // Retry after 5 seconds
                    }
                }
                close(trackerServerSocket);
            } else {
                // Sub Tracker: Try to connect to the master tracker
                while (true) {
                    int syncSocket = socket(AF_INET, SOCK_STREAM, 0);
                    sockaddr_in masterAddr = {};
                    masterAddr.sin_family = AF_INET;
                    masterAddr.sin_port = htons(otherSyncPort);
                    inet_pton(AF_INET, otherIp.c_str(), &masterAddr.sin_addr);

                    if (connect(syncSocket, (sockaddr*)&masterAddr, sizeof(masterAddr)) >= 0) {
                        cout << "Connected to master tracker for sync.\n";
                        syncComplete = true;  // Mark sync as complete
                        handleTrackerSync(syncSocket, logFile,false);
                        
                        break;  // Exit the loop once sync is complete
                    } else {
                        close(syncSocket);
                        cout << "Failed to connect to master tracker. Retrying...\n";
                        this_thread::sleep_for(chrono::seconds(5));  // Retry after 5 seconds
                    }
                }
            }
        }
    });

    trackerSyncThread.detach();  // Detach sync thread to keep it running indefinitely

    // Main loop for accepting client connections after sync is complete
    while (!syncComplete) {
        cout << "Waiting for sync to complete before accepting clients...\n";
        this_thread::sleep_for(chrono::seconds(1));  // Wait until sync is complete
    }
   cout<<"going sleep for 5 sec"<<endl;
    sleep(5);
    // Accept client connections
    while (true)
    {
        int clientSocket = accept(clientServerSocket, nullptr, nullptr);
        if (clientSocket >= 0)
        {
            thread(handleClient, clientSocket, "Tracker " + to_string(trackerNo),trackerNo).detach();
        }
    }
    close(clientServerSocket);
}
int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        cerr << "Usage: ./tracker <tracker_info.txt> <tracker_no>" << endl;
        return -1;
    }

    string trackerInfoFile = argv[1];
    int trackerNo = stoi(argv[2]);
    currentTrackerId=trackerNo;
    ifstream trackerInfo(trackerInfoFile);
    if (!trackerInfo.is_open())
    {
        cerr << "Error opening tracker info file" << endl;
        return -1;
    }

    string ip;
    int client_port, sync_port;
    while (trackerInfo >> ip >> client_port >> sync_port)
    {
        trackers.push_back({ip, client_port, sync_port});
    }
    trackerInfo.close();

    if (trackerNo < 0 || trackerNo >= trackers.size())
    {
        cerr << "Invalid tracker number" << endl;
        return -1;
    }

    ::logFile = "log" + to_string(trackerNo) + ".txt";
    std::ofstream ofs(::logFile, std::ofstream::out | std::ofstream::trunc); // Clear the log file
    ofs.close();
    int clientPort = trackers[trackerNo].client_port;
    int syncPort = trackers[trackerNo].sync_port;
    int otherTrackerNo = (trackerNo == 0) ? 1 : 0;
    string otherIp = trackers[otherTrackerNo].ip;
    int otherSyncPort = trackers[otherTrackerNo].sync_port;

    runTracker(clientPort, syncPort, logFile, trackerNo, otherIp, otherSyncPort);
    return 0;
}