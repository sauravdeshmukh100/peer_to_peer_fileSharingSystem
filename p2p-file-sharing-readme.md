# Peer-to-Peer Distributed File Sharing System

This project implements a basic peer-to-peer distributed file sharing system with group-based access control. It consists of a tracker server and a client application.

## Prerequisites

- C++ compiler (supporting C++11 or later)
- OpenSSL library

## Compilation

### Tracker

To compile the tracker:

```bash
g++ tracker.cpp -o tracker
```

### Client

To compile the client:

```bash
g++ client.cpp -o client -lssl -lcrypto
```

Note: The `-lssl -lcrypto` flags are required to link against the OpenSSL library.

## Running the Application

### Tracker

To run the tracker:

```bash
./tracker tracker_info.txt tracker_no
```

Where:
- `tracker_info.txt` is a file containing IP and port information for all trackers
- `tracker_no` is the number of the tracker to run (0-based index into the tracker info file)

### Client

To run the client:

```bash
./client <IP>:<PORT> tracker_info.txt
```

Where:
- `<IP>:<PORT>` is the IP address and port number of the client
- `tracker_info.txt` is the same file used for the tracker, containing tracker information

## Usage

Once the client is running, you can use the following commands:

1. Create User Account: `create_user <user_id> <passwd>`
2. Login: `login <user_id> <passwd>`
3. Create Group: `create_group <group_id>`
4. Join Group: `join_group <group_id>`
5. Leave Group: `leave_group <group_id>`
6. List Pending Join Requests: `list_requests <group_id>`
7. Accept Group Joining Request: `accept_request <group_id> <user_id>`
8. List All Groups in Network: `list_groups`
9. List All Sharable Files in Group: `list_files <group_id>`
10. Upload File: `upload_file <file_path> <group_id>`
11. Logout: `logout`

## Notes

- At least one tracker must be running for the system to work.
- Users must create an account and log in before performing other operations.
- File sharing is limited to metadata exchange; actual file transfer is not implemented.
- The system uses a basic authentication mechanism; passwords are not encrypted in transit.

## Limitations

- Actual file transfer between peers is not implemented.
- The system does not support multiple trackers (fallback mechanism).
- There's no data encryption for communication between clients and trackers.

