#include <map>
#include <iostream>      // For std::cout, std::endl
#include <fstream>       // For std::ifstream
#include <sstream>       // For std::stringstream
#include <vector>        // For std::vector
#include <iomanip>       // For std::setw, std::setfill
#include <openssl/sha.h> // For SHA1 functions and SHA_CTX
#include <string>
#include <sys/stat.h>
#include <mutex>
#include <set>
#include <algorithm>
#include <cmath>

// Shared mutex for thread safety
std::mutex downloadMutex;
using namespace std;

void createAndStoreFileMetadata(const string &curr_client, pair<string, pair<string, vector<string>>> &entry);

void loadFileMetadata(const string &curr_client);
// stroing the file hash -> <file_path, vector<chunk_hashes>>
int total_chunks = 0;
map<string, pair<string, vector<string>>> clientFileMetadata;
// map<string, string> chunkStorage;
string curr_client;
int connectToTracker(const string &ip, int port)
{
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (clientSocket < 0)
    {
        cerr << "Error creating client socket" << endl;
        return -1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serverAddress.sin_addr);

    // Connect to tracker
    if (connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        cerr << "Error connecting to tracker having ip " << ip << " and port " << port << endl;
        return -1;
    }

    return clientSocket;
}

void createUserAccount(int clientSocket, const string &user_id, const string &password)
{
    string request = "create_user " + user_id + " " + password;
    send(clientSocket, request.c_str(), request.size(), 0);

    // Receive response
    // Receive the response
    char buffer[1024] = {0}; // Ensure enough space for the response
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

    if (bytesReceived > 0)
    {
        cout << "Tracker response: " << buffer << endl;
    }
    else
    {
        cerr << "Error receiving response from tracker" << endl;
    }
    //   cout << "Tracker response: " << buffer <<   endl;
}

void loginUser(int clientSocket, const string &user_id, const string &password, string &ip, string &port)
{
    string request = "login " + user_id + " " + password + " " + ip + " " + port;
    send(clientSocket, request.c_str(), request.size(), 0);

    // Receive response
    // Receive the response
    char buffer[1024] = {0}; // Ensure enough space for the response
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
    curr_client = user_id;

    if (bytesReceived > 0)
    {
        cout << "Tracker response: " << buffer << endl;
    }
    else
    {
        cerr << "Error receiving response from tracker" << endl;
    }

    loadFileMetadata(curr_client);
    //   cout << "Tracker response: " << buffer <<   endl;
}

void createGroup(int clientSocket, const string &group_id)
{
    string request = "create_group " + group_id;
    //   cout<<"sending request is "<<request<<"\n";
    send(clientSocket, request.c_str(), request.size(), MSG_NOSIGNAL);
    //   cout<<"response sent"<<  endl;
    // Receive response
    // Receive the response
    char buffer[1024] = {0}; // Ensure enough space for the response
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

    if (bytesReceived > 0)
    {
        cout << "Tracker response: " << buffer << endl;
    }
    else
    {
        cerr << "Error receiving response from tracker" << endl;
    }
    //     cout<<"response came"<<  endl;
    //   cout << "Tracker response: " << buffer <<   endl;
}

void joinGroup(int clientSocket, const string &group_id)
{
    string request = "join_group " + group_id;
    send(clientSocket, request.c_str(), request.size(), 0);

    // Receive response
    char buffer[1024] = {0};
    recv(clientSocket, buffer, sizeof(buffer), 0);
    cout << "Tracker response: " << buffer << endl;
}

void listPendingJoinRequests(int clientSocket, const string &group_id)
{
    string request = "list_requests " + group_id;
    send(clientSocket, request.c_str(), request.size(), 0);

    // Receive response
    char buffer[1024] = {0};
    recv(clientSocket, buffer, sizeof(buffer), 0);
    cout << "Tracker response: " << buffer << endl;
}

void leaveGroup(int clientSocket, const string &group_id)
{
    string request = "leave_group " + group_id;
    send(clientSocket, request.c_str(), request.size(), 0);

    // Receive response
    char buffer[1024] = {0};
    recv(clientSocket, buffer, sizeof(buffer), 0);
    cout << "Tracker response: " << buffer << endl;
}

void acceptJoinRequest(int clientSocket, const string &group_id, const string &user_id)
{
    string request = "accept_request " + group_id + " " + user_id;
    send(clientSocket, request.c_str(), request.size(), 0);

    // Receive response
    char buffer[1024] = {0};
    recv(clientSocket, buffer, sizeof(buffer), 0);
    cout << "Tracker response: " << buffer << endl;
}

void listAllGroups(int clientSocket)
{
    string request = "list_groups";
    send(clientSocket, request.c_str(), request.size(), 0);

    // Receive response
    char buffer[1024] = {0};
    recv(clientSocket, buffer, sizeof(buffer), 0);
    cout << "Tracker response: " << buffer << endl;
}

void listGroupMembers(int clientSocket, const std::string &group_id)
{
    std::string request = "list_members " + group_id;
    send(clientSocket, request.c_str(), request.length(), 0);

    // Receive the list of members
    char buffer[1024] = {0};
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

    if (bytesReceived > 0)
    {
        std::cout << "Group members: " << buffer << std::endl;
    }
    else
    {
        std::cerr << "Error receiving response from tracker" << std::endl;
    }
}

void logout(int clientSocket)
{
    std::string request = "logout";
    send(clientSocket, request.c_str(), request.length(), 0);

    // Receive the list of members
    char buffer[1024] = {0};
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

    if (bytesReceived > 0)
    {
        std::cout << buffer << std::endl;
    }
    else
    {
        std::cerr << "Error receiving response from tracker" << std::endl;
    }

    // createAndStoreFileMetadata();
}

void listFiles(int clientSocket, const string &group_id)
{
    string request = "list_files " + group_id;
    send(clientSocket, request.c_str(), request.size(), 0);

    char buffer[1024] = {0};
    recv(clientSocket, buffer, sizeof(buffer), 0);
    cout << "Tracker response: " << buffer << endl;
}

std::pair<string, std::vector<string>> generateFileHash(const string &filePath)
{
    int file = open(filePath.c_str(), O_RDONLY);
    if (file < 0)
    {
        std::cerr << "Error opening file: " << filePath << std::endl;
        return {"", {}}; // Return empty string and vector if file can't be opened
    }

    unsigned char overallHash[SHA_DIGEST_LENGTH];
    SHA_CTX overallCtx;
    SHA1_Init(&overallCtx); // Initialize SHA1 context for overall file hash

    std::vector<string> chunkHashes; // Vector to store chunk hashes
    unsigned char chunkHash[SHA_DIGEST_LENGTH];
    char buffer[512 * 1024]; // Buffer for 512 KB
    ssize_t bytesRead;

    // Read the file in chunks
    while ((bytesRead = read(file, buffer, sizeof(buffer))) > 0)
    {
        // Update the overall hash with the chunk data
        SHA1_Update(&overallCtx, buffer, bytesRead);

        // Compute the hash for the current chunk
        SHA_CTX chunkCtx;
        SHA1_Init(&chunkCtx);                      // Initialize SHA1 context for the chunk
        SHA1_Update(&chunkCtx, buffer, bytesRead); // Update hash with chunk data
        SHA1_Final(chunkHash, &chunkCtx);          // Finalize hash computation for the chunk

        // Convert chunk hash to hex string
        std::stringstream chunkHashStream;
        for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
        {
            chunkHashStream << std::hex << (int)chunkHash[i];
        }
        chunkHashes.push_back(chunkHashStream.str()); // Store chunk hash
    }

    // Finalize overall hash computation
    SHA1_Final(overallHash, &overallCtx);

    close(file); // Close the file

    // Convert overall file hash to hex string
    std::stringstream overallHashStream;
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
        overallHashStream << std::hex << (int)overallHash[i];
    }

    // Return a pair of overall file hash and vector of chunk hashes
    return {overallHashStream.str(), chunkHashes};
}

void uploadFile(int clientSocket, const string &filePath, const string &group_id)
{
    if (group_id.empty())
    {
        cout << "Enter a valid group ID." << endl;
        return;
    }

    // Open the file
    ifstream file(filePath, ios::binary);
    if (!file.is_open())
    {
        cout << "Error opening file: " << filePath << endl;
        return;
    }

    // knowing file size
    // Move the file pointer to the end
    // file.seekg(0, std::ios::end);

    // Get the position of the file pointer (file size) in bytes
    std::streampos fileSize = file.tellg();

    const size_t CHUNK_SIZE = 512 * 1024; // 512 KB
    vector<string> chunkHashesVec;
    string fullFileHash;
    vector<char> buffe(CHUNK_SIZE);
    SHA_CTX sha1Context;

    // Initialize the SHA1 context for full file hash calculation
    SHA1_Init(&sha1Context);

    while (file.read(buffe.data(), CHUNK_SIZE) || file.gcount() > 0)
    {
        cout<<"yes i am insde loop"<<endl;
        size_t bytesRead = file.gcount();

        // Calculate chunk hash
        unsigned char chunkHash[SHA_DIGEST_LENGTH];
        SHA1(reinterpret_cast<const unsigned char *>(buffe.data()), bytesRead, chunkHash);

        // Convert chunk hash to hexadecimal string
        stringstream ss;
        for (int i = 0; i < SHA_DIGEST_LENGTH; ++i)
        {
            ss << hex << setw(2) << setfill('0') << static_cast<int>(chunkHash[i]);
        }
        cout<<"pushing entry to vec "<<ss.str()<<endl;
        chunkHashesVec.push_back(ss.str());

        // Update full file hash context
        SHA1_Update(&sha1Context, buffe.data(), bytesRead);
    }
    // int number_of_chunks = ceil((double)fileSize / CHUNK_SIZE);

    // Finalize the full file hash
    unsigned char fileHashBytes[SHA_DIGEST_LENGTH];
    SHA1_Final(fileHashBytes, &sha1Context);

    stringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i)
    {
        ss << hex << setw(2) << setfill('0') << static_cast<int>(fileHashBytes[i]);
    }
    fullFileHash = ss.str();

    file.close();

    if (fullFileHash.empty())
    {
        cout << "Error generating file hash." << endl;
        return;
    }

    // Extract filename from the file path
    string fileName = filePath.substr(filePath.find_last_of("/") + 1);

    // Format the message to send to the tracker
    string request = "upload_file " + group_id + " " + fileName + " " + fullFileHash;
    // for (const auto& chunkHash : chunkHashesVec) {
    //     request += chunkHash + " ";
    // }

    // Send the file metadata to the tracker
    cout << "Sending metadata to tracker..." << endl;
    send(clientSocket, request.c_str(), request.size(), 0);

    // Receive the response from the tracker
    char buffer[1024] = {0};
    recv(clientSocket, buffer, sizeof(buffer), 0);
    string response(buffer);

    if (response != "You are not a member of this group." && response != "This file path already exists.")
    {
        clientFileMetadata[fullFileHash] = {filePath, chunkHashesVec};
        cout<<"storing entry in map size of chunkhashvec "<<chunkHashesVec.size()<<endl;
        cout << "stored entry with " << fullFileHash << endl;
        pair<string, pair<string, vector<string>>> entry = {fullFileHash, {filePath, chunkHashesVec}};
        createAndStoreFileMetadata(curr_client, entry);
    }

    cout << "Tracker response: " << response << endl;
}

void createAndStoreFileMetadata(const string &curr_client, pair<string, pair<string, vector<string>>> &entry)
{
    // Ensure the "filemetadata" folder exists
    const string folderName = "filemetadata";
    mkdir(folderName.c_str(), 0777);

    // Define the file path
    string filePath = folderName + "/" + curr_client + ".txt";

    // Open the file for appending
    ofstream outFile(filePath, ios::app);
    if (!outFile)
    {
        cerr << "Error creating file: " << filePath << endl;
        return;
    }

    // Write the entry to the file
    string fileHash = entry.first, filepath = entry.second.first;

    outFile << fileHash << " ";
    outFile << filepath << " ";

    for (const auto &chunkHash : entry.second.second)
    {
        outFile << chunkHash << " ";
    }
    outFile << "\n";

    outFile.close();
    cout << "Metadata stored in file: " << filePath << endl;
}

void loadFileMetadata(const string &curr_client)
{
    // Define the file path
    const string folderName = "filemetadata";
    string filePath = folderName + "/" + curr_client + ".txt";

    // Open the file for reading
    ifstream inFile(filePath);
    if (!inFile)
    {
        // cerr << "Error opening file: " << filePath << endl;
        return;
    }

    string line;
    while (getline(inFile, line))
    {
        istringstream iss(line);
        string fileHash, filePathValue, chunkHash;
        vector<string> chunkHashes;

        if (iss >> fileHash >> filePathValue)
        {
            while (iss >> chunkHash)
            {
                chunkHashes.push_back(chunkHash);
            }
            clientFileMetadata[fileHash] = {filePathValue, chunkHashes};
        }
    }

    inFile.close();
    cout << "Metadata loaded from file: " << filePath << endl;
}

int connectToPeer(const string &peer_ip, int peer_port)
{
    // Create a socket
    int peerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (peerSocket < 0)
    {
        cerr << "Error creating socket." << endl;
        return -1;
    }

    // Set up the server address structure
    sockaddr_in peerAddr = {0};
    peerAddr.sin_family = AF_INET;
    peerAddr.sin_port = htons(peer_port);

    // Convert IP address from string to binary form
    if (inet_pton(AF_INET, peer_ip.c_str(), &peerAddr.sin_addr) <= 0)
    {
        cerr << "Invalid IP address: " << peer_ip << endl;
        close(peerSocket);
        return -1;
    }

    // Connect to the peer
    if (connect(peerSocket, (struct sockaddr *)&peerAddr, sizeof(peerAddr)) < 0)
    {
        cerr << "Connection to " << peer_ip << ":" << peer_port << " failed." << endl;
        close(peerSocket);
        return -1;
    }
    else
    {
        cout << "connected to ip" << peer_ip << "  and port " << peer_port << endl;
    }

    return peerSocket; // Return the socket descriptor for communication
}

// Function to connect to peer and fetch chunk information
void fetchChunkInfo(const string &peer_ip, int peer_port, map<int, set<string>> &chunkToPeers, const string &file_sha)
{
    int peerSocket = connectToPeer(peer_ip, peer_port);
    if (peerSocket < 0)
    {
        cerr << "Failed to connect to peer: " << peer_ip << ":" << peer_port << endl;
        return;
    }

    // Request chunk information
    string request = "chunk_info " + file_sha;
    send(peerSocket, request.c_str(), request.size(), 0);

    // Receive chunk info from peer
    char buffer[4096] = {0};
    recv(peerSocket, buffer, sizeof(buffer), 0);
    string chunkInfo(buffer);

    // Parse chunk info
    istringstream iss(chunkInfo);
    int chunk_id;
    while (iss >> chunk_id)
    {
        // std::lock_guard<std::mutex> lock(downloadMutex);
        chunkToPeers[chunk_id].insert(peer_ip + " " + to_string(peer_port));
    }

    close(peerSocket);
}

// Function to calculate SHA1 hash of a given data
string calculateSHA1(const char *data, size_t size)
{
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char *>(data), size, hash);

    stringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i)
    {
        ss << hex << setw(2) << setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

// Function to download a chunk
void downloadChunk(const int &chunk_no, const string &peer_info, const string &destination_path, const string &filesha)
{
    istringstream peerStream(peer_info);
    string peer_ip;
    int peer_port;
    peerStream >> peer_ip >> peer_port;

    int peerSocket = connectToPeer(peer_ip, peer_port);
    if (peerSocket < 0)
    {
        cerr << "Failed to connect to peer: " << peer_ip << ":" << peer_port << endl;
        return;
    }

    // Request specific chunk
    string request = "download_chunk " + filesha + " " + to_string(chunk_no);
    send(peerSocket, request.c_str(), request.size(), 0);

    // Receive chunk data
    char chunkBuffer[512 * 1024 + 42]; // 512 KB buffer + extra 41 bytes for chunk sha and spcace
    int bytesReceived = recv(peerSocket, chunkBuffer, sizeof(chunkBuffer), 0);

    if (bytesReceived > 0)
    {
        string receivedData(chunkBuffer, bytesReceived);
        size_t spacePos = receivedData.find(' ');

        if (spacePos != string::npos)
        {
            string receivedChunkSha = receivedData.substr(0, spacePos); // Extract received chunk SHA
            string chunkContent = receivedData.substr(spacePos + 1);    // Extract chunk content
            cout<<"chunkContent is"<<chunkContent<<endl;

            // Calculate SHA1 of the chunk content
            string calculatedSha = calculateSHA1(chunkContent.data(), chunkContent.size());

            // Verify chunk integrity
            if (calculatedSha == receivedChunkSha)
            {
                cout << "Chunk integrity verified. Writing to file..." << endl;
                // here
                std::lock_guard<std::mutex> lock(downloadMutex);
                ofstream outFile(destination_path, ios::binary | ios::out);
                cout << "destination path" << destination_path << endl;
                if (!outFile)
                {
                    cout << "error in writing file" << endl;
                }
                outFile.seekp(chunk_no * 512 * 1024);
                outFile.write(chunkContent.data(), chunkContent.size());
                outFile.close();

                // clientFileMetadata[filesha]={destination_path,}
            }
            else
            {
                cerr << "Error: Chunk integrity check failed for chunk " << chunk_no << endl;
            }
        }
        else
        {
            cerr << "Error: Malformed chunk data received." << endl;
        }
    }
    else
    {
        cerr << "Error: Failed to receive chunk data from peer." << endl;
    }

    close(peerSocket);
}

void download_file(int clientSocket, const string &group_id, const string &file_sha, const string &destination_path)
{

    // Send request to tracker for peer information
    string request = "download_file " + group_id + " " + file_sha;
    send(clientSocket, request.c_str(), request.size(), 0);

    // Receive peer information from tracker

    char buffer[4096] = {0};
    recv(clientSocket, buffer, sizeof(buffer), 0);
    string peerInfo(buffer);
    cout << "reponse from tracker " << peerInfo << endl;

    if (peerInfo == "File not found")
    {
        cout << "Error: File not found in group." << endl;
        return;
    }

    // Parse peer information
    istringstream peerStream(peerInfo);
    string peer_ip;
    int peer_port;
    vector<pair<string, int>> peers;
    cout << "parsing peerinfo" << endl;
    while (peerStream >> peer_ip >> peer_port)
    {
        cout << "peer_ip " << peer_ip << " peer_port" << peer_port << endl;

        peers.emplace_back(peer_ip, peer_port);
    }

    // Fetch chunk information from peers
    map<int, set<string>> chunkToPeers; // chunk_sha -> {peers}
    vector<thread> threads;
    for (const auto &[peer_ip1, peer_port1] : peers)
    {
        cout << "peer_ip " << peer_ip1 << " peer_port" << peer_port1 << endl;
        // threads.emplace_back(fetchChunkInfo, peer_ip, peer_port, std::ref(chunkToPeers), file_sha);
        fetchChunkInfo(peer_ip1, peer_port1, chunkToPeers, file_sha);
    }
    // this file have have total these many chunks
    total_chunks = chunkToPeers.size();

    // assume first all the chunks will be downloaded
    clientFileMetadata[file_sha].first = destination_path;
    for (int i = 0; i < total_chunks; i++)
    {
        // just pushing empty string so that we can directly acces v[i]
        clientFileMetadata[file_sha].second.push_back("");
    }

    // after using total_chunks variable set this variable to 0 as it is gloabal
    total_chunks = 0;
    // Wait for all threads to complete
    // for (auto& t : threads)
    // {
    //     t.join();
    // }

    // Apply rarest-first piece selection
    vector<int> chunks;
    for (const auto &[chunk_no, peerSet] : chunkToPeers)
    {
        chunks.push_back(chunk_no);
    }
    sort(chunks.begin(), chunks.end(), [&chunkToPeers](const int &a, const int &b)
         { return chunkToPeers[a].size() < chunkToPeers[b].size(); }); // Rarest first

    // Download chunks using multithreading
    vector<thread> downloadThreads;
    int chunk_id = 0;
    for (const int &chunk_no : chunks)
    {
        for (const string &peer_info : chunkToPeers[chunk_no])
        {
            downloadThreads.emplace_back(downloadChunk, chunk_no, peer_info, destination_path, file_sha);
        }
    }

    // if atleast one chunk of file is not downloaded just do following
    // clientFileMetadata.erase(file_sha); // we wiil try our best to download at least one chunk as chunk may
    // availbale to many peers

    
    // Wait for all downloads to complete
    for (auto &t : downloadThreads)
    {
        t.join();
    }

    cout << "File downloaded successfully to " << destination_path << endl;
}