
#include <fcntl.h>       // For open()
#include <unistd.h>      // For read(), close()
#include <sys/socket.h>
#include <arpa/inet.h>   // For inet_pton()
#include <netinet/in.h>
#include <iostream>
#include <cstring>
#include <vector>
#include<string>
using namespace std;
// Structure to hold tracker info
struct TrackerInfo {
       string ip;
    int port;
};

// Helper function to parse IP and port from a string
bool parseLine(const    string& line,    string& ip, int& port) {
    size_t spacePos = line.find(' ');
    if (spacePos ==    string::npos) {
        return false;  // Invalid line
    }

    ip = line.substr(0, spacePos);             // Extract IP
    port =    stoi(line.substr(spacePos + 1));  // Extract port number
    return true;
}









   vector<TrackerInfo> readTrackerInfo( const string & filename) {


   
       vector<TrackerInfo> trackers;
    int fd = open(filename.c_str(), O_RDONLY);  // Open the file using system call

    if (fd < 0) {
           cerr << "Error opening tracker info file: " << filename <<    endl;
        return trackers;
    }

    // Buffer to read file data in chunks
    const int BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
       string fileContent;  // String to store file content
    ssize_t bytesRead;

    // Read file content in chunks
    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
        fileContent.append(buffer, bytesRead);  // Append read data to the string
    }

    if (bytesRead < 0) {
           cerr << "Error reading tracker info file" <<    endl;
        close(fd);  // Close file before returning
        return trackers;
    }

    close(fd);  // Close the file after reading

    // Parse the file content line by line
    size_t pos = 0;
       string line;
    while ((pos = fileContent.find('\n')) !=    string::npos) {
        line = fileContent.substr(0, pos);  // Extract line
        fileContent.erase(0, pos + 1);      // Remove processed line from content

           string ip;
        int port;
        if (parseLine(line, ip, port)) {
            trackers.push_back({ip, port});  // Add tracker info to the list
        }
    }

    return trackers;
}