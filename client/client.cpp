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

void sendChunk(int clientSocket, const string& chunk_sha)
{
    // Check if the requested chunk exists
    if (clientFileMetadata.find(chunk_sha) == clientFileMetadata.end())
    {
        string errorMessage = "Chunk not found";
        send(clientSocket, errorMessage.c_str(), errorMessage.size(), 0);
        return;
    }

    // Open the file containing the chunk
    string filePath = clientFileMetadata[chunk_sha].first;
    ifstream chunkFile(filePath, ios::binary);
    if (!chunkFile.is_open())
    {
        string errorMessage = "Error opening chunk file";
        send(clientSocket, errorMessage.c_str(), errorMessage.size(), 0);
        return;
    }

    // Read the chunk data
    char buffer[512 * 1024]; // 512 KB buffer
    chunkFile.read(buffer, sizeof(buffer));
    streamsize bytesRead = chunkFile.gcount();
    chunkFile.close();
    string response(buffer);
    // Send the chunk data to the client
    if (bytesRead > 0)
    {
        cout<<"sending final chunk"<<response<<endl;
        send(clientSocket, response.c_str(), response.size(), 0);
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
        // cout<<" request to client from client "<<request<<endl;
        // cout<<"listenport"<<listenPort<<endl;
        if(request.rfind("chunk_info", 0) == 0)
        {
            // cout<<"request.substr(10) in client server side just check sha"<<request.substr(11)<<"end"<<endl;

             auto it=clientFileMetadata.begin();
            //  cout<<"it first is"<<it->first<<"end"<<endl;

            if(clientFileMetadata.find(request.substr(11))==clientFileMetadata.end())
            {
                cout<<" I can't send first reponse there is no entry with given hash "<<endl;
                
                close(clientSocket);
                continue;
            }
            
             vector<string> chunk_hashes= clientFileMetadata[request.substr(11)].second;
             if(chunk_hashes.size()==0)
             {
                cout<<" i can't send 1st response as entry exist but with o chunks "<<endl;
                close(clientSocket);
                continue;
             }
             string response;
            int count=0;
             for(auto & i:chunk_hashes)
             {
                response+= i + " " + to_string(count++) + " ";
             }
             cout<<" sending 1st response from client server side : " <<response<<endl;

             send(clientSocket, response.c_str(),response.size(),0);
        }
        else if (request.rfind("download_chunk", 0) == 0)
        {
            string chunk_sha = request.substr(15);
            cout<<"sending 2nd response from client side : "<<chunk_sha<<endl;
            sendChunk(clientSocket, chunk_sha);
        }

        close(clientSocket);
    }
}




int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        cerr << "Usage: ./client <IP>:<PORT> tracker_info.txt" << endl;
        return 1;
    }

    // Step 1: Parse the <IP>:<PORT> argument
    string ip_port = argv[1];
    size_t colon_pos = ip_port.find(':');
    if (colon_pos == string::npos)
    {
        cerr << "Invalid IP:PORT format. Expected <IP>:<PORT>." << endl;
        return 1;
    }

    string ip = ip_port.substr(0, colon_pos); // Extract IP
    int port;
    try
    {
        port = stoi(ip_port.substr(colon_pos + 1)); // Extract PORT and convert to integer
    }
    catch (const invalid_argument &e)
    {
        cerr << "Invalid port number." << endl;
        return 1;
    }

    string trackerInfoFile = argv[2]; // tracker_info.txt path

    // Step 2: Read the tracker info
    vector<TrackerInfo> trackers = readTrackerInfo(trackerInfoFile);
    if (trackers.empty())
    {
        cerr << "No trackers found in tracker_info.txt" << endl;
        return 1;
    }

    // Step 3: Connect to the first tracker
    int clientSocket = connectToTracker(trackers[0].ip, trackers[0].port);
    if (clientSocket < 0)
    {
        cerr << "Cannot connect to tracker" << endl;
        return 1;
    }

    std::thread listener(listenForChunkRequests, port);
    listener.detach();

    // Handle commands
    string command;
    while (true)
    {

        cout << "\nMenu:\n";
        cout << "1. Create User\n";
        cout << "2. Login\n";
        cout << "3. Create Group\n";
        cout << "4. Join Group\n";
        cout << "5. Leave Group\n";
        cout << "6. List Pending Join Requests\n";
        cout << "7. Accept Join Request\n";
        cout << "8. List All Groups\n";
        cout << "9. List All Members of a Group\n";
        cout << "10. to logout\n";
        cout << "11. List All Sharable Files in Group\n";
        cout << "12. Upload File to Group\n";
         cout << "13. dowload File from Group\n";
        cout << "0. Exit\n";

        cout << "Enter your choice: ";

        std::getline(std::cin, command);

        // Convert command input to an integer
        int choice;
        try
        {
            choice = std::stoi(command); // Convert the entire string input to an integer
        }
        catch (std::invalid_argument &e)
        {
            std::cerr << "Invalid input. Please enter a valid number." << std::endl;
            continue;
        }

        switch (choice)
        {
        case 1:
        { // Create User
            string user_id, passwd;
            cout << "Enter user ID and password: ";
            getline(cin, command);
            istringstream iss(command);
            iss >> user_id >> passwd;
            if (user_id.empty() || passwd.empty())
            {
                cerr << "Invalid input. Please provide both user ID and password." << endl;
                break;
            }
            createUserAccount(clientSocket, user_id, passwd);
            break;
        }

        case 2:
        { // Login
            string user_id, passwd;
            cout << "Enter user ID and password: ";
            getline(cin, command);
            istringstream iss(command);
            iss >> user_id >> passwd;
            if (user_id.empty() || passwd.empty())
            {
                cerr << "Invalid input. Please provide both user ID and password." << endl;
                break;
            }
            string temp_port = to_string(port);
            loginUser(clientSocket, user_id, passwd, ip, temp_port);
            break;
        }
        case 3:
        { // Create Group
            string group_id;
            cout << "Enter group ID: ";
            getline(cin, group_id);
            createGroup(clientSocket, group_id);
            break;
        }
        case 4:
        { // Join Group
            string group_id;
            cout << "Enter group ID  ";
            getline(cin, group_id);
            joinGroup(clientSocket, group_id);
            break;
        }
        case 5:
        { // Leave Group
            string group_id;
            cout << "Enter group ID  ";
            getline(cin, group_id);
            leaveGroup(clientSocket, group_id);
            break;
        }
        case 6:
        { // List Pending Join Requests
            string group_id;
            cout << "Enter group ID : ";
            getline(cin, group_id);
            listPendingJoinRequests(clientSocket, group_id);
            break;
        }
        case 7:
        { // Accept Join Request
            string group_id, user_id;
            cout << "Enter group ID and user id: ";
            getline(cin, command);
            istringstream iss(command);
            iss >> group_id >> user_id;
            acceptJoinRequest(clientSocket, group_id, user_id);
            break;
        }
        case 8:
        { // List All Groups
            listAllGroups(clientSocket);
            break;
        }
        case 9:
        { // List All Members of a Group (New Option)
            std::string group_id;
            std::cout << "Enter group ID: ";
            std::getline(std::cin, group_id);
            listGroupMembers(clientSocket, group_id); // New function to handle the request
            break;
        }
        case 10:
        {

            logout(clientSocket);
            break;
        }
        case 11:
        {
            string group_id;
            cout << "Enter group ID: ";
            getline(cin, group_id);
            listFiles(clientSocket, group_id);
            break;
        }

        case 12:
        {
            string file_path, group_id;
            cout << "Enter file path and group ID: ";
            getline(cin, file_path);
            getline(cin, group_id);
            uploadFile(clientSocket, file_path, group_id);
            break;
        }

        case 13:
        {
            string group_id, file_sha, destination_path;
            cout<<"enter gorupid filesha destination path: ";
            cin>> group_id>>file_sha>>destination_path;
            download_file(clientSocket,group_id,file_sha,destination_path);
            break;
        }

        case 0: // Exit
        {
            string request = "exit";
            send(clientSocket, request.c_str(), request.size(), 0);
            cout << "Exiting..." << endl;
            // close(clientSocket);
            return 0;
        }

        default:
            cout << "Invalid choice. Please try again." << endl;
        }
    }
}

//    cout<<"coming out of while\n";

// }
