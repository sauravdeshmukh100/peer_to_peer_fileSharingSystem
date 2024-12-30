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

// #include <openssl/evp.h> // For EVP functions
#include <openssl/sha.h>
#include <fcntl.h> // For open()
                   // For read(), close()

#include <algorithm> // Include for  find
using namespace std;

int mastersocket = -1;
int SUBTRACKER = -1;

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

std::mutex fileMutex; // To ensure thread safety in case of concurrent access

bool copyFileContent(const std::string &source, const std::string &destination)
{
    std::lock_guard<std::mutex> lock(fileMutex);

    // Open the source file
    std::ifstream inputFile(source);
    if (!inputFile.is_open())
    {
        std::cerr << "Error: Unable to open source file " << source << std::endl;
        return false;
    }

    // Read the content of the source file
    std::string content((std::istreambuf_iterator<char>(inputFile)), std::istreambuf_iterator<char>());
    inputFile.close();

    // Open or create the destination file
    std::ofstream outputFile(destination, std::ios::app); // Open in append mode
    if (!outputFile.is_open())
    {
        std::cerr << "Error: Unable to create or open destination file " << destination << std::endl;
        return false;
    }

    // Write content to the destination file
    outputFile << content;
    outputFile.close();

    std::cout << "Content from " << source << " written to " << destination << std::endl;
    return true;
}

// Function to write to the log file
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

// Function to read the log file
std::vector<std::string> readLog(const std::string &logFile)
{
    std::lock_guard<std::mutex> lock(logMutex);
    std::ifstream log(logFile);
    std::vector<std::string> entries;
    std::string line;
    while (std::getline(log, line))
    {
        istringstream iss(line);
        string command;

        iss >> command;

        if (command == "user_created")
        {
            string user_id, password;
            iss >> user_id >> password;

            users[user_id].first = password;
            users[user_id].second = false;
        }

        else if (command == "login")
        {
            string user_id, password;
            iss >> user_id >> password;
            users[user_id].second = true;
            // curr_client = usre_id;
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
        else if (command == "Left_group")
        {
            string user_id, group_id, client_ip, client_port;
            iss >> user_id >> group_id >> client_ip >> client_port;

            auto &members = groups[group_id].members;
            auto it = find(members.begin(), members.end(), user_id);

            // storing all the shas of files which curr client has
            vector<string> currentSHAs = groups[group_id].file_owner[user_id];
            // iterating through each sha which current client has
            for (auto &sha : currentSHAs)
            {
                // deleting the info of this client with particular sha

                string peer = user_id + " " + client_ip + " " + client_port;
                vector<string> &peers = groups[group_id].fileSha_to_peers[sha].second;

                auto it = find(peers.begin(), peers.end(), peer);
                // int peer_index = it - peers.begin();
                peers.erase(it);

                // deleting the entry of this file if this is only own by current user
                if (groups[group_id].fileSha_to_peers[sha].second.size() == 0)
                {
                    groups[group_id].fileSha_to_peers.erase(sha);
                }
            }

            // deleting current user from file map
            groups[group_id].file_owner.erase(user_id);
            string new_owner = "null";
            if (members.size() == 1)
            {
                std::cout << "deleting group " << group_id << endl;

                groups.erase(group_id);

                continue;
            }

            if (groups[group_id].owner == *it)
            {
                for (int i = 0; i < members.size(); i++)
                {
                    if (members[i] != *it)
                    {
                        groups[group_id].owner = members[i];
                        new_owner = members[i];
                        std::cout << "groupid " << group_id << " having new owner " << members[i] << endl;
                    }
                }
            }
            members.erase(it);
        }

        else if (command == "User_added")
        {
            string user_id, group_id;
            iss >> user_id >> group_id;
            auto &pending = groups[group_id].pendingRequests;
            auto it = find(pending.begin(), pending.end(), user_id);
            groups[group_id].members.push_back(user_id);
            pending.erase(it);
        }
        else if (command == "logout" or command=="exit")
        {
            string user_id;
            iss >> user_id;
            users[user_id].second = false;
        }
        else if (command == "upload_file")
        {
            string upload_no, fileHash, curr_client, client_ip, client_port, group_id, fileName, filesize;
            iss >> command >> fileHash >> curr_client >> client_ip >> client_port >> group_id >> fileName >> filesize;
            string temp = curr_client + " " + client_ip + " " + client_port;
            if (upload_no == "upload")
            {
                groups[group_id].fileSha_to_peers[fileHash].first = fileName + " " + filesize;
                groups[group_id].fileSha_to_peers[fileHash].second.push_back(temp);

                // groups[group_id].file_owner[curr_client].push_back(fileName);
                groups[group_id].file_owner[curr_client].push_back(fileHash);
            }
            else
            {
                groups[group_id].fileSha_to_peers[fileHash].second.push_back(temp);
                groups[group_id].file_owner[curr_client].push_back(fileHash);
            }
        }

        else if (command == "stop_share")
        {
            string file_sha, group_id,curr_client;
            iss >> file_sha >> group_id>>curr_client;

            vector<string> &temp = groups[group_id].file_owner[curr_client];
            auto it = find(temp.begin(), temp.end(), file_sha);
            // delete this file sha if this is only own by current user
            vector<string> &temp2 = groups[group_id].fileSha_to_peers[file_sha].second;

            if (temp2.size() == 1)
            {
                cout << "size is 1" << endl;
                string temp3 = temp2[0];
                size_t spacePos = temp3.find(curr_client);

                if (spacePos != string::npos)
                {
                    cout << "erasing file sha" << endl;
                    groups[group_id].fileSha_to_peers.erase(file_sha);
                }
            }

            temp.erase(it);
        }
        entries.push_back(line);
        
    }
    return entries;
}

// Function to send log file content
void sendLogFile(int socket, const std::string &logFile)
{
    auto entries = readLog(logFile);
    std::string data;
    for (const auto &entry : entries)
    {
        data += entry + "\n";
    }
    send(socket, data.c_str(), data.size(), 0);
}

// Function to receive log file content
void receiveLogFile(int socket, const std::string &logFile)
{
    char buffer[4096];
    int bytesRead = recv(socket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead > 0)
    {
        buffer[bytesRead] = '\0';
        std::ofstream log(logFile, std::ios::app);
        log << buffer;
        log.close();
    }
}

void handleClient(int clientSocket, string tracker_name)
{

    string client_ip, client_port;

    bool login = false;
    bool sub_tracker = false;
    string curr_client;

    if (tracker_name == "master")
    {

        cout << "master is trying to taking content from sub" << endl;
        // write conetent of file log1.txt to log0.txt
        const std::string file1 = "log1.txt";
        const std::string file2 = "log0.txt";

        // Copy content from log1.txt to log0.txt
        if (copyFileContent(file1, file2))
            std::cout << "File synchronization completed successfully!" << std::endl;
    }

    while (true)
    {
        char buffer[1024] = {0};
        int b = recv(clientSocket, buffer, sizeof(buffer), 0);
        // cout<<"b is"<<b<<endl;

        if (b <= 0)
        {
            cout << "didn't recieve anything from client " << endl;
            users[curr_client].second = false;
            string response = "exit " + curr_client + " " + client_ip + " " + client_port;
            curr_client = "";
            cout << "logging out from user " << curr_client << "\n";
            
            if (tracker_name == "master")
                send(SUBTRACKER, response.c_str(), response.size(), 0);
            else
                send(mastersocket, response.c_str(), response.size(), 0);
             writeToLog(logFile, response);    

            close(clientSocket);
            // writeToLog(logFile, "Handled request: " + request);

            // check if this is subtracker and masterclient has +ve value then send data to mastertracker
            return;
        }

        std::string request(buffer);
        std::istringstream iss(request);
        std::string command;

        iss >> command;
        cout << "command is " << command << endl;
        // cout << "command is " << command << endl;

        if (command == "I_am_online")
        {
            if (tracker_name == "master")
                SUBTRACKER = clientSocket;
        }

        else if (command == "sub-tracker")
        {
            // Extract the remaining content of the stream
            std::string remainingData;
            std::getline(iss, remainingData);
            cout << "writing request in mastertracker file=" << remainingData << endl;
            writeToLog(logFile, remainingData.substr(1)); // Assuming logFile is defined and passed to the function
        }
        else if (command == "exit")
        {

            cout << "connection closed from client\n";
            users[curr_client].second = false;
            string response = "exit " + curr_client + " " + client_ip + " " + client_port;

            curr_client = "";
            cout << "logging out from user " << curr_client << "\n";
            close(clientSocket);
            writeToLog(logFile, response);

            if (tracker_name == "master")
                send(SUBTRACKER, response.c_str(), response.size(), 0);
            else
                send(mastersocket, response.c_str(), response.size(), 0);
            return;
        }
        else if (command == "create_user")
        {

            // this is need not be curr user
            string user_id, password;
            iss >> user_id >> password;
            if (users.find(user_id) == users.end())
            {
                users[user_id].first = password;
                users[user_id].second = false;
                string response = "User_created " + user_id + " " + password;
                writeToLog(logFile, response);
                send(clientSocket, response.c_str(), response.size(), 0);

                if (tracker_name == "master")
                {
                    cout << "sending response to subtracker" << endl;
                    send(SUBTRACKER, response.c_str(), response.size(), 0);
                }
                else
                {
                    string tracker_reponse = "sub-tracker " + response;
                    cout << "sending response to master=" << tracker_reponse << endl;

                    send(mastersocket, tracker_reponse.c_str(), tracker_reponse.size(), 0);
                }

                //  cout<<"hii"<<endl;
            }
            else
            {
                string response = "User already exists.";
                send(clientSocket, response.c_str(), response.size(), 0);
                // cout<<"hii"<<endl;
            }
        }
        else if (command == "login")
        {
            // login = true;

            if (users.find(curr_client) != users.end() and users[curr_client].second == true)
            {
                string response = "user has already logged in here";
                send(clientSocket, response.c_str(), response.size(), 0);
                continue;
            }
            string user_id, password;
            iss >> user_id >> password >> client_ip >> client_port;

            if (users.find(user_id) != users.end() && users[user_id].first == password)
            {
                if (users[user_id].second)
                {
                    string response = "this user has already logged in at some other port";
                    send(clientSocket, response.c_str(), response.size(), 0);
                    continue;
                }
                users[user_id].second = true;
                curr_client = user_id;
                string response = "Login succesfull";
                string request = "Login " + user_id + " " + password + " " + client_ip + " " + client_port;
                writeToLog(logFile, request);

                send(clientSocket, response.c_str(), response.size(), 0);

                if (tracker_name == "master")
                    send(SUBTRACKER, request.c_str(), response.size(), 0);
                else
                    send(mastersocket, request.c_str(), response.size(), 0);
            }
            else
            {
                string response = "Login failed. Check username and password.";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }
        else if (users.find(curr_client) == users.end() or users[curr_client].second == false)
        {
            string response = "login first.";
            send(clientSocket, response.c_str(), response.size(), 0);
        }
        else if (command == "create_group")
        {

            // cout<<"i am in create grp\n";
            string group_id;
            iss >> group_id;
            if (groups.find(group_id) == groups.end())
            {

                string owner_name = curr_client;

                cout << "owner is " << owner_name << endl;
                groups[group_id] = {owner_name, {}, {}};
                groups[group_id].members.push_back(curr_client);
                string response = "Group created " + group_id + " " + owner_name;
                send(clientSocket, response.c_str(), response.size(), 0);
                writeToLog(logFile, response);
                if (tracker_name == "master")
                    send(SUBTRACKER, response.c_str(), response.size(), 0);
                else
                    send(mastersocket, response.c_str(), response.size(), 0);
            }
            else
            {
                string response = "Group already exists.";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }
        else if (command == "join_group")
        {
            string group_id, user_id = curr_client;
            iss >> group_id;
            if (groups.find(group_id) != groups.end())
            {

                auto &members = groups[group_id].members;
                auto it = find(members.begin(), members.end(), user_id);
                if (it != members.end())
                {
                    string response = "U are already member of group";
                    send(clientSocket, response.c_str(), response.size(), 0);
                    continue;
                }

                auto &pending = groups[group_id].pendingRequests;
                it = find(pending.begin(), pending.end(), user_id);
                if (it != pending.end())
                {
                    string response = "U have already sent request for joining group";
                    send(clientSocket, response.c_str(), response.size(), 0);
                    continue;
                }

                groups[group_id].pendingRequests.push_back(user_id);
                string response = "Join_request " + user_id + " " + group_id;
                
                send(clientSocket, response.c_str(), response.size(), 0);

                // send to tracker ;
                if (tracker_name == "master")
                    send(SUBTRACKER, response.c_str(), response.size(), 0);
                else
                    send(mastersocket, response.c_str(), response.size(), 0);

                writeToLog(logFile, response);    
            }
            else
            {
                string response = "Group does not exist.";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }
        else if (command == "leave_group")
        {
            string group_id, user_id = curr_client;
            iss >> group_id;
            if (groups.find(group_id) != groups.end())
            {
                auto &members = groups[group_id].members;
                auto it = find(members.begin(), members.end(), user_id);
                if (it != members.end())
                {
                    // storing all the shas of files which curr client has
                    vector<string> currentSHAs = groups[group_id].file_owner[curr_client];
                    // iterating through each sha which current client has
                    for (auto &sha : currentSHAs)
                    {
                        // deleting the info of this client with particular sha

                        string peer = curr_client + " " + client_ip + " " + client_port;
                        vector<string> &peers = groups[group_id].fileSha_to_peers[sha].second;

                        auto it = find(peers.begin(), peers.end(), peer);
                        // int peer_index = it - peers.begin();
                        peers.erase(it);

                        // deleting the entry of this file if this is only own by current user
                        if (groups[group_id].fileSha_to_peers[sha].second.size() == 0)
                        {
                            groups[group_id].fileSha_to_peers.erase(sha);
                        }
                    }

                    // deleting current user from file map
                    groups[group_id].file_owner.erase(curr_client);
                    // string new_owner = "null";
                    if (members.size() == 1)
                    {
                        cout << "deleting group " << group_id << endl;

                        groups.erase(group_id);
                        string response = "Left_group " + user_id + " " + group_id + " " + client_ip + " " + client_port;

                    if (tracker_name == "master")
                        send(SUBTRACKER, response.c_str(), response.size(), 0);
                    else
                        send(mastersocket, response.c_str(), response.size(), 0);
                    
                    cout<<"writing to log file response="<<response<<endl;
                    writeToLog(logFile, response); 
                     send(clientSocket, response.c_str(), response.size(), 0);

                        continue;
                    }

                    if (groups[group_id].owner == *it)
                    {
                        for (int i = 0; i < members.size(); i++)
                        {
                            if (members[i] != *it)
                            {
                                groups[group_id].owner = members[i];
                               
                                cout << "groupid " << group_id << " having new owner " << members[i] << endl;
                            }
                        }
                    }
                    members.erase(it);
                    string response = "Left group successfully.";
                    send(clientSocket, response.c_str(), response.size(), 0);

                    response = "Left_group " + user_id + " " + group_id + " " + client_ip + " " + client_port;

                    if (tracker_name == "master")
                        send(SUBTRACKER, response.c_str(), response.size(), 0);
                    else
                        send(mastersocket, response.c_str(), response.size(), 0);

                    writeToLog(logFile, response);    
                }
                else
                {
                    string response = "You are not a member of this group.";
                    send(clientSocket, response.c_str(), response.size(), 0);
                }
            }
            else
            {
                string response = "Group does not exist.";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }
        else if (command == "list_requests")
        {
            string group_id;
            iss >> group_id;
            if (groups.find(group_id) != groups.end())
            {
                string response = "Pending requests: ";
                for (const auto &user : groups[group_id].pendingRequests)
                {
                    response += user + " ";
                }
                send(clientSocket, response.c_str(), response.size(), 0);
            }
            else
            {
                string response = "Group does not exist.";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }
        else if (command == "accept_request")
        {
            string group_id, user_id;
            iss >> group_id >> user_id;
            if (groups.find(group_id) != groups.end())
            {
                if (groups[group_id].owner != curr_client)
                {
                    string response = "You are not the owner of this group";
                    send(clientSocket, response.c_str(), response.size(), 0);
                    continue;
                }
                auto &pending = groups[group_id].pendingRequests;
                auto it = find(pending.begin(), pending.end(), user_id);
                if (it != pending.end())
                {
                    if (find(groups[group_id].members.begin(), groups[group_id].members.end(), user_id) != groups[group_id].members.end())
                    {
                        string response = "User already exist.";
                        send(clientSocket, response.c_str(), response.size(), 0);
                    }
                    else
                    {
                        groups[group_id].members.push_back(user_id);
                        pending.erase(it);
                        string response = "User_added " + user_id + " " + group_id;
                        send(clientSocket, response.c_str(), response.size(), 0);

                        // send(clientSocket, response.c_str(), response.size(), 0);
                        if (tracker_name == "master")
                            send(SUBTRACKER, response.c_str(), response.size(), 0);
                        else
                            send(mastersocket, response.c_str(), response.size(), 0);
                        writeToLog(logFile, response);    
                    }
                }
                else
                {
                    string response = "No such join request.";
                    send(clientSocket, response.c_str(), response.size(), 0);
                }
            }
            else
            {
                string response = "Group does not exist.";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }
        else if (command == "list_groups")
        {
            string response = "Available groups: ";
            for (const auto &group : groups)
            {
                response += group.first + " ";
            }
            send(clientSocket, response.c_str(), response.size(), 0);
        }

        else if (command == "list_members")
        {
            string group_id;
            iss >> group_id;

            if (groups.find(group_id) != groups.end())
            {
                // Assuming group structure has a member list
                vector<string> members = groups[group_id].members;
                string response = "Members: ";
                for (const auto &member : members)
                {
                    response += member + " ";
                }
                send(clientSocket, response.c_str(), response.length(), 0);
            }
            else
            {
                string response = "Group not found.";
                send(clientSocket, response.c_str(), response.length(), 0);
            }
        }
        else if (command == "logout")
        {
            users[curr_client].second = false;
            string response = "logout " + curr_client;

            curr_client = "";
            // cout << " curr client size is " << curr_client.size() << endl;
            
            send(clientSocket, response.c_str(), response.length(), 0);

            // send(clientSocket, response.c_str(), response.size(), 0);
            if (tracker_name == "master")
                send(SUBTRACKER, response.c_str(), response.size(), 0);
            else
                send(mastersocket, response.c_str(), response.size(), 0);
            writeToLog(logFile, response);    
        }

        else if (command == "list_files")
        {
            string group_id;
            iss >> group_id;

            if (groups.find(group_id) != groups.end())
            {

                if (find(groups[group_id].members.begin(), groups[group_id].members.end(), curr_client) == groups[group_id].members.end())
                {
                    string response = "You are not member of this group\n";
                    send(clientSocket, response.c_str(), response.size(), 0);
                    continue;
                }
                string response = "Sharable files and their hashes name and size:\n";
                set<pair<string, string>> st;
                for (const auto &entry : groups[group_id].file_owner)
                {
                    if (users[entry.first].second == false)
                        continue;

                    vector<string> filehashes = entry.second; // Get the filename
                    // const auto & chunkHashes = entry.second; // Get the pair (file hash, chunk hashes)

                    // response +=  ;

                    for (auto &hash : filehashes)
                    {
                        string filename = groups[group_id].fileSha_to_peers[hash].first;
                        st.insert({hash, filename}); // used map to remove duplicates
                    }
                    // iterate through set

                    // response += "Chunk Hashes:\n";
                    // for (const auto &chunkHash : chunkHashes)
                    // {
                    //     response += chunkHash + "\n"; // Improved formatting
                    // }
                }

                for (auto &it : st)
                {
                    // cout<<"size of set is"<<st.size()<<endl;
                    response += it.first + " " + it.second + "\n";
                }
                send(clientSocket, response.c_str(), response.size(), 0);
            }
            else
            {
                string response = "Group does not exist.";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }

        else if (command == "upload_file")
        {
            // cout << "command is upload file" << endl;
            string group_id;
            string fileName, fileHash;
            vector<string> chunkHashes;
            string filesize;
            // Step 1: Extract group_id, fileName, and fileHash from the message
            iss >> group_id >> fileName >> filesize >> fileHash;
            cout << "group_id is " << group_id << " filename is " << fileName << "filesize is" << filesize << endl;
            // Step 2: Extract chunk hashes (rest of the message)

            // Step 3: Check if the group exists
            if (groups.find(group_id) != groups.end())
            {

                if (find(groups[group_id].members.begin(), groups[group_id].members.end(), curr_client) == groups[group_id].members.end())
                {
                    // Client is not a member of the group
                    string response = "You are not a member of this group.";
                    send(clientSocket, response.c_str(), response.size(), 0);
                    continue;
                }
                //
                string temp = curr_client + " " + client_ip + " " + client_port;
                // check if this file is already uploaded
                if (groups[group_id].fileSha_to_peers.find(fileHash) != groups[group_id].fileSha_to_peers.end())
                {

                    vector<string> &v = groups[group_id].fileSha_to_peers[fileHash].second;
                    if (find(v.begin(), v.end(), temp) != v.end())
                    {
                        string response = "u have already uploded or dowmloaded this file";
                        send(clientSocket, response.c_str(), response.size(), 0);

                        continue;
                    }
                    // string response = "This file  is  uploaded by u also";
                    groups[group_id].fileSha_to_peers[fileHash].second.push_back(temp);
                    groups[group_id].file_owner[curr_client].push_back(fileHash);

                    string response = "2nd upload " + fileHash + " " + temp + " " + group_id + " abc.txt " + "0";

                    // send(clientSocket, response.c_str(), response.size(), 0);
                    if (tracker_name == "master")
                        send(SUBTRACKER, response.c_str(), response.size(), 0);
                    else
                        send(mastersocket, response.c_str(), response.size(), 0);
                    writeToLog(logFile, response);    

                    // send(clientSocket, response.c_str(), response.size(), 0);
                    continue;
                }

                groups[group_id].fileSha_to_peers[fileHash].first = fileName + " " + filesize;
                groups[group_id].fileSha_to_peers[fileHash].second.push_back(temp);

                // groups[group_id].file_owner[curr_client].push_back(fileName);
                groups[group_id].file_owner[curr_client].push_back(fileHash);
                // Send success response to client
                string response = "File uploaded  successfully.";
                send(clientSocket, response.c_str(), response.size(), 0);

                //

                response = "upload " + fileHash + " " + temp + " " + group_id + " " + fileName + " " + filesize;

                // send(clientSocket, response.c_str(), response.size(), 0);
                if (tracker_name == "master")
                    send(SUBTRACKER, response.c_str(), response.size(), 0);
                else
                    send(mastersocket, response.c_str(), response.size(), 0);

                writeToLog(logFile, response);    
            }
            else
            {
                // If group doesn't exist, send error response
                string response = "Group does not exist.";
                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }

        else if (command == "download_file")
        {
            string group_id, file_sha;
            iss >> group_id >> file_sha;

            // check wheather he is member of grpup
            if (find(groups[group_id].members.begin(), groups[group_id].members.end(), curr_client) == groups[group_id].members.end())
            {
                // Client is not a member of the group
                string response = "You are not a member of this group.";
                send(clientSocket, response.c_str(), response.size(), 0);
                continue;
            }

            // Validate group and file existence
            if (groups.find(group_id) == groups.end() ||
                groups[group_id].fileSha_to_peers.find(file_sha) == groups[group_id].fileSha_to_peers.end())
            {
                string response = "File not found";
                send(clientSocket, response.c_str(), response.size(), 0);
                return;
            }

            // check whearher has he already uploaded or downloaded this file

            // if(groups[])
            istringstream name_size(groups[group_id].fileSha_to_peers[file_sha].first);
            string name, filesize;
            name_size >> name >> filesize;
            // Collect peers sharing the file
            string response = filesize + " ";
            //  sending reponse as  ip an ]d port of each peer having content of this file
            for (const string &temp : groups[group_id].fileSha_to_peers[file_sha].second)
            {
                istringstream s(temp);
                string tu, ti, tp;
                s >> tu >> ti >> tp;

                response += ti + " " + tp + " \n"; // storing only ip and port not userid
            }
            // cout<<"response is  \n"<<response<<endl;
            // Send peer information to client
            send(clientSocket, response.c_str(), response.size(), 0);
        }

        else if (command == "stop_share")
        {
            string group_id, file_sha;
            iss >> group_id >> file_sha;

            // check wheather he is member of grpup
            if (find(groups[group_id].members.begin(), groups[group_id].members.end(), curr_client) == groups[group_id].members.end())
            {
                // Client is not a member of the group
                string response = "You are not a member of this group.";
                send(clientSocket, response.c_str(), response.size(), 0);
                continue;
            }

            // Validate group and file existence
            if (groups.find(group_id) == groups.end() ||
                groups[group_id].fileSha_to_peers.find(file_sha) == groups[group_id].fileSha_to_peers.end())
            {
                string response = "File not found";
                send(clientSocket, response.c_str(), response.size(), 0);
                continue;
            }

            // checking if current client can share this file or not
            vector<string> &temp = groups[group_id].file_owner[curr_client];
            auto it = find(temp.begin(), temp.end(), file_sha);
            if (it == temp.end())
            {
                string response = "u dont have this file";
                send(clientSocket, response.c_str(), response.size(), 0);
                continue;
            }

            // delete this file sha if this is only own by current user
            vector<string> &temp2 = groups[group_id].fileSha_to_peers[file_sha].second;

            if (temp2.size() == 1)
            {
                cout << "size is 1" << endl;
                string temp3 = temp2[0];
                size_t spacePos = temp3.find(curr_client);

                if (spacePos != string::npos)
                {
                    cout << "erasing file sha" << endl;
                    groups[group_id].fileSha_to_peers.erase(file_sha);
                }
            }

            temp.erase(it);
            string response = "stopped sharing file";
            send(clientSocket, response.c_str(), response.size(), 0);

            response = "stop_share " + file_sha + " " + group_id + " " + curr_client;

            // send(clientSocket, response.c_str(), response.size(), 0);
            if (tracker_name == "master")
                send(SUBTRACKER, response.c_str(), response.size(), 0);
            else
                send(mastersocket, response.c_str(), response.size(), 0);
            writeToLog(logFile, response);    
        }
    }
}

void handlemaster(int &mastersocket)
{

    // write conetent of file log0.txt to log1.txt
    const std::string file1 = "log0.txt";
    const std::string file2 = "log1.txt";

    // Copy content from log0.txt to log1.txt
    copyFileContent(file1, file2);

    std::cout << "File synchronization completed successfully!" << std::endl;

    string response = "I_am_online";
    send(mastersocket, response.c_str(), response.size(), 0);

    while (true)
    {
        char buffer[1024] = {0};
        int b = recv(mastersocket, buffer, sizeof(buffer), 0);

        if (b <= 0)
        {
            cout << "didn't recieve anything from master " << endl;

            close(mastersocket);
            // writeToLog(logFile, "Handled request: " + request);

            // check if this is subtracker and masterclient has +ve value then send data to mastertracker
            return;
        }

        string request(buffer);
        cout << "writing request in subtracker file=" << request << endl;

        writeToLog(logFile, request);
    }
}