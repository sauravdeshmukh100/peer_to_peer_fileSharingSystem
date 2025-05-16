#include <string>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <cstring>
#include <sstream>
#include <iostream> // For std::cout, std::cerr
#include <fstream>  // For std::ofstream
#include <set>
#include <sys/stat.h> // For struct stat and mkdir
#include <unordered_map>
#include <ctime>
// #include <openssl/evp.h> // For EVP functions
#include <openssl/sha.h>
#include <fcntl.h> // For open()
                   // For read(), close()
#include <atomic>
#include <algorithm> // Include for  find
using namespace std;

// In tracker.cpp (global variables)
atomic<long> globalSeqNum{0}; // Shared atomic counter
int currentTrackerId;         // 0 for master, 1 for sub

struct Group
{
    string owner;                   // Owner of the group
    vector<string> members;         // Members in the group
    vector<string> pendingRequests; // Pending join requests
    // map<string, pair<string, vector<string>>> sharableFiles; // map of file_name -> SHA1 hash
    map<string, vector<string>> file_owner;                     // userid to fileShas              // map from owner to file path
    map<string, pair<string, vector<string>>> fileSha_to_peers; // hash -> {name, vector<peers>}
};

// Function to handle client requests
extern map<string, pair<string, bool>> users; // user_id -> hashed password
extern map<string, Group> groups;             // group_id -> Group

std::mutex logMutex;
string logFile;
// Global socket for sync communication
int syncSocket = -1;
std::mutex fileMutex; // To ensure thread safety in case of concurrent access

void writeToLog(const std::string &logFile, const std::string &entry)
{
    std::lock_guard<std::mutex> lock(logMutex);
    std::ofstream log(logFile, std::ios::app);
    if (log.is_open())
    {
        log << entry << std::endl;
    }

    log.close();
}

void applyLogEntry(const string &entry)
{
    istringstream iss(entry);
    long seq;
    int sourceTracker;
    string command;
  cout<<"inside applyLogEntry"<<endl;
  cout<<"entry passed is "<<entry<<endl;
    iss>> command;

    // if (seq <= globalSeqNum)
    //     return; // Skip old entries

    // lock_guard<mutex> lock(logMutex);
    // globalSeqNum = seq; // Update global sequence

    cout<<"command is "<<command<<endl;
   
    if (command == "User_created")
    {
        cout<<"inside user created"<<endl;
        string user_id, password;
        iss >> user_id >> password;
        cout<<"user_id "<<user_id<<endl;
        users[user_id].first = password;
        users[user_id].second = false;
    }
    else if (command == "Login")
    {
        cout<<"inside login in apply log "<<endl;
        string user_id, password, client_ip, client_port;
        iss >> user_id >> password >> client_ip >> client_port;
        users[user_id].second = true;
    }
    else if (command == "create_group")
    {
        string group_id, owner_name;
        iss >> group_id >> owner_name;
        groups[group_id] = {owner_name, {}, {}};
        groups[group_id].members.push_back(owner_name);
    }
    else if (command == "join_group")
    {
        string user_id, group_id;
        iss >> user_id >> group_id;
        groups[group_id].pendingRequests.push_back(user_id);
    }
    else if (command == "User_added")
    {
        string user_id, group_id;
        iss >> user_id >> group_id;
        auto &pending = groups[group_id].pendingRequests;
        auto it = find(pending.begin(), pending.end(), user_id);
        if (it != pending.end())
        {
            groups[group_id].members.push_back(user_id);
            pending.erase(it);
        }
    }
    else if (command == "Left_group")
    {
        string user_id, group_id, client_ip, client_port;
        iss >> user_id >> group_id >> client_ip >> client_port;
        auto &members = groups[group_id].members;
        auto it = find(members.begin(), members.end(), user_id);
        if (it != members.end())
        {
            vector<string> currentSHAs = groups[group_id].file_owner[user_id];
            string peer = user_id + " " + client_ip + " " + client_port;
            for (auto &sha : currentSHAs)
            {
                auto &peers = groups[group_id].fileSha_to_peers[sha].second;
                auto peer_it = find(peers.begin(), peers.end(), peer);
                if (peer_it != peers.end())
                    peers.erase(peer_it);
                if (peers.empty())
                    groups[group_id].fileSha_to_peers.erase(sha);
            }
            groups[group_id].file_owner.erase(user_id);
            if (members.size() == 1)
            {
                groups.erase(group_id);
            }
            else
            {
                if (groups[group_id].owner == *it)
                {
                    for (const auto &m : members)
                    {
                        if (m != *it)
                        {
                            groups[group_id].owner = m;
                            break;
                        }
                    }
                }
                members.erase(it);
            }
        }
    }
    else if (command == "upload_file")
    {
        string upload_no, fileHash, curr_client, client_ip, client_port, group_id, fileName, filesize;
        iss >> upload_no >> fileHash >> curr_client >> client_ip >> client_port >> group_id >> fileName >> filesize;
        string temp = curr_client + " " + client_ip + " " + client_port;
        if (upload_no == "upload")
        {
            groups[group_id].fileSha_to_peers[fileHash].first = fileName + " " + filesize;
            groups[group_id].fileSha_to_peers[fileHash].second.push_back(temp);
            groups[group_id].file_owner[curr_client].push_back(fileHash);
        }
        else if (upload_no == "2nd")
        {
            groups[group_id].fileSha_to_peers[fileHash].second.push_back(temp);
            groups[group_id].file_owner[curr_client].push_back(fileHash);
        }
    }
    else if (command == "stop_share")
    {
        string file_sha, group_id, curr_client;
        iss >> file_sha >> group_id >> curr_client;
        auto &temp = groups[group_id].file_owner[curr_client];
        auto it = find(temp.begin(), temp.end(), file_sha);
        if (it != temp.end())
        {
            auto &peers = groups[group_id].fileSha_to_peers[file_sha].second;
            for (auto peer_it = peers.begin(); peer_it != peers.end();)
            {
                if (peer_it->find(curr_client) != string::npos)
                    peer_it = peers.erase(peer_it);
                else
                    ++peer_it;
            }
            if (peers.empty())
                groups[group_id].fileSha_to_peers.erase(file_sha);
            temp.erase(it);
        }
    }
    else if (command == "logout" || command == "exit")
    {
        string user_id;
        iss >> user_id;
        users[user_id].second = false;
    }
}

bool flag = true; // at start program flag should be true
// bool syncComplete=false;
void handleTrackerSync(int syncSocket, const string &logFile, bool sendv)
{
    // bool send will be true when we just want to send one latest updated (nth) entry to other tracker
    // by assuming it is synched with me till n-1 entries
    //     while(true)
    // {
    if (flag == true)
    {

        cout << "marked flag as " << flag << endl;
        flag = false;
        cout << "marked flag as " << flag << endl;
        // Just came online, need full synchronization
        string message = "i just came online";
        send(syncSocket, message.c_str(), message.length(), 0);
        cout << "sent message i " << message << endl;
        char buffer[4096];
        int bytesReceived = recv(syncSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytesReceived > 0)
        {
            buffer[bytesReceived] = '\0';
            string response(buffer);
            std ::cout << "response recieved " << response << endl;
            if (response == "i just came online")
            {
                cout<<"we both just got in hence don't don anything"<<endl;
            }
            else
            {
                // Other tracker sent its log file directly
                string otherLog(buffer);

                // Process the received log file line by line
                istringstream logStream(otherLog);
                string entry;
                while (getline(logStream, entry))
                {
                    if (!entry.empty())
                    {
                        writeToLog(logFile, entry);
                        applyLogEntry(entry);
                    }
                }
            }
        }

        // After initial synchronization, set flag to false

        // syncComplete=true;
        // cout<<"marked as syncComplete as "<<syncComplete<<endl;
    }
    if (sendv == false)
    {
        // // Normal operation - continuous receiving mode
        while (true)
        {
            char buffer[4096];
            int bytesReceived = recv(syncSocket, buffer, sizeof(buffer) - 1, 0);
           
            if (bytesReceived <= 0)
            {
                // Other tracker has gone down
                cout << "Connection to peer tracker lost" << endl;
                break;
            }
            else
            {
                buffer[bytesReceived] = '\0';
                string message(buffer);
                cout<<"message recieved from other tracker is "<<message<<endl;
                if (message == "i just came online")
                {
                    // The other tracker just came online, send our entire log file
                    ifstream logStream(logFile);
                    string logContents((istreambuf_iterator<char>(logStream)), istreambuf_iterator<char>());
                    send(syncSocket, logContents.c_str(), logContents.length(), 0);
                }
                else
                {
                    // Received latest log entry from the other tracker
                    writeToLog(logFile, message);
                    applyLogEntry(message);
                }
            }
        }
    }
    else
    {
        // Send mode - send latest log entry to other tracker
        if (!logFile.empty())
        {
            // Read the last entry from the log file
            ifstream logStream(logFile);
            string lastEntry;
            string line;

            while (getline(logStream, line))
            {
                if (!line.empty())
                {
                    lastEntry = line; // Keep updating to get the last line
                }
            }

            if (!lastEntry.empty())
            {
                // Send the latest entry to the other tracker
                cout<<"sending last entry "<<lastEntry<<endl;
                send(syncSocket, lastEntry.c_str(), lastEntry.length(), 0);
            }
        }
    }
    // }
}

void handleClient(int clientSocket, string trackerName, int trackerNo)
{
    string clientIp, clientPort;
    bool login = false;
    string currClient;
    // int syncSocket = (trackerName == "master") ? SUBTRACKER : mastersocket;

    while (true)
    {

        cout << "inside handleclient " << endl;
        char buffer[1024] = {0};
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);

        if (bytesRead <= 0)
        {
            cout << "Connection closed from client" << endl;
            if (!currClient.empty())
            {
                users[currClient].second = false;
                string logEntry = "exit " + currClient + " " + clientIp + " " + clientPort;
                writeToLog(logFile, logEntry);

                // Sync with other tracker
                if (syncSocket >= 0)
                {
                    handleTrackerSync(syncSocket, logFile, true);
                }
            }
            close(clientSocket);
            return;
        }

        string request(buffer);
        istringstream iss(request);
        string command;
        iss >> command;
        cout << trackerName << " received command: " << command << endl;

        // Handle client exit
        if (command == "exit")
        {
            cout << "Connection closed from client" << endl;
            users[currClient].second = false;
            string logEntry = "exit " + currClient + " " + clientIp + " " + clientPort;
            writeToLog(logFile, logEntry);

            // Sync with other tracker
            if (syncSocket >= 0)
            {
                handleTrackerSync(syncSocket, logFile, true);
            }

            string response = "Exit processed by " + trackerName;
            send(clientSocket, response.c_str(), response.size(), 0);
            close(clientSocket);
            return;
        }

        // Handle user creation
        else if (command == "create_user")
        {
            string userId, password;
            iss >> userId >> password;

            if (users.find(userId) == users.end())
            {
                users[userId].first = password;
                users[userId].second = false;
                string logEntry = "User_created " + userId + " " + password;
                writeToLog(logFile, logEntry);

                // Sync with other tracker
                if (syncSocket >= 0)
                {
                    handleTrackerSync(syncSocket, logFile, true);
                }

                string response = "User created successfully by " + trackerName;
                send(clientSocket, response.c_str(), response.size(), 0);
            }
            else
            {
                string response = "User already exists";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }

        // Handle login
        else if (command == "login")
        {
            if (users.find(currClient) != users.end() && users[currClient].second == true)
            {
                string response = "User has already logged in here";
                send(clientSocket, response.c_str(), response.size(), 0);
                continue;
            }

            string userId, password;
            iss >> userId >> password >> clientIp >> clientPort;

            if (users.find(userId) != users.end() && users[userId].first == password)
            {
                if (users[userId].second)
                {
                    string response = "This user has already logged in at some other port";
                    send(clientSocket, response.c_str(), response.size(), 0);
                    continue;
                }

                users[userId].second = true;
                currClient = userId;
                string logEntry = "Login " + userId + " " + password + " " + clientIp + " " + clientPort;
                writeToLog(logFile, logEntry);

                // Sync with other tracker
                if (syncSocket >= 0)
                {
                    handleTrackerSync(syncSocket, logFile, true);
                }

                string response = "Login successful, processed by " + trackerName;
                send(clientSocket, response.c_str(), response.size(), 0);
            }
            else
            {
                string response = "Login failed. Check username and password.";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }

        // Require login for other operations
        else if (users.find(currClient) == users.end() || users[currClient].second == false)
        {
            string response = "Please login first";
            send(clientSocket, response.c_str(), response.size(), 0);
        }

        // Handle group creation
        else if (command == "create_group")
        {
            string groupId;
            iss >> groupId;

            if (groups.find(groupId) == groups.end())
            {
                string ownerName = currClient;
                groups[groupId] = {ownerName, {}, {}};
                groups[groupId].members.push_back(currClient);

                string logEntry = "create_group " + groupId + " " + ownerName;
                writeToLog(logFile, logEntry);

                // Sync with other tracker
                if (syncSocket >= 0)
                {
                    handleTrackerSync(syncSocket, logFile, true);
                }

                string response = "Group created successfully by " + trackerName;
                send(clientSocket, response.c_str(), response.size(), 0);
            }
            else
            {
                string response = "Group already exists";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }

        // Handle join group request
        else if (command == "join_group")
        {
            string groupId, userId = currClient;
            iss >> groupId;

            if (groups.find(groupId) != groups.end())
            {
                auto &members = groups[groupId].members;
                auto it = find(members.begin(), members.end(), userId);

                if (it != members.end())
                {
                    string response = "You are already a member of this group";
                    send(clientSocket, response.c_str(), response.size(), 0);
                    continue;
                }

                auto &pending = groups[groupId].pendingRequests;
                it = find(pending.begin(), pending.end(), userId);

                if (it != pending.end())
                {
                    string response = "You have already sent a request to join this group";
                    send(clientSocket, response.c_str(), response.size(), 0);
                    continue;
                }

                groups[groupId].pendingRequests.push_back(userId);
                string logEntry = "join_group " + userId + " " + groupId;
                writeToLog(logFile, logEntry);

                // Sync with other tracker
                if (syncSocket >= 0)
                {
                    handleTrackerSync(syncSocket, logFile, true);
                }

                string response = "Join request sent successfully, processed by " + trackerName;
                send(clientSocket, response.c_str(), response.size(), 0);
            }
            else
            {
                string response = "Group does not exist";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }

        // Handle leave group
        else if (command == "leave_group")
        {
            string groupId, userId = currClient;
            iss >> groupId;

            if (groups.find(groupId) != groups.end())
            {
                auto &members = groups[groupId].members;
                auto it = find(members.begin(), members.end(), userId);

                if (it != members.end())
                {
                    // Handle file ownership and peer information
                    vector<string> currentSHAs = groups[groupId].file_owner[currClient];
                    for (auto &sha : currentSHAs)
                    {
                        string peer = currClient + " " + clientIp + " " + clientPort;
                        vector<string> &peers = groups[groupId].fileSha_to_peers[sha].second;

                        auto peerIt = find(peers.begin(), peers.end(), peer);
                        if (peerIt != peers.end())
                        {
                            peers.erase(peerIt);
                        }

                        if (groups[groupId].fileSha_to_peers[sha].second.empty())
                        {
                            groups[groupId].fileSha_to_peers.erase(sha);
                        }
                    }

                    groups[groupId].file_owner.erase(currClient);

                    // Handle group deletion if last member
                    if (members.size() == 1)
                    {
                        groups.erase(groupId);
                        string logEntry = "Left_group " + userId + " " + groupId + " " + clientIp + " " + clientPort;
                        writeToLog(logFile, logEntry);

                        // Sync with other tracker
                        if (syncSocket >= 0)
                        {
                            handleTrackerSync(syncSocket, logFile, true);
                        }

                        string response = "You left the group and it was deleted (last member)";
                        send(clientSocket, response.c_str(), response.size(), 0);
                        continue;
                    }

                    // Handle ownership transfer if owner leaves
                    if (groups[groupId].owner == *it)
                    {
                        for (int i = 0; i < members.size(); i++)
                        {
                            if (members[i] != *it)
                            {
                                groups[groupId].owner = members[i];
                                break;
                            }
                        }
                    }

                    members.erase(it);
                    string logEntry = "Left_group " + userId + " " + groupId + " " + clientIp + " " + clientPort;
                    writeToLog(logFile, logEntry);

                    // Sync with other tracker
                    if (syncSocket >= 0)
                    {
                        handleTrackerSync(syncSocket, logFile, true);
                    }

                    string response = "You left the group successfully";
                    send(clientSocket, response.c_str(), response.size(), 0);
                }
                else
                {
                    string response = "You are not a member of this group";
                    send(clientSocket, response.c_str(), response.size(), 0);
                }
            }
            else
            {
                string response = "Group does not exist";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }

        // Handle list requests
        else if (command == "list_requests")
        {
            string groupId;
            iss >> groupId;

            if (groups.find(groupId) != groups.end())
            {
                if (groups[groupId].owner != currClient)
                {
                    string response = "You are not the owner of this group";
                    send(clientSocket, response.c_str(), response.size(), 0);
                    continue;
                }

                string response = "Pending requests: ";
                for (const auto &user : groups[groupId].pendingRequests)
                {
                    response += user + " ";
                }
                send(clientSocket, response.c_str(), response.size(), 0);
            }
            else
            {
                string response = "Group does not exist";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }

        // Handle accept request
        else if (command == "accept_request")
        {
            string groupId, userId;
            iss >> groupId >> userId;

            if (groups.find(groupId) != groups.end())
            {
                if (groups[groupId].owner != currClient)
                {
                    string response = "You are not the owner of this group";
                    send(clientSocket, response.c_str(), response.size(), 0);
                    continue;
                }

                auto &pending = groups[groupId].pendingRequests;
                auto it = find(pending.begin(), pending.end(), userId);

                if (it != pending.end())
                {
                    if (find(groups[groupId].members.begin(), groups[groupId].members.end(), userId) != groups[groupId].members.end())
                    {
                        string response = "User is already a member of this group";
                        send(clientSocket, response.c_str(), response.size(), 0);
                    }
                    else
                    {
                        groups[groupId].members.push_back(userId);
                        pending.erase(it);

                        string logEntry = "User_added " + userId + " " + groupId;
                        writeToLog(logFile, logEntry);

                        // Sync with other tracker
                        if (syncSocket >= 0)
                        {
                            handleTrackerSync(syncSocket, logFile, true);
                        }

                        string response = "User added to the group successfully";
                        send(clientSocket, response.c_str(), response.size(), 0);
                    }
                }
                else
                {
                    string response = "No such join request";
                    send(clientSocket, response.c_str(), response.size(), 0);
                }
            }
            else
            {
                string response = "Group does not exist";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }

        // Handle list groups
        else if (command == "list_groups")
        {
            string response = "Available groups: ";
            for (const auto &group : groups)
            {
                response += group.first + " ";
            }
            send(clientSocket, response.c_str(), response.size(), 0);
        }

        // Handle list members
        else if (command == "list_members")
        {
            string groupId;
            iss >> groupId;

            if (groups.find(groupId) != groups.end())
            {
                string response = "Members: ";
                for (const auto &member : groups[groupId].members)
                {
                    response += member + " ";
                }
                send(clientSocket, response.c_str(), response.size(), 0);
            }
            else
            {
                string response = "Group not found";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }

        // Handle logout
        else if (command == "logout")
        {
            users[currClient].second = false;
            string logEntry = "logout " + currClient;
            writeToLog(logFile, logEntry);

            // Sync with other tracker
            if (syncSocket >= 0)
            {
                handleTrackerSync(syncSocket, logFile, true);
            }

            string response = "Logout successful";
            send(clientSocket, response.c_str(), response.size(), 0);
            currClient = "";
        }

        // Handle list files
        else if (command == "list_files")
        {
            string groupId;
            iss >> groupId;

            if (groups.find(groupId) != groups.end())
            {
                if (find(groups[groupId].members.begin(), groups[groupId].members.end(), currClient) == groups[groupId].members.end())
                {
                    string response = "You are not a member of this group";
                    send(clientSocket, response.c_str(), response.size(), 0);
                    continue;
                }

                string response = "Sharable files and their hashes name and size:\n";
                set<pair<string, string>> st;

                for (const auto &entry : groups[groupId].file_owner)
                {
                    if (users[entry.first].second == false)
                        continue;

                    vector<string> fileHashes = entry.second;
                    for (auto &hash : fileHashes)
                    {
                        string filename = groups[groupId].fileSha_to_peers[hash].first;
                        st.insert({hash, filename});
                    }
                }

                for (auto &it : st)
                {
                    response += it.first + " " + it.second + "\n";
                }

                send(clientSocket, response.c_str(), response.size(), 0);
            }
            else
            {
                string response = "Group does not exist";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }

        // Handle upload file
        else if (command == "upload_file")
        {
            string groupId, fileName, fileHash, fileSize;
            iss >> groupId >> fileName >> fileSize >> fileHash;

            if (groups.find(groupId) != groups.end())
            {
                if (find(groups[groupId].members.begin(), groups[groupId].members.end(), currClient) == groups[groupId].members.end())
                {
                    string response = "You are not a member of this group";
                    send(clientSocket, response.c_str(), response.size(), 0);
                    continue;
                }

                string temp = currClient + " " + clientIp + " " + clientPort;

                // Check if file already uploaded
                if (groups[groupId].fileSha_to_peers.find(fileHash) != groups[groupId].fileSha_to_peers.end())
                {
                    vector<string> &v = groups[groupId].fileSha_to_peers[fileHash].second;

                    if (find(v.begin(), v.end(), temp) != v.end())
                    {
                        string response = "You have already uploaded or downloaded this file";
                        send(clientSocket, response.c_str(), response.size(), 0);
                        continue;
                    }

                    groups[groupId].fileSha_to_peers[fileHash].second.push_back(temp);
                    groups[groupId].file_owner[currClient].push_back(fileHash);

                    string logEntry = "upload_file 2nd " + fileHash + " " + currClient + " " + clientIp + " " + clientPort + " " + groupId + " " + fileName + " " + fileSize;
                    writeToLog(logFile, logEntry);

                    // Sync with other tracker
                    if (syncSocket >= 0)
                    {
                        handleTrackerSync(syncSocket, logFile, true);
                    }

                    string response = "File added to your shared files";
                    send(clientSocket, response.c_str(), response.size(), 0);
                    continue;
                }

                // New file upload
                groups[groupId].fileSha_to_peers[fileHash].first = fileName + " " + fileSize;
                groups[groupId].fileSha_to_peers[fileHash].second.push_back(temp);
                groups[groupId].file_owner[currClient].push_back(fileHash);

                string logEntry = "upload_file upload " + fileHash + " " + currClient + " " + clientIp + " " + clientPort + " " + groupId + " " + fileName + " " + fileSize;
                writeToLog(logFile, logEntry);

                // Sync with other tracker
                if (syncSocket >= 0)
                {
                    handleTrackerSync(syncSocket, logFile, true);
                }

                string response = "File uploaded successfully";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
            else
            {
                string response = "Group does not exist";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }

        // Handle download file
        else if (command == "download_file")
        {
            string groupId, fileSha;
            iss >> groupId >> fileSha;

            if (find(groups[groupId].members.begin(), groups[groupId].members.end(), currClient) == groups[groupId].members.end())
            {
                string response = "You are not a member of this group";
                send(clientSocket, response.c_str(), response.size(), 0);
                continue;
            }

            if (groups.find(groupId) == groups.end() || groups[groupId].fileSha_to_peers.find(fileSha) == groups[groupId].fileSha_to_peers.end())
            {
                string response = "File not found";
                send(clientSocket, response.c_str(), response.size(), 0);
                continue;
            }

            istringstream nameSize(groups[groupId].fileSha_to_peers[fileSha].first);
            string name, fileSize;
            nameSize >> name >> fileSize;

            string response = fileSize + " ";
            for (const string &temp : groups[groupId].fileSha_to_peers[fileSha].second)
            {
                istringstream s(temp);
                string tu, ti, tp;
                s >> tu >> ti >> tp;
                response += ti + " " + tp + " \n";
            }

            send(clientSocket, response.c_str(), response.size(), 0);
        }

        // Handle stop share
        else if (command == "stop_share")
        {
            string groupId, fileSha;
            iss >> groupId >> fileSha;

            if (find(groups[groupId].members.begin(), groups[groupId].members.end(), currClient) == groups[groupId].members.end())
            {
                string response = "You are not a member of this group";
                send(clientSocket, response.c_str(), response.size(), 0);
                continue;
            }

            if (groups.find(groupId) == groups.end() || groups[groupId].fileSha_to_peers.find(fileSha) == groups[groupId].fileSha_to_peers.end())
            {
                string response = "File not found";
                send(clientSocket, response.c_str(), response.size(), 0);
                continue;
            }

            vector<string> &temp = groups[groupId].file_owner[currClient];
            auto it = find(temp.begin(), temp.end(), fileSha);

            if (it == temp.end())
            {
                string response = "You don't have this file";
                send(clientSocket, response.c_str(), response.size(), 0);
                continue;
            }

            vector<string> &temp2 = groups[groupId].fileSha_to_peers[fileSha].second;
            string peerString = currClient + " " + clientIp + " " + clientPort;
            auto peerIt = find(temp2.begin(), temp2.end(), peerString);

            if (peerIt != temp2.end())
            {
                temp2.erase(peerIt);
            }

            if (temp2.empty())
            {
                groups[groupId].fileSha_to_peers.erase(fileSha);
            }

            temp.erase(it);

            string logEntry = "stop_share " + fileSha + " " + groupId + " " + currClient;
            writeToLog(logFile, logEntry);

            // Sync with other tracker
            if (syncSocket >= 0)
            {
                handleTrackerSync(syncSocket, logFile, true);
            }

            string response = "Stopped sharing file";
            send(clientSocket, response.c_str(), response.size(), 0);
        }
        else
        {
            string response = "Unknown command";
            send(clientSocket, response.c_str(), response.size(), 0);
        }
    }
}