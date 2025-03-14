# README - BitTorrent Protocol

---

## DESCRIPTION OF ENTITIES

### CLIENT ENTITY (including file structures)
In order to implement the functionalities of this protocol, I used an array of type Client, where each client was introduced. The index of each client within this array is equal to their rank. The clients are instances of the structure `Client`, and they all have fields containing the owned files, the wanted files, the output files, as well as details regarding the number of consecutive segments that they downloaded (max. 10 in a row) and the last source they downloaded the segments from.

The `files` field is an array of type `CFile` (Client file), containing the file name and the segments. The `wanted_files` field is an array of type `CWantedFile` (Client wanted file), containing the file name, the segments downloaded from the tracker in order for the client to check what needs to be downloaded, the swarm clients from which the file can be downloaded and an array of integers `owned_segments` correlated to the field `downloaded_segments` so that if `downloaded_segments[index]` is owned, then `owned_segments[index] = 1`, otherwise `owned_segments[index] = 0`.

### TRACKER ENTITY
The tracker is a unique entity who gathers all the information about the files/segments without knowing what segment is owned by who.

---

## FLOW OF THE PROGRAM

Clients start by sending the data to the tracker by calling the function `send_file_info_to_tracker()`. This sends the number of owned files, followed by the segments of each owned file by every client, to the tracker, so that the tracker receives all the information regarding the total segments of each file, as well as what file is owned by who (the swarm of a file). The tracker receiving is done through `receive_file_info()`, where each file is "reconstructed," by keeping track of the fact that each segment has to be unique for every file and that the swarm of a file has to contain the same client only once. For instance: Client1 and Client2 both own file1, so when receiving the segments for file1 by the tracker, the segments from both clients must be added only once.

Up to this point, the tracker has an evidence of the overall file "database." Clients are not allowed to proceed until the tracker finishes storing all the required information. This is done by making the clients wait until the tracker sends an "ACK" message to them, enabling them to start the communication.

Upon receiving the confirmation message, each client creates two threads: one for downloading and one for uploading. Every client's upload thread waits for requests from other client's download thread. The first step for every client is to ask the tracker for all the segments of every wanted file, so that the client knows what segments to ask for in the future. In order to receive and send the information back, the function `receive_and_deal_requests()` is executed by the tracker where it searches the client's wanted file through its collection of files and sends back all the segments of the file. The client receives these segments and loads them into the `downloaded_segments` array field. I also implemented a function called `mark_owned_segments()` which is meant to be used in the corner-case where the clients own fractions of a file prior to exchanging information with the tracker or other clients, thus not downloading an already owned segment.

The actual communication process between clients starts when each client's wanted files segments are requested to download from other clients. However, the client does not know who to send requests to yet, so a "SWARM" request has to be sent to the tracker. When the tracker receives a "SWARM" message for a certain file, it sends back the swarm of that file. Now that the client received the swarm, it can start downloading segments by sending requests to the upload threads of other clients. This means that every client's upload thread can receive messages from any other client, without knowing who exactly is sending the request, so therefore `MPI_ANY_SOURCE` has to be used upon receiving requests in the upload thread. If the segment is owned, then an "ACK" message is sent back, otherwise a "SNO" (Segment not owned) message will be sent. If a client successfully downloads a segment, the `consecutive_segments` for that client is incremented. When the value reaches 10, the downloading is interrupted so that an exchange for the swarm of the file with the tracker takes place again. The downloading requests are sent to various clients, so that no client is ever overloaded with too many messages. _(Uncomment line 405 in helpers.c to show this.)_ This is done through the client's `last_swarm_idx` field which is changed after each segment download. When an "ACK" is received for a segment, the segment is marked as owned and then `last_swarm_idx` takes the value of the index corresponding to the client in the swarm file who sent the "ACK".

**Example:**
Swarm file contains clients 1, 4, 5. The first segment is downloaded from 1. The next segment will be downloaded from index 1 (client 4), the next one from index 2 (client 5) and then the next one will be downloaded from index 0 (client 1) and so on. If a client does not have a segment, the iteration continues to the next one.

After downloading all segments, the client sends a "CLOSE" message to the tracker and then the client's download thread can close. If all clients finished, an "EXIT" message from the tracker is sent to all clients' upload threads so that these threads can close as well, followed by the tracker closing.

