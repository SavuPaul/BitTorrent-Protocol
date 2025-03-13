#include "tema2.h"

#define MAX_NO_OF_CLIENTS 10
#define MAX_INPUT_FILE_LENGTH 10
#define MAX_SEGMENTS 500

Client* init_client_from_file(char *filename, int rank) {
    Client *client = malloc(sizeof(Client));

    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("Eroare la deschiderea fisierului %s\n", filename);
        return NULL;
    }

    // Reads the number of owned files for the client
    fscanf(f, "%d", &client->no_of_owned_files);

    // Allocate memory for the files
    client->files = malloc((client->no_of_owned_files) * sizeof(CFile));

    for (int i = 0; i < client->no_of_owned_files; i++) {
        // Allocates memory for the file name
        client->files[i].file_name = malloc(MAX_FILENAME * sizeof(char));

        // Reads the file name and its number of segments
        fscanf(f, "%s", client->files[i].file_name);
        fscanf(f, "%d", &client->files[i].no_of_segments);

        // Allocates memory for file segments
        client->files[i].file_segments = malloc(client->files[i].no_of_segments * sizeof(char *));
        for (int j = 0; j < client->files[i].no_of_segments; j++) {
            client->files[i].file_segments[j] = malloc((HASH_SIZE + 1) * sizeof(char));

            fscanf(f, "%s", client->files[i].file_segments[j]);
        }
    }

    // Reads the number of wanted files for the client
    fscanf(f, "%d", &client->no_of_wanted_files);

    // Allocates memory for the names of the wanted files
    client->wanted_files = malloc(client->no_of_wanted_files * sizeof(CFileWanted));

    for (int i = 0; i < client->no_of_wanted_files; i++) {
        client->wanted_files[i].file_name = malloc(MAX_FILENAME * sizeof(char));
        fscanf(f, "%s", client->wanted_files[i].file_name);

        client->wanted_files[i].owned_segments = malloc(MAX_SEGMENTS * sizeof(int));

        client->wanted_files[i].downloaded_segments = malloc(MAX_SEGMENTS * sizeof(char *));
        for (int j = 0; j < MAX_SEGMENTS; j++) {
            client->wanted_files[i].downloaded_segments[j] = malloc((HASH_SIZE + 1) * sizeof(char));
        }

        client->wanted_files[i].swarm_clients = malloc(10 * sizeof(int));
        client->wanted_files[i].owned = 0;
    }

    client->consecutive_segments = 0;

    fclose(f);

    return client;
}

Client* init_all_clients(int *no_of_clients, int rank) {
    Client *clients = malloc(MAX_CLIENTS * sizeof(Client));

    for (int i = 1; i < MAX_NO_OF_CLIENTS; i++) {
        char filename[MAX_INPUT_FILE_LENGTH];
        snprintf(filename, MAX_INPUT_FILE_LENGTH, "in%d.txt", i);

        Client* client = init_client_from_file(filename, rank);
        
        // If client is NULL it means the input text files do not exist from this point on 
        if (client == NULL) {
            break;
        }

        clients[i] = *client;
        (*no_of_clients)++;
    }

    return clients;
}

Tracker* init_tracker(int no_of_clients) {
    Tracker* tracker = malloc(sizeof(Tracker));

    tracker->total_files = 0;

    // Allocate memory for the tracker files, using maximum sizes
    tracker->files = malloc(MAX_FILES * sizeof(TFile));
    
    // Allocate memory for the tracker's file's fields
    for (int i = 0; i < MAX_FILES; i++) {
        tracker->files[i].file_name = malloc(MAX_FILENAME * sizeof(char));
        tracker->files[i].swarm = malloc(MAX_FILES * sizeof(int));
        tracker->files[i].segments = malloc(MAX_SEGMENTS * sizeof(char *));
        tracker->files[i].uploaded_segments = 0;
        tracker->files[i].swarm_size = 0;
    }

    for (int i = 0; i < MAX_FILES; i++) {
        for (int j = 0; j < MAX_SEGMENTS; j++) {
            tracker->files[i].segments[j] = malloc((HASH_SIZE + 1) * sizeof(char));
        }
    }

    tracker->clients = malloc(MAX_CLIENTS * sizeof(int));
    for (int i = 1; i <= no_of_clients; i++) {
        tracker->clients[i] = 0;
    }

    return tracker;
}

void create_output_files(Client *clients, int rank) {
    clients[rank].outputs = malloc(clients[rank].no_of_wanted_files * sizeof(OutputFile));
    for (int i = 0; i < clients[rank].no_of_wanted_files; i++) {
        snprintf(clients[rank].outputs[i].filename, MAX_FILENAME + 1, "client%d_%s", rank, clients[rank].wanted_files[i].file_name);

        clients[rank].outputs[i].file = fopen(clients[rank].outputs[i].filename, "w");
        if (clients[rank].outputs[i].file == NULL) {
            perror("Error opening output file");
            exit(-1);
        }
    }
}

int tracker_contains_filename(Tracker *tracker, char *name) {
    if (tracker->total_files == 0) {
        return 0;
    }

    for (int i = 0; i < tracker->total_files; i++) {
        if (strcmp(tracker->files[i].file_name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

int tracker_contains_segment_for_file(Tracker *tracker, char *segment, char *file) {
    for (int i = 0; i < tracker->total_files; i++) {
        if (strcmp(tracker->files[i].file_name, file) == 0) {
            // If there are no existing segments, return 0
            if (tracker->files[i].uploaded_segments == 0) {
                return 0;
            } else {
                for (int j = 0; j < tracker->files[i].uploaded_segments; j++) {
                    if (strcmp(tracker->files[i].segments[j], segment) == 0) {
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

void add_client_to_swarm_file(Tracker *tracker, int client, char *filename) {
    for (int i = 0; i < tracker->total_files; i++) {
        if (strcmp(tracker->files[i].file_name, filename) == 0) {
            tracker->files[i].swarm[tracker->files[i].swarm_size] = client;
            tracker->files[i].swarm_size++;
            break;
        }
    }
}

void add_segment_to_tracker_file(Tracker *tracker, char *segment, char *name) {
    for (int i = 0; i < tracker->total_files; i++) {
        if (strcmp(tracker->files[i].file_name, name) == 0) {
            if (tracker->files[i].uploaded_segments == 0) {
                tracker->files[i].segments[tracker->files[i].uploaded_segments] = segment;
                tracker->files[i].uploaded_segments++;
            } else {
                int found = 0;
                for (int j = 0; j < tracker->files[i].uploaded_segments; j++) {
                    if (strcmp(tracker->files[i].segments[j], segment) == 0) {
                        found = 1;
                    }
                }
                if (found == 0) {
                    tracker->files[i].segments[tracker->files[i].uploaded_segments] = segment;
                    tracker->files[i].uploaded_segments++;
                } else {
                    return;
                }
            }
        }
    }
}

void send_file_info_to_tracker(Client *clients, int rank) {
    // Send the number of owned files
    int no_of_owned_files = clients[rank].no_of_owned_files;
    MPI_Send(&no_of_owned_files, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);

    CFile *files = clients[rank].files;

    for (int i = 0; i < no_of_owned_files; i++) {
        // Send length of the file_name
        int name_length = strlen(files[i].file_name) + 1;
        MPI_Send(&name_length, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);

        // Send the name of the file
        MPI_Send(files[i].file_name, name_length, MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);

        // Send the number of segments
        MPI_Send(&files[i].no_of_segments, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);

        // Send the segments
        for (int j = 0; j < files[i].no_of_segments; j++) {
            MPI_Send(files[i].file_segments[j], HASH_SIZE + 1, MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);
        }
    }
}

void receive_file_info(Tracker *tracker, int no_of_clients) {
    for (int i = 1; i <= no_of_clients; i++) {
        int no_of_owned_files;

        // Number of owned files from client i
        MPI_Status status;
        MPI_Recv(&no_of_owned_files, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);

        for (int j = 0; j < no_of_owned_files; j++) {
            // Receive filename length
            int filename_length;
            MPI_Recv(&filename_length, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);

            // Receive the name of the file
            char *name = malloc(MAX_FILENAME * sizeof(char));
            MPI_Recv(name, filename_length, MPI_CHAR, i, 0, MPI_COMM_WORLD, &status);

            // Only add file name if tracker does not contain it
            if (tracker_contains_filename(tracker, name) == 0) {
                tracker->files[tracker->total_files].file_name = name;
                tracker->total_files++;
            }

            // Add the client to the swarm of the current file
            add_client_to_swarm_file(tracker, i, name);

            // Receive the number of segments for this file
            int no_of_segments;
            MPI_Recv(&no_of_segments, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);

            for (int k = 0; k < no_of_segments; k++) {
                char *segment = malloc((HASH_SIZE + 1) * sizeof(char));

                // Receive a segment one at a time and verify if it exists already for the given file
                MPI_Recv(segment, HASH_SIZE + 1, MPI_CHAR, i, 0, MPI_COMM_WORLD, &status);

                add_segment_to_tracker_file(tracker, segment, name);
            }
        }
    }
}

void receive_and_deal_requests(Tracker *tracker, Client *clients, int rank) {
    MPI_Status status;

    for (int i = 0; i < clients[rank].no_of_wanted_files; i++) {
        int filename_length = 0;
        MPI_Recv(&filename_length, 1, MPI_INT, rank, 0, MPI_COMM_WORLD, &status);

        // Get the name of the file that the client wants
        char filename[filename_length];
        MPI_Recv(filename, filename_length, MPI_CHAR, rank, 0, MPI_COMM_WORLD, &status);
            
        // The tracker now knows that the client <i> wants file <filename>.
        // The tracker should send the client the list of segments of the file.

        // Send the segments of the file
        for (int j = 0; j < tracker->total_files; j++) {
            if (strcmp(tracker->files[j].file_name, filename) == 0) {
                // Send the number of file segments which will be sent
                MPI_Send(&tracker->files[j].uploaded_segments, 1, MPI_INT, rank, 0, MPI_COMM_WORLD);
                
                // Send the file segments to the client
                for (int k = 0; k < tracker->files[j].uploaded_segments; k++) {
                    MPI_Send(tracker->files[j].segments[k], HASH_SIZE + 1, MPI_CHAR, rank, 0, MPI_COMM_WORLD);
                }
            }
        }
    }
}

void send_swarm_info(Tracker *tracker, char *filename, int rank, int tag) {
    for (int i = 0; i < tracker->total_files; i++) {
        if (strcmp(tracker->files[i].file_name, filename) == 0) {
            // Send length of swarm file
            int length = tracker->files[i].swarm_size;
            MPI_Send(&length, 1, MPI_INT, rank, tag, MPI_COMM_WORLD);

            // Send the swarm clients
            for (int j = 0; j < length; j++) {
                MPI_Send(&tracker->files[i].swarm[j], 1, MPI_INT, rank, tag, MPI_COMM_WORLD);
            }

            // Once the swarm file has been sent, add the client at index <rank> in the swarm file
            // if the swarm file does not already contain that client
            int found = 0;
            for (int j = 0; j < length; j++) {
                if (tracker->files[i].swarm[j] == rank) {
                    found = 1;
                }
            }

            if (found == 0) {
                tracker->files[i].swarm[length] = rank;
                tracker->files[i].swarm_size++;
            }
        }
    }
}

int check_if_segment_is_owned(Client *clients, int rank, char *segment, char *filename) {
    for (int i = 0; i < clients[rank].no_of_owned_files; i++) {
        if (strcmp(clients[rank].files[i].file_name, filename) == 0) {
            for (int j = 0; j < clients[rank].files[i].no_of_segments; j++) {
                if (strcmp(segment, clients[rank].files[i].file_segments[j]) == 0) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

// A client can own only a part of a file, so already-owned segments need to be marked
void mark_owned_segments(Client *clients, int client, int idx) {
    for (int i = 0; i < clients[client].wanted_files[idx].no_of_segments; i++) {
        char *crt_segment = clients[client].wanted_files[idx].downloaded_segments[i];
        int owned = check_if_segment_is_owned(clients, client, crt_segment, clients[client].wanted_files[idx].file_name);
        if (owned == 1) {
            clients[client].wanted_files[idx].owned_segments[i] = 1;
        } else {
            clients[client].wanted_files[idx].owned_segments[i] = 0;
        }
    }
}

CFileWanted find_file_for_client(Client *clients, int rank, char *filename) {
    for (int i = 0; i < clients[rank].no_of_wanted_files; i++) {
        if (strcmp(clients[rank].wanted_files[i].file_name, filename) == 0) {
            return clients[rank].wanted_files[i];
        }
    }
    printf("Error at find_file_for_client. File should've been found.\n");
    exit(-1);
}

void download_segments(Tracker *tracker, Client *clients, int rank, char *filename, char *segment, int sgm_idx) {
    MPI_Status status;
    CFileWanted crt_file = find_file_for_client(clients, rank, filename);

    for (int i = clients[rank].last_swarm_idx + 1; i < crt_file.no_of_swarm_clients; i++) {
        int destination = crt_file.swarm_clients[i];

        if (destination == rank) {
            if (i == crt_file.no_of_swarm_clients - 1) {
                i = -1;
            }
            continue;
        }

        char message[5] = "SEGM";
        MPI_Send(message, 5, MPI_CHAR, destination, 1, MPI_COMM_WORLD);

        // Send length of file name
        int length = strlen(crt_file.file_name) + 1;
        MPI_Send(&length, 1, MPI_INT, destination, 1, MPI_COMM_WORLD);

        // Send file name
        MPI_Send(crt_file.file_name, length, MPI_CHAR, destination, 1, MPI_COMM_WORLD);

        // Send segment to request
        MPI_Send(segment, HASH_SIZE + 1, MPI_CHAR, destination, 1, MPI_COMM_WORLD);  

        // Wait for response
        char response[4];
        MPI_Recv(response, 4, MPI_CHAR, destination, 2, MPI_COMM_WORLD, &status);

        // UNCOMMENT FOR PRINTING THE ANSWERS THAT THE CLIENTS GET FROM REQUESTING THE SWARM CLIENTS
        // printf("CLIENT %d GOT %s from client %d.\n", rank, response, destination);

        if (strcmp(response, "ACK") == 0) {
            for (int j = 0; j < clients[rank].no_of_wanted_files; j++) {
                if (strstr(clients[rank].outputs[j].filename, filename) != NULL) {
                    // UNCOMMENT FOR PRINTING THE VARIETY OF THE CLIENTS INTERROGATED FROM THE SWARM FILE
                    // printf("Added segment %s for file %s by client %d from client %d\n", segment, crt_file.file_name, rank, destination);

                    // Add to file
                    fprintf(clients[rank].outputs[j].file, "%s\n", segment);

                    // Increase the consecutive segments
                    clients[rank].consecutive_segments++;

                    // Mark segment as owned
                    crt_file.owned_segments[sgm_idx] = 1;

                    // Change last swarm index for this client
                    clients[rank].last_swarm_idx = i;
                    if (clients[rank].last_swarm_idx == crt_file.no_of_swarm_clients - 1) {
                        clients[rank].last_swarm_idx = -1;
                    }
                    return;
                }
            }
        } else if (strcmp(response, "SNO") == 0) {
            if (i == crt_file.no_of_swarm_clients - 1) {
                // Reset loop if no swarm client had the segment
                i = -1;
            } else {
                continue;
            }
        }
    }
}

int check_downloads_client(Client *clients, int rank) {
    // Check if all files have been marked as owned
    for (int i = 0; i < clients[rank].no_of_wanted_files; i++) {
        if (clients[rank].wanted_files[i].owned == 0) {
            return 0;
        }
    }
    return 1;
}

int all_clients_finished(Tracker *tracker, int no_of_clients) {
    for (int i = 1; i <= no_of_clients; i++) {
        if (tracker->clients[i] == 0) {
            return 0;
        }
    }
    return 1;
}

void print_clients(Client *clients, int no_of_clients) {
    for (int i = 1; i <= no_of_clients; i++) {
        printf("CLIENT %d has the following information:\n", i);

        printf("Number of owned files: %d\n", clients[i].no_of_owned_files);

        for (int j = 0; j < clients[i].no_of_owned_files; j++) {
            printf("   Name for owned file %d is %s\n", j, clients[i].files[j].file_name);
            printf("   It has %d segments.\n", clients[i].files[j].no_of_segments);
            for (int k = 0; k < clients[i].files[j].no_of_segments; k++) {
                printf("      %s\n", clients[i].files[j].file_segments[k]);
            }
        }

        printf("Number of wanted files: %d\n", clients[i].no_of_wanted_files);

        for (int j = 0; j < clients[i].no_of_wanted_files; j++) {
            printf("   %s\n", clients[i].wanted_files[j].file_name);
        }

        printf("\n");
    }
}

void print_tracker(Tracker *tracker) {
    printf("Tracker contains:\n");
    for (int i = 0; i < tracker->total_files; i++) {
        printf("%s and has %d segments:\n", tracker->files[i].file_name, tracker->files[i].uploaded_segments);

        for (int j = 0; j < tracker->files[i].uploaded_segments; j++) {
            printf("   %s\n", tracker->files[i].segments[j]);
        }

        printf("The swarm file for file %s is:\n", tracker->files[i].file_name);
        for (int j = 0; j < tracker->files[i].swarm_size; j++) {
            printf("   %d\n", tracker->files[i].swarm[j]);
        }
        printf("\n");
    }
}

void free_client(Client *client) {
    for (int i = 0; i < client->no_of_owned_files; i++) {
        free(client->files[i].file_name);
        for (int j = 0; j < client->files[i].no_of_segments; j++) {
            free(client->files[i].file_segments[j]);
        }
        free(client->files[i].file_segments);
    }
    free(client->files);

    for (int i = 0; i < client->no_of_wanted_files; i++) {
        free(client->wanted_files[i].file_name);
        free(client->wanted_files[i].owned_segments);
        for (int j = 0; j < MAX_SEGMENTS; j++) {
            free(client->wanted_files[i].downloaded_segments[j]);
        }
        free(client->wanted_files[i].downloaded_segments);
    }
    free(client->wanted_files);
}

void free_tracker(Tracker *tracker) {
    for (int i = 0; i < MAX_FILES; i++) {
        free(tracker->files[i].file_name);
        free(tracker->files[i].swarm);
    }
    for (int i = 0; i < MAX_FILES; i++) {
        for (int j = 0; j < MAX_SEGMENTS; j++) {
            free(tracker->files[i].segments[j]);
        }
        free(tracker->files->segments);
    }
    free(tracker->files);
    free(tracker->clients);
}