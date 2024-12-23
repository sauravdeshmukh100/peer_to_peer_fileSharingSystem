#include <string>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <cstring>
#include <sstream>
#include <iostream> // For std::cout, std::cerr
#include <fstream>  // For std::ofstream
#include<set>
#include <sys/stat.h> // For struct stat and mkdir
#include <unordered_map>

// #include <openssl/evp.h> // For EVP functions
#include <openssl/sha.h>
#include <fcntl.h> // For open()
                   // For read(), close()

#include <algorithm> // Include for  find


struct Group
{
    string owner;                                            // Owner of the group
    vector<string> members;                                  // Members in the group
    vector<string> pendingRequests;                          // Pending join requests
    // map<string, pair<string, vector<string>>> sharableFiles; // map of file_name -> SHA1 hash
    map<string, vector<string>> file_owner;                  // userid to fileShas              // map from owner to file path
    map<string, pair<string, vector<string>>> fileSha_to_peers; // hash -> {name, vector<peers>}
};

// Function to handle client requests
extern map<string, pair<string, bool>> users; // user_id -> hashed password
extern map<string, Group> groups;             // group_id -> Group

// Helper to generate SHA1 hash of fil

void handleClient(int clientSocket)
{

    string client_ip, client_port;

    bool login = false;

    string curr_client;
    while (true)
    {
        char buffer[1024] = {0};
        int b = recv(clientSocket, buffer, sizeof(buffer), 0);
        // cout<<"b is"<<b<<endl;

        if (b <= 0)
        {
            cout << "didn't recirebvr anything from client " << endl;
            users[curr_client].second = false;
            // Delete file associated with the user
            // string directory = "client_ip-ports";
            // string filePath = directory + "/" + curr_client;
            // if (remove(filePath.c_str()) != 0)
            // {
            //     cerr << "Error deleting file for user: " << curr_client << endl;
            // }

            curr_client = "";
            cout << "logging out from user " << curr_client << "\n";
            close(clientSocket);
            return;
        }

        string request(buffer);
        istringstream iss(request);
        string command;

        iss >> command;
        cout << "command is " << command << endl;
        // cout << "command is " << command << endl;

        if (command == "exit")
        {

            cout << "connection closed from client\n";
            users[curr_client].second = false;
            // Delete file associated with the user
            // string directory = "client_ip-ports";
            // string filePath = directory + "/" + curr_client;
            // if (remove(filePath.c_str()) != 0)
            // {
            //     cerr << "Error deleting file for user: " << curr_client << endl;
            // }
            curr_client = "";
            cout << "logging out from user " << curr_client << "\n";
            close(clientSocket);
            return;
        }
        if (command == "create_user")
        {

            // this is need not be curr user
            string user_id, password;
            iss >> user_id >> password;
            if (users.find(user_id) == users.end())
            {
                users[user_id].first = password;
                users[user_id].second = false;
                string response = "User created successfully.";
                send(clientSocket, response.c_str(), response.size(), 0);
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

                
                // Create directory if it does not exist
                // string directory = "client_ip-ports";
                // struct stat st = {0};
                // if (stat(directory.c_str(), &st) == -1)
                // {
                //     mkdir(directory.c_str(), 0700);
                // }

                // Create file with user_id as name and store IP and port
                // string filePath = directory + "/" + curr_client;
                // ofstream outFile(filePath);
                // if (outFile.is_open())
                // {
                //     outFile << client_ip << " " << client_port;
                //     outFile.close();
                // }
                // else
                // {
                //     cerr << "Error creating file for user: " << curr_client << endl;
                // }

                string response = "Login successful.";
                send(clientSocket, response.c_str(), response.size(), 0);
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
                string response = "Group created successfully.";
                send(clientSocket, response.c_str(), response.size(), 0);
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
                string response = "Join request sent.";
                send(clientSocket, response.c_str(), response.size(), 0);
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
                        vector<string> & peers =  groups[group_id].fileSha_to_peers[sha].second;

                        auto it = find(peers.begin(), peers.end(), peer);
                        // int peer_index = it - peers.begin();
                        peers.erase(it);

                        // deleting the entry of this file if this is only own by current user
                        if(groups[group_id].fileSha_to_peers[sha].second.size()==0)
                        {
                            groups[group_id].fileSha_to_peers.erase(sha);
                        }
                    }
                 
                 
                 // deleting current user from file map
                 groups[group_id].file_owner.erase(curr_client);


                 


                    // vector<string> filenames = groups[group_id].file_owner[user_id];
                    // groups[group_id].file_owner.erase(user_id); // delting the this from upload file

                    // // deleting all the files uploaded by current user
                    // for (auto &name : filenames)
                    // {
                    //     if (groups[group_id].sharableFiles.find(name) != groups[group_id].sharableFiles.end())
                    //     {
                    //         cout << "deleting file " << name << endl;
                    //         groups[group_id].sharableFiles.erase(name);
                    //     }
                    // }

                    if (members.size() == 1)
                    {
                        cout << "deleting group " << group_id << endl;

                        groups.erase(group_id);
                        string response = "Left group successfully.";
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
                        string response = "User added to group.";
                        send(clientSocket, response.c_str(), response.size(), 0);
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

            // Delete file associated with the user
            // string directory = "client_ip-ports";
            // string filePath = directory + "/" + curr_client;
            // if (remove(filePath.c_str()) != 0)
            // {
            //     cerr << "Error deleting file for user: " << curr_client << endl;
            // }

            curr_client = "";
            // cout << " curr client size is " << curr_client.size() << endl;
            string response = "logout Successfull";
            send(clientSocket, response.c_str(), response.length(), 0);
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
                string response = "Sharable files and their hashes:\n";
                for (const auto &entry : groups[group_id].file_owner)
                {
                    if (users[entry.first].second == false)
                        continue;

                    vector<string> filehashes = entry.second; // Get the filename
                    // const auto & chunkHashes = entry.second; // Get the pair (file hash, chunk hashes)

                    // response +=  ;
                    unordered_map<string,string> st;
                    for (auto &hash : filehashes)
                    {
                        string filename=groups[group_id].fileSha_to_peers[hash].first;
                        st.insert({hash,filename}); //used set to remove duplicates
                    }
                   // iterate through set
                   for(auto & it :st)
                   {
                    response+=it.first + " " + it.second +"\n";
                   }

                    // response += "Chunk Hashes:\n";
                    // for (const auto &chunkHash : chunkHashes)
                    // {
                    //     response += chunkHash + "\n"; // Improved formatting
                    // }
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
            iss >> group_id >> fileName >>filesize>>fileHash;
            cout << "group_id is " << group_id << " filename is " << fileName << "filesize is"<<filesize<< endl;
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
                    
                    vector<string> & v =groups[group_id].fileSha_to_peers[fileHash].second;
                    if(find(v.begin(),v.end(),temp)!=v.end())
                    {
                        string response ="u have already uploded or dowmloaded this file";
                        send(clientSocket, response.c_str(), response.size(), 0);

                        continue;

                    }
                    string response = "This file  is  uploaded by u also";
                    groups[group_id].fileSha_to_peers[fileHash].second.push_back(temp);
                    groups[group_id].file_owner[curr_client].push_back(fileHash);
                    send(clientSocket, response.c_str(), response.size(), 0);
                    continue;
                }

                groups[group_id].fileSha_to_peers[fileHash].first=fileName + " " + filesize;
                groups[group_id].fileSha_to_peers[fileHash].second.push_back(temp) ;

                // groups[group_id].file_owner[curr_client].push_back(fileName);
                groups[group_id].file_owner[curr_client].push_back(fileHash);
                // Send success response to client
                string response = "File uploaded  successfully.";
                send(clientSocket, response.c_str(), response.size(), 0);
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
          istringstream name_size (groups[group_id].fileSha_to_peers[file_sha].first);
          string name,filesize;
          name_size>>name>>filesize;
            // Collect peers sharing the file
            string response=  filesize + " " ;
           //  sending reponse as  ip an ]d port of each peer having content of this file 
            for (const string & temp : groups[group_id].fileSha_to_peers[file_sha].second)
            {
                 istringstream s (temp);
                 string tu,ti,tp;
                 s>>tu>>ti>>tp;

                 response += ti + " " + tp + " \n"; // storing only ip and port not userid
            }
            cout<<"response is  \n"<<response<<endl;
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
           vector<string>  & temp=groups[group_id].file_owner[curr_client];
           auto it=find(temp.begin(), temp.end(),file_sha);
            if( it== temp.end())
            {
                string response = "u dont have this file";
            send(clientSocket, response.c_str(), response.size(), 0);
            continue;
            }

          
          
         temp.erase(it);
            string response ="stopped sharing file" ;
            send(clientSocket, response.c_str(), response.size(), 0);
        }
    }
}
