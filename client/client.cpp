#include <iostream>
#include <vector>
#include <sstream>
#include <string>
#include <openssl/sha.h>
#include <thread>
#include "read_tracker.h"
#include "client_functions.h"
using namespace std;


// Mock storage for chunks
// map<string, string> chunkStorage; // chunk_sha -> file path
// const size_t CHUNK_SIZE = 2;

void sendChunk(int clientSocket, const int& chunk_no, const string & filesha)
{
    // Check if the file sha exists
    if (clientFileMetadata.find(filesha) == clientFileMetadata.end())
    {
        string errorMessage = "file not found";
        send(clientSocket, errorMessage.c_str(), errorMessage.size(), 0);
        return;
    }

    // Open the file containing the chunk
    string filePath = clientFileMetadata[filesha].first;
    // Open the file in binary mode
    ifstream chunkFile(filePath, ios::binary);
    if (!chunkFile.is_open())
    {
        printMessage("Error: Could not open file " + filePath);
        string errorMessage = "Error: Could not open file";
        send(clientSocket, errorMessage.c_str(), errorMessage.size(), 0);
        return;
    }

    // Calculate the file offset for the specific chunk
    size_t offset = chunk_no * CHUNK_SIZE;

    // Move the file read pointer to the offset
    chunkFile.seekg(offset, ios::beg);
    if (!chunkFile)
    {
        printMessage("Error: Failed to seek to chunk " + to_string(chunk_no));
        string errorMessage = "Error: Invalid chunk number";
        send(clientSocket, errorMessage.c_str(), errorMessage.size(), 0);
        chunkFile.close();
        return;
    }

    // Read the specific chunk data
    char buffer[CHUNK_SIZE];
    chunkFile.read(buffer, sizeof(buffer));
    streamsize bytesRead = chunkFile.gcount();
    chunkFile.close();

    // Send the chunk data to the client
    if (bytesRead > 0)
    {
        string temp (buffer, bytesRead);
        
        // cout<<"for chunkno "<<chunk_no<<" sending data "<<temp.substr(0,10)<<endl;
        string response = clientFileMetadata[filesha].second[chunk_no] + " " + temp;
        // cout<<"sending data of size"<<response.size()<<endl;

        // printMessage("Sending chunk " + to_string(chunk_no) );
        send(clientSocket, response.c_str(), response.size(), 0);
    }
    else
    {
        printMessage("Error: No data read for chunk " + to_string(chunk_no));
        string errorMessage = "Error: No data for chunk";
        send(clientSocket, errorMessage.c_str(), errorMessage.size(), 0);
    }
}

void listenForChunkRequests(int listenPort)
{
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(listenPort);

    bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, 5);

    while (true)
    {
        int clientSocket = accept(serverSocket, nullptr, nullptr);

        char buffer[1024] = {0};
        recv(clientSocket, buffer, sizeof(buffer), 0);

        string request(buffer);
        // cout<<"request recieved="<<request<<endl;
        if(request.rfind("chunk_info", 0) == 0)
        {
            // printMessage("Request received: " + request.substr(11));

            auto it = clientFileMetadata.begin();
            // printMessage("First entry key: " + it->first);

            string response;

            if(clientFileMetadata.find(request.substr(11)) == clientFileMetadata.end())
            {
                printMessage("Entry not found for given hash");
                response = "entry not found for this file";
                send(clientSocket, response.c_str(), response.size(), 0);
                close(clientSocket);
                continue;
            }
            
            vector<string> chunk_hashes = clientFileMetadata[request.substr(11)].second;
              
            if(chunk_hashes.size() == 0)
            {
                printMessage("No chunks found for the given entry");
                response = "no chunk found for this entry";
                send(clientSocket, response.c_str(), response.size(), 0);
                close(clientSocket);
                continue;
            }
            
            int count = 0;
            for (auto & i : chunk_hashes)
            {
                if (i.size() == 0) { count++; continue; }
                response += to_string(count++) + " ";
            }
            // printMessage("Sending response: " + response);
            send(clientSocket, response.c_str(), response.size(), 0);
        }
        else if (request.rfind("download_chunk", 0) == 0)
        {
            // cout<<"inside doenload "<<endl;
            istringstream iss(request);
            string command, filesha, schunk_no;
            iss >> command >> filesha >> schunk_no;
            int chunk_no = stoi(schunk_no);

            // printMessage("Command: " + command + ", File SHA: " + filesha + ", Chunk No: " + to_string(chunk_no));
            // printMessage("Sending chunk request: " + to_string(chunk_no));
            sendChunk(clientSocket, chunk_no, filesha);
        }

        close(clientSocket);
    }
}





int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printMessage("Usage: ./client <IP>:<PORT> tracker_info.txt");
        return 1;
    }

    // Step 1: Parse the <IP>:<PORT> argument
    std::string ip_port = argv[1];
    size_t colon_pos = ip_port.find(':');
    if (colon_pos == std::string::npos)
    {
        printMessage("Invalid IP:PORT format. Expected <IP>:<PORT>.");
        return 1;
    }

    std::string ip = ip_port.substr(0, colon_pos);
    int port;
    try
    {
        port = std::stoi(ip_port.substr(colon_pos + 1));
    }
    catch (const std::invalid_argument &e)
    {
        printMessage("Invalid port number.");
        return 1;
    }

    std::string trackerInfoFile = argv[2];

    // Step 2: Read the tracker info
    std::vector<TrackerInfo> trackers = readTrackerInfo(trackerInfoFile);
    if (trackers.empty())
    {
        printMessage("No trackers found in tracker_info.txt");
        return 1;
    }

    // Step 3: Connect to the first tracker
    int clientSocket = connectToTracker(trackers[1].ip, trackers[1].port);
    if (clientSocket < 0)
    {
        printMessage("Cannot connect to tracker");
        return 1;
    }

    std::thread listener(listenForChunkRequests, port);
    listener.detach();

    // Handle commands
    std::string command;
    while (true)
    {
        printMessage("\nMenu:\n"
                     "1. Create User\n"
                     "2. Login\n"
                     "3. Create Group\n"
                     "4. Join Group\n"
                     "5. Leave Group\n"
                     "6. List Pending Join Requests\n"
                     "7. Accept Join Request\n"
                     "8. List All Groups\n"
                     "9. List All Members of a Group\n"
                     "10. Logout\n"
                     "11. List All Sharable Files in Group\n"
                     "12. Upload File to Group\n"
                     "13. Download File from Group\n"
                     "14. Stop Share\n"
                     "15. Show downloads\n"
                     "0. Exit\n");

        printMessage("Enter your choice: ");
        readInput(command);

        int choice;
        try
        {
            choice = std::stoi(command);
        }
        catch (std::invalid_argument &e)
        {
            printMessage("Invalid input. Please enter a valid number.");
            continue;
        }

        switch (choice)
        {
        case 1: // Create User
        {
            std::string user_id, passwd;
            printMessage("Enter user ID and password: ");
            readInput(command);
            std::istringstream iss(command);
            iss >> user_id >> passwd;
            if (user_id.empty() || passwd.empty())
            {
                printMessage("Invalid input. Please provide both user ID and password.");
                break;
            }
            createUserAccount(clientSocket, user_id, passwd);
            break;
        }

        case 2: // Login
        {
            std::string user_id, passwd;
            printMessage("Enter user ID and password: ");
            readInput(command);
            std::istringstream iss(command);
            iss >> user_id >> passwd;
            if (user_id.empty() || passwd.empty())
            {
                printMessage("Invalid input. Please provide both user ID and password.");
                break;
            }
            std::string temp_port = std::to_string(port);
            loginUser(clientSocket, user_id, passwd, ip, temp_port);
            break;
        }
        case 3: // Create Group
        {
            std::string group_id;
            printMessage("Enter group ID: ");
            readInput(command);
            std::istringstream iss(command);
            iss>> group_id;
            if(group_id.empty())
            {
                cout<<"please enter group_id"<<endl;
                break;
            }
            createGroup(clientSocket, group_id);
            break;
        }
        case 4: // Join Group
        {
            std::string group_id;
            printMessage("Enter group ID: ");
            readInput(group_id);
            joinGroup(clientSocket, group_id);
            break;
        }
        case 5: // Leave Group
        {
            std::string group_id;
            printMessage("Enter group ID: ");
            readInput(group_id);
            leaveGroup(clientSocket, group_id);
            break;
        }
        case 6: // List Pending Join Requests
        {
            std::string group_id;
            printMessage("Enter group ID: ");
            readInput(group_id);
            listPendingJoinRequests(clientSocket, group_id);
            break;
        }
        case 7: // Accept Join Request
        {
            std::string group_id, user_id;
            printMessage("Enter group ID and user ID: ");
            readInput(command);
            std::istringstream iss(command);
            iss >> group_id >> user_id;
            acceptJoinRequest(clientSocket, group_id, user_id);
            break;
        }
        case 8: // List All Groups
        {
            listAllGroups(clientSocket);
            break;
        }
        case 9: // List All Members of a Group
        {
            std::string group_id;
            printMessage("Enter group ID: ");
            readInput(group_id);
            listGroupMembers(clientSocket, group_id);
            break;
        }
        case 10: // Logout
        {
            logout(clientSocket);
            break;
        }
        case 11: // List All Sharable Files in Group
        {
            std::string group_id;
            printMessage("Enter group ID: ");
            readInput(group_id);
            listFiles(clientSocket, group_id);
            break;
        }
        case 12: // Upload File to Group
        {
            std::string file_path, group_id;
            printMessage("Enter file path: ");
            readInput(file_path);
            printMessage("Enter group ID: ");
            readInput(group_id);
            uploadFile(clientSocket, file_path, group_id);
            break;
        }
        case 13: // Download File from Group
        {
            std::string group_id, file_sha, destination_path;
            printMessage("Enter group ID: ");
            readInput(group_id);
            printMessage("Enter file SHA: ");
            readInput(file_sha);
            printMessage("Enter destination path: ");
            readInput(destination_path);
            download_file(clientSocket, group_id, file_sha, destination_path);
            break;
        }
        case 14:
        {
            string groupid, file_sha;
            printMessage("Enter group ID: ");
            readInput(groupid);
            printMessage("Enter file SHA: ");
            readInput(file_sha);
            stopshare(clientSocket, groupid, file_sha);
            break;

        }
        case 15:
        {
            
            showdownloads(clientSocket);
            break;

        }
        case 0: // Exit
        {
            std::string request = "exit";
            send(clientSocket, request.c_str(), request.size(), 0);
            printMessage("Exiting...");
            return 0;
        }
        default:
            printMessage("Invalid choice. Please try again.");
        }
    }
}



