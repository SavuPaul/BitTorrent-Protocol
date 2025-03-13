#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100
#define MAX_CLIENTS 10

typedef struct CFile {
    char *file_name;
    int no_of_segments;         // number of owned segments
    char **file_segments;       // actual file segments
} CFile;

typedef struct CFileWanted {
    char *file_name;
    int no_of_segments;
    char **downloaded_segments;
    int *owned_segments;        // idx array for owned_segments after
                                // download from another client
    int no_of_swarm_clients;
    int *swarm_clients;
    int owned;
} CFileWanted;

typedef struct OutputFile {
    char filename[MAX_FILENAME + 1];
    FILE *file;
} OutputFile;

typedef struct Client {
    int no_of_owned_files;
    CFile *files;
    int no_of_wanted_files;
    CFileWanted *wanted_files;
    int consecutive_segments;
    OutputFile *outputs;
    int last_swarm_idx;
} Client;

typedef struct TFile {
    char *file_name;
    int uploaded_segments;
    char **segments;
    int swarm_size;
    int *swarm;         // int array containing client id's
                        // (a client id is equal to their index
                        // inside the clients array)
} TFile;

typedef struct Tracker {
    TFile *files;
    int total_files;
    int *clients;
} Tracker;

// Creates a client from the file data
Client* init_client_from_file(char *filename, int rank);

// Initializes all clients from their files
Client* init_all_clients(int *no_of_clients, int rank);

// Initializes the tracker
Tracker* init_tracker(int no_of_clients);

// Creates an array of output files to write to
void create_output_files(Client *clients, int rank);

// Frees memory for a client
void free_client();

// Frees memory for the tracker
void free_tracker();

// Checks if tracker already contains the given file name
int tracker_contains_filename(Tracker *tracker, char *name);

// Checks if tracker already contains the given segment for the given file
int tracker_contains_segment_for_file(Tracker *tracker, char *segment, char *file);

// Adds a client to the swarm of a file
void add_client_to_swarm_file(Tracker *tracker, int client, char *filename);

// Adds segment information to the file on the tracker
void add_segment_to_tracker_file(Tracker *tracker, char *segment, char *name);

// Clients send their initial file information to the tracker
void send_file_info_to_tracker(Client *clients, int rank);

// Tracker receives the clients' file information
void receive_file_info(Tracker *tracker, int no_of_clients);

// Receives the requests for the files that the clients want
void receive_and_deal_requests(Tracker *tracker, Client *clients, int client);

// Sends the swarm information to the client i
void send_swarm_info(Tracker *tracker, char *filename, int rank, int tag);

// Checks if a client has a given segment in a given file
int check_if_segment_is_owned(Client *clients, int client, char *segment, char *filename);

// Checks what segments are owned already and marks them
void mark_owned_segments(Client *clients, int client, int idx);

// Downloads segments from various clients mentioned in swarm
void download_segments(Tracker *tracker, Client *clients, int rank, char *filename, char *segment, int sgm_idx);

// Checks if a client has finished downloading all wanted files
int check_downloads_client(Client *clients, int rank);

// Checks if all clients have finished downloading
int all_clients_finished(Tracker *tracker, int no_of_clients);

// DEBUGGING (prints all client details)
void print_clients(Client *clients, int no_of_clients);

// DEBUGGING (prints all tracker details)
void print_tracker(Tracker *tracker);
