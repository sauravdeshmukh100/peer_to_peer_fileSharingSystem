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
#include <queue>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <filesystem>

// Shared mutex for thread safety
std::mutex downloadMutex;
using namespace std;
// size_t FILESIZE;
int CHUNK_SIZE = 512 * 1024; // 512 *1024 = 512 KB
std::mutex ioMutex;
// mainting following maps for status of file . D (Downloading), C (Complete)
map<string, pair<string, int>> D; /* status of files : DOWNLOADING -> map from file sha to {group_id, no of chunks downloaded}*/
map<string, pair<string, int>> C; /*status of files : COMPLETED -> map from file sha to {group_id, no of chunks downloaded}*/
void printMessage(const std::string &message)
{
    // std::lock_guard<std::mutex> lock(ioMutex); // Lock the mutex
    std::cout << message << std::endl;
}

void readInput(std::string &input)
{
    // std::lock_guard<std::mutex> lock(ioMutex); // Lock the mutex
    // std::cout << "Enter input: ";
    std::getline(std::cin, input);
}

class ThreadPool
{
public:
    ThreadPool(size_t numThreads)
    {
        for (size_t i = 0; i < numThreads; ++i)
        {
            workers.emplace_back([this]
                                 {
                while (true)
                {
                    std::function<void()> task;

                    // Lock the task queue
                    {
                        std::unique_lock<std::mutex> lock(queueMutex);
                        condition.wait(lock, [this] { return !tasks.empty() || stop; });

                        if (stop && tasks.empty())
                            return; // Exit thread if stopping and no tasks remain

                        task = std::move(tasks.front());
                        tasks.pop();
                    }

                    // Execute the task
                    task();
                } });
        }
    }

    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread &worker : workers)
            worker.join();
    }

    // Add a new task to the pool
    void enqueue(std::function<void()> task)
    {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            tasks.emplace(std::move(task));
        }
        condition.notify_one();
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop = false;
};

void createAndStoreFileMetadata(const string &curr_client, pair<string, pair<string, vector<string>>> &entry);

void loadFileMetadata(const string &curr_client);
// stroing the file hash -> <file_path, vector<chunk_hashes>>

map<string, pair<string, vector<string>>> clientFileMetadata;
// map<string, string> chunkStorage;
string curr_client;
int connectToTracker(const string &ip, int port)
{
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (clientSocket < 0)
    {
        printMessage("Error creating client socket");
        return -1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serverAddress.sin_addr);

    // Connect to tracker
    if (connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        printMessage("Error connecting to tracker having ip " + ip + " and port " + to_string(port));
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
        printMessage("Tracker response: " + string(buffer));
    }
    else
    {
        printMessage("Error receiving response from tracker");
    }
    //   printMessage("Tracker response: " + string(buffer));
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
        printMessage("Tracker response: " + string(buffer));
    }
    else
    {
        printMessage("Error receiving response from tracker");
    }

    loadFileMetadata(curr_client);
    //   printMessage("Tracker response: " + string(buffer));
}

void createGroup(int clientSocket, const string &group_id)
{
    string request = "create_group " + group_id;
    //   printMessage("sending request is " + request);
    send(clientSocket, request.c_str(), request.size(), MSG_NOSIGNAL);
    //   printMessage("response sent");
    // Receive response
    // Receive the response
    char buffer[1024] = {0}; // Ensure enough space for the response
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

    if (bytesReceived > 0)
    {
        printMessage("Tracker response: " + string(buffer));
    }
    else
    {
        printMessage("Error receiving response from tracker");
    }
    //     printMessage("response came");
    //   printMessage("Tracker response: " + string(buffer));
}

void joinGroup(int clientSocket, const string &group_id)
{
    string request = "join_group " + group_id;
    send(clientSocket, request.c_str(), request.size(), 0);

    // Receive response
    char buffer[1024] = {0};
    recv(clientSocket, buffer, sizeof(buffer), 0);
    printMessage("Tracker response: " + string(buffer));
}

void listPendingJoinRequests(int clientSocket, const string &group_id)
{
    string request = "list_requests " + group_id;
    send(clientSocket, request.c_str(), request.size(), 0);

    // Receive response
    char buffer[1024] = {0};
    recv(clientSocket, buffer, sizeof(buffer), 0);
    printMessage("Tracker response: " + string(buffer));
}

void leaveGroup(int clientSocket, const string &group_id)
{
    string request = "leave_group " + group_id;
    send(clientSocket, request.c_str(), request.size(), 0);

    // Receive response
    char buffer[1024] = {0};
    recv(clientSocket, buffer, sizeof(buffer), 0);
    printMessage("Tracker response: " + string(buffer));
}

void acceptJoinRequest(int clientSocket, const string &group_id, const string &user_id)
{
    string request = "accept_request " + group_id + " " + user_id;
    send(clientSocket, request.c_str(), request.size(), 0);

    // Receive response
    char buffer[1024] = {0};
    recv(clientSocket, buffer, sizeof(buffer), 0);
    printMessage("Tracker response: " + string(buffer));
}

void listAllGroups(int clientSocket)
{
    string request = "list_groups";
    send(clientSocket, request.c_str(), request.size(), 0);

    // Receive response
    char buffer[1024] = {0};
    recv(clientSocket, buffer, sizeof(buffer), 0);
    printMessage("Tracker response: " + string(buffer));
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
        printMessage("Group members: " + string(buffer));
    }
    else
    {
        printMessage("Error receiving response from tracker");
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
        printMessage(string(buffer));
    }
    else
    {
        printMessage("Error receiving response from tracker");
    }

    // createAndStoreFileMetadata();
}

void listFiles(int clientSocket, const string &group_id)
{
    string request = "list_files " + group_id;
    send(clientSocket, request.c_str(), request.size(), 0);

    char buffer[1024] = {0};
    recv(clientSocket, buffer, sizeof(buffer), 0);
    printMessage("Tracker response: " + string(buffer));
}

std::pair<string, std::vector<string>> generateFileHash(const string &filePath)
{
    int file = open(filePath.c_str(), O_RDONLY);
    if (file < 0)
    {
        printMessage("Error opening file: " + filePath);
        return {"", {}}; // Return empty string and vector if file can't be opened
    }

    unsigned char overallHash[SHA_DIGEST_LENGTH];
    SHA_CTX overallCtx;
    SHA1_Init(&overallCtx); // Initialize SHA1 context for overall file hash

    std::vector<string> chunkHashes; // Vector to store chunk hashes
    unsigned char chunkHash[SHA_DIGEST_LENGTH];
    char buffer[CHUNK_SIZE]; // Buffer for 512 KB
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

size_t calculateFileSize(const std::string &filePath)
{
    try
    {
        return std::filesystem::file_size(filePath);
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        std::cerr << "Error calculating file size: " << e.what() << std::endl;
        return 0;
    }
}

void uploadFile(int clientSocket, const string &filePath, const string &group_id)
{
    if (group_id.empty())
    {
        printMessage("Enter a valid group ID.");
        return;
    }

    // Open the file
    ifstream file(filePath, ios::binary);
    if (!file.is_open())
    {
        printMessage("Error opening file: " + filePath);
        return;
    }

    // 512 KB
    vector<string> chunkHashesVec;
    string fullFileHash;
    vector<char> buffe(CHUNK_SIZE);
    SHA_CTX sha1Context;

    // Initialize the SHA1 context for full file hash calculation
    SHA1_Init(&sha1Context);

    while (file.read(buffe.data(), CHUNK_SIZE) || file.gcount() > 0)
    {

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
        // printMessage("pushing entry to vec " + ss.str());
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
        printMessage("Error generating file hash.");
        return;
    }

    // Get the position of the file pointer (file size) in bytes
    size_t fileSize = calculateFileSize(filePath);
    // cout << "filesize=" << fileSize << endl;

    // Extract filename from the file path
    string fileName = filePath.substr(filePath.find_last_of("/") + 1);

    // Format the message to send to the tracker
    string request = "upload_file " + group_id + " " + fileName + " " + to_string(fileSize) + " " + fullFileHash;
    // for (const auto& chunkHash : chunkHashesVec) {
    //     request += chunkHash + " ";
    // }

    // Send the file metadata to the tracker
    printMessage("Sending metadata to tracker...");

    send(clientSocket, request.c_str(), request.size(), 0);

    // Receive the response from the tracker
    char buffer[1024] = {0};
    recv(clientSocket, buffer, sizeof(buffer), 0);
    string response(buffer);

    if (response != "You are not a member of this group." && response != "This file path already exists.")
    {
        clientFileMetadata[fullFileHash] = {filePath, chunkHashesVec};
        printMessage("storing entry in map size of chunkhashvec " + to_string(chunkHashesVec.size()));
        printMessage("stored entry with " + fullFileHash);

        pair<string, pair<string, vector<string>>> entry = {fullFileHash, {filePath, chunkHashesVec}};
        createAndStoreFileMetadata(curr_client, entry);
    }

    printMessage("Tracker response: " + response);
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
        printMessage("Error creating file: " + filePath);
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
    printMessage("Metadata stored in file: " + filePath);
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
    printMessage("Metadata loaded from file: " + filePath);
}

int connectToPeer(const string &peer_ip, int peer_port)
{
    // Create a socket
    int peerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (peerSocket < 0)
    {
        printMessage("Error creating socket.");
        return -1;
    }

    // Set up the server address structure
    sockaddr_in peerAddr = {0};
    peerAddr.sin_family = AF_INET;
    peerAddr.sin_port = htons(peer_port);

    // Convert IP address from string to binary form
    if (inet_pton(AF_INET, peer_ip.c_str(), &peerAddr.sin_addr) <= 0)
    {
        printMessage("Invalid IP address: " + peer_ip);
        close(peerSocket);
        return -1;
    }

    // Connect to the peer
    if (connect(peerSocket, (struct sockaddr *)&peerAddr, sizeof(peerAddr)) < 0)
    {
        printMessage("Connection to " + peer_ip + ":" + to_string(peer_port) + " failed.");
        close(peerSocket);
        return -1;
    }
    else
    {
        // printMessage("Connected to IP: " + peer_ip + " and port: " + to_string(peer_port));
    }

    return peerSocket; // Return the socket descriptor for communication
}

// Function to connect to peer and fetch chunk information
bool fetchChunkInfo(const string &peer_ip, int peer_port, map<int, set<string>> &chunkToPeers, const string &file_sha)
{
    int peerSocket = connectToPeer(peer_ip, peer_port);
    if (peerSocket < 0)
    {
        printMessage("Failed to connect to peer: " + peer_ip + ":" + to_string(peer_port));
        return false;
    }

    // Request chunk information
    string request = "chunk_info " + file_sha;
    send(peerSocket, request.c_str(), request.size(), 0);
    if (request == "entry not found for this file" or request == "no chunk found for this entry")
    {
        printMessage("Response from tracker: " + request);
        return false;
    }
    // Receive chunk info from peer
    char buffer[4096] = {0};
    recv(peerSocket, buffer, sizeof(buffer), 0);
    string chunkInfo(buffer);

    // Parse chunk info
    istringstream iss(chunkInfo);
    int chunk_id;
    while (iss >> chunk_id)
    {
        chunkToPeers[chunk_id].insert(peer_ip + " " + to_string(peer_port));
    }
    return true;
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

bool ensureFileExists(const std::string &filePath)
{
    // Check if file exists
    if (std::filesystem::exists(filePath))
    {
        // Truncate the file if it exists
        std::ofstream truncateFile(filePath, std::ios::binary | std::ios::trunc);
        if (!truncateFile)
        {
            std::cerr << "Error: Unable to truncate file: " << filePath << std::endl;
            return false;
        }
        //     truncateFile.seekp(total_chunks * CHUNK_SIZE - 1); // Move to the last byte
        //    truncateFile.write("", 1);
        truncateFile.close(); // Close after truncation
    }
    else
    {
        // Create the file if it does not exist
        std::ofstream tempFile(filePath, std::ios::binary);
        if (!tempFile)
        {
            std::cerr << "Error: Unable to create file: " << filePath << std::endl;
            return false;
        }
        //     tempFile.seekp(total_chunks * CHUNK_SIZE - 1); // Move to the last byte
        //    tempFile.write("", 1);
        tempFile.close(); // Close after creation
    }
    // cout << "inside ensurefilecontent" << endl;

    return true;
}

bool writeChunkToFile(const std::string &filePath, const std::string &chunkContent, int chunk_no, size_t chunkSize)
{
    // Ensure the file exists
    // if (!ensureFileExists(filePath))
    // {
    //     return false;
    // }

    // Open the file for updating
    std::fstream outFile(filePath, std::ios::binary | std::ios::in | std::ios::out);
    // std::fstream outFileUpdate(destination_path, std::ios::binary | std::ios::in | std::ios::out);

    if (!outFile)
    {
        std::cerr << "Error: Unable to open file for writing: " << filePath << std::endl;
        return false;
    }

    // Seek to the correct position and write the chunk
    // cout<<"writng chunk to file"<<chunkContent<<endl;
    outFile.seekp(chunk_no * chunkSize);
    // Print file pointer position
    std::streampos currentPosition = outFile.tellp();
    // if(chunk_no==0)
    // cout << "writing content of " << chunk_no << " at " << currentPosition << " and the content is /n" << chunkContent << endl
    //  << endl;
    // std::cout << "File pointer position after seekp: for chunk_no "<<chunk_no<<" " <<  << std::endl;
    outFile.write(chunkContent.data(), chunkContent.size());
    // outFile.close();

    return true;
}

// Function to download a chunk
bool downloadChunk(int clientSocket, const int &chunk_no, const string &peer_info, const string &destination_path, const string &filesha, string group_id, string fs, int total_chunks)
{
    istringstream peerStream(peer_info);

    string peer_ip;
    int peer_port;
    peerStream >> peer_ip >> peer_port;
    if (peer_ip.empty() || peer_port <= 0)
    {
        printMessage("Invalid peer_info in downloading chunks: " + peer_info);
        return false;
    }

    int peerSocket = connectToPeer(peer_ip, peer_port);
    if (peerSocket < 0)
    {
        printMessage("Failed to connect to peer: " + peer_ip + ":" + to_string(peer_port));
        return false;
    }
    // Request specific chunk
    string request = "download_chunk " + filesha + " " + to_string(chunk_no);
    send(peerSocket, request.c_str(), request.size(), 0);

    // Calculate size to receive
    size_t size = 41;           // For SHA and space
    size_t filesize = stoi(fs); // Example 1 MB file size
    // cout << "filesize in download chunk" << filesize << endl;
    int q = filesize / CHUNK_SIZE;

    if (chunk_no == q)
    {
        size += filesize % CHUNK_SIZE;
    }
    else
    {
        size += CHUNK_SIZE;
    }

    // Allocate buffer dynamically

    // cout << "chunk_no=" << chunk_no << endl;
    char *chunkBuffer = new char[size];
    // cout << "Expected size for chunk " << chunk_no << ": " << size << endl;

    // Receive chunk data
    size_t bytesReceived = 0;
    // cout << "outside loop" << endl;
    while (bytesReceived < size)
    {
        // cout << "Waiting to receive " << size - bytesReceived << " more bytes for chunk " << chunk_no << endl;

        int recieved = recv(peerSocket, chunkBuffer + bytesReceived, size - bytesReceived, 0);
        if (recieved <= 0)
        {
            if (recieved == 0)
            {
                std::cerr << "Connection closed by peer for chunk " << chunk_no << "." << std::endl;
            }
            else
            {
                std::cerr << "Error during recv for chunk " << chunk_no << ": " << strerror(errno) << std::endl;
            }
            delete[] chunkBuffer;
            close(peerSocket);
            return false;
        }

        bytesReceived += recieved;
        // cout << "Received " << recieved << " bytes. Total received for chunk " << chunk_no << ": " << bytesReceived << endl;
    }

    // cout << "below loop" << endl;

    // Parse received data
    string receivedData(chunkBuffer, bytesReceived);
    delete[] chunkBuffer; // Free the buffer
    size_t spacePos = receivedData.find(' ');

    if (spacePos != string::npos)
    {
        string receivedChunkSha = receivedData.substr(0, spacePos); // Extract received chunk SHA
        string chunkContent = receivedData.substr(spacePos + 1);    // Extract chunk content

        if (chunkContent.empty())
        {
            printMessage("Error: Received empty chunk content for chunk " + to_string(chunk_no));
            close(peerSocket);
            // cout << "getting of for chunk " << chunk_no << endl;
            return false;
        }

        // Calculate SHA1 of the chunk content
        string calculatedSha = calculateSHA1(chunkContent.data(), chunkContent.size());

        // Verify chunk integrity
        if (calculatedSha == receivedChunkSha)
        {
            if (!writeChunkToFile(destination_path, chunkContent, chunk_no, CHUNK_SIZE))
            {
                printMessage("Error: Failed to write chunk to file.");
                close(peerSocket);
                cout << "getting of for chunk " << chunk_no << endl;
                return false;
            }

            // cout << "for chunk_no " << chunk_no << " calsha=" << calculatedSha << " resha" << receivedChunkSha << endl;

            // Update client map
            if (chunk_no >= clientFileMetadata[filesha].second.size())
            {
                printMessage("Can't store info for this chunk in client map: " + to_string(chunk_no));
            }
            else
            {
                clientFileMetadata[filesha].second[chunk_no] = calculatedSha;
                if (D[filesha].second == 0)
                {
                    // send tracker info that u have downloaded this file successfully
                    string request = "upload_file " + group_id + " " + "abc" + " " + fs + " " + filesha;
                    send(clientSocket, request.c_str(), request.size(), 0);

                    printMessage("File downloaded successfully to " + destination_path);
                }
                if (D[filesha].second + 1 == total_chunks)
                {
                    C[filesha].second = D[filesha].second + 1;
                     C[filesha].first = D[filesha].first;
                    D.erase(filesha);
                }
                else
                {

                    D[filesha].second++;
                }
            }
            // cout << "getting of for chunk " << chunk_no << endl;
            close(peerSocket);
            return true;
        }
        else
        {
            printMessage("Error: Chunk integrity check failed for chunk " + to_string(chunk_no));
            printMessage("calcusha=" + calculatedSha + " receivedsha=" + receivedChunkSha);
        }
    }
    else
    {
        printMessage("Error: Malformed chunk data received.");
    }
    // cout << "getting of for chunk " << chunk_no << endl;
    close(peerSocket);
    return false;
}

void downloadChunksThread(int clientSocket, const string &group_id, const string &file_sha, const string &destination_path)
{

    // cout << "inside download chunk thread " << endl;
    // Send request to tracker for peer information
    string request = "download_file " + group_id + " " + file_sha;
    send(clientSocket, request.c_str(), request.size(), 0);

    // Receive peer information from tracker

    char buffer[4096] = {0};
    recv(clientSocket, buffer, sizeof(buffer), 0);
    string peerInfo(buffer);
    printMessage("Response from tracker: " + peerInfo);

    if (peerInfo == "File not found")
    {
        printMessage("Error: File not found in group.");
        D.erase(file_sha);
        return;
    }

    // Parse peer information
    istringstream peerStream(peerInfo);
    string peer_ip;
    int peer_port;
    vector<pair<string, int>> peers;
    // printMessage("Parsing peer info.");
    string filesize;
    peerStream >> filesize;
    // cout << "got filesize in client side is" << filesize << endl;
    // FILESIZE = stoi(filesize);
    while (peerStream >> peer_ip >> peer_port)
    {
        // printMessage("Peer IP: " + peer_ip + " Peer Port: " + to_string(peer_port));
        if (peer_ip.empty() || peer_port <= 0)
        {
            printMessage("Invalid peer info.");
            continue;
        }

        peers.emplace_back(peer_ip, peer_port);
    }

    // Fetch chunk information from peers
    map<int, set<string>> chunkToPeers; // chunk_sha -> {peers}
    vector<thread> threads;
    for (const auto &[peer_ip1, peer_port1] : peers)
    {
        // printMessage("Peer IP: " + peer_ip1 + " Peer Port: " + to_string(peer_port1));
        if (fetchChunkInfo(peer_ip1, peer_port1, chunkToPeers, file_sha) == false)
        {
            printMessage("Download failed due to inability to fetch chunk info.");
            D.erase(file_sha);
            return;
        }
    }
    // this file have total these many chunks
    int total_chunks = 0;
    total_chunks = chunkToPeers.size();
    // cout << "totalchunk is " << total_chunks << endl;
    // assume first all the chunks will be downloaded
    clientFileMetadata[file_sha].first = destination_path;
    for (int i = 0; i < total_chunks; i++)
    {
        clientFileMetadata[file_sha].second.push_back("");
    }

    // Apply rarest-first piece selection
    vector<int> chunks;
    for (const auto &[chunk_no, peerSet] : chunkToPeers)
    {
        chunks.push_back(chunk_no);
    }
    sort(chunks.begin(), chunks.end(), [&chunkToPeers](const int &a, const int &b)
         { return chunkToPeers[a].size() < chunkToPeers[b].size(); }); // Rarest first

    if (!ensureFileExists(destination_path))
    {
        D.erase(file_sha);
        return;
    }

    // cout << "feched file info" << endl;

    // Open the file in binary mode for updating ony once

    // Download chunks using thread pooling
    vector<thread> downloadThreads;

    // Thread Pool for downloading chunks
    ThreadPool threadPool(4); // Initialize with 4 worker threads (adjust as needed)
    // cout << "before threead pool";
    // Add download tasks to the thread pool
    for (const int &chunk_no : chunks)
    {
        threadPool.enqueue([&, chunk_no]()
                           {
    try {
        for (const string &peer_info : chunkToPeers[chunk_no]) {
            if (downloadChunk(clientSocket, chunk_no, peer_info, destination_path, file_sha, group_id, filesize, total_chunks)) {
                break;
            }
        }
    } catch (const std::exception &e) {
        std::cerr << "Exception in thread: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception in thread." << std::endl;
    } });
    }

    // Wait for all tasks to finish by relying on ThreadPool destructor (automatically waits)
    std::this_thread::sleep_for(std::chrono::seconds(2)); // Optional wait to allow completion

    // if i haven't store this file sha in map it means i haven't downloaded this file
    if (clientFileMetadata.find(file_sha) == clientFileMetadata.end())
        D.erase(file_sha);

    std::fstream outFile(destination_path, std::ios::binary | std::ios::in | std::ios::out);
    if (!outFile)
    {
        printMessage("Error: Unable to open file for writing.");
        return;
    }

    // closed the file at last only after completion for all threads
    outFile.close();
}

void download_file(int clientSocket, const string &group_id, const string &file_sha, const string &destination_path)
{

    // cout<<"in download_file function"<<endl;

    // push filesha to downloading queue
    D[file_sha] = {group_id, 0};
    // Launch a new thread
    std::thread downloadThread(downloadChunksThread, clientSocket, group_id, file_sha, destination_path); // run thread independantly
    //  cout<<"below downloadThread"<<endl;
    // cout <<  << destination_path << endl;
    downloadThread.detach();
}

void stopshare(int clientSocket, string &group_id, string &file_sha)
{

    // Send request to tracker for peer information
    string request = "stop_share " + group_id + " " + file_sha;
    send(clientSocket, request.c_str(), request.size(), 0);

    // Receive response from tracker

    char buffer[4096] = {0};
    recv(clientSocket, buffer, sizeof(buffer), 0);
    string response(buffer);
    printMessage("Response from tracker: " + response);
    // delete this entry from client side
    if (response == "stopped sharing file")
    {
        clientFileMetadata.erase(file_sha);
    }
}

void showdownloads(int clientSocket)
{

    std::cout << "Show downloads:" << std::endl;

    // Display downloading files
    for (const auto &entry : D)
    {
        const std::string &fileSha = entry.first;
        const std::string &groupId = entry.second.first;
        const int chunksDownloaded = entry.second.second;

        std::cout << "[D] [" << groupId << "] " << fileSha
                  << " (Chunks downloaded: " << chunksDownloaded << ")" << std::endl;
    }

    // Display completed files
    for (const auto &entry : C)
    {
        const std::string &fileSha = entry.first;
        const std::string &groupId = entry.second.first;
        const int chunksDownloaded = entry.second.second;

        std::cout << "[C] [" << groupId << "] " << fileSha
                  << " (Chunks downloaded: " << chunksDownloaded << ")" << std::endl;
    }
}