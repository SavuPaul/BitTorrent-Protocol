#include "tema2.h"

Client *clients;
int no_of_clients;
Tracker *tracker;

void *download_thread_func(void *arg)
{
    int rank = *(int*) arg;
    MPI_Status status;

    // Get the number of files that the client wants
    int no_of_wanted_files = clients[rank].no_of_wanted_files;

    // Send all wanted file details to the tracker
    for (int i = 0; i < no_of_wanted_files; i++) {
        char *name = clients[rank].wanted_files[i].file_name;
        int length = strlen(clients[rank].wanted_files[i].file_name) + 1;
        
        // Send file name length first
        MPI_Send(&length, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);

        // Send the actual file name
        MPI_Send(name, length, MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);

        // Receive the segments of the files        
        // Receive the number of segments which will be downloaded and the segments as well
        MPI_Recv(&clients[rank].wanted_files[i].no_of_segments, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD, &status);

        for (int j = 0; j < clients[rank].wanted_files[i].no_of_segments; j++) {
            char *segment = malloc((HASH_SIZE + 1) * sizeof(char));
            MPI_Recv(segment, HASH_SIZE + 1, MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD, &status);
            clients[rank].wanted_files[i].downloaded_segments[j] = segment;
        }

        // Client has to check what segments are already owned and what segments are missing and mark them
        // for the specific file at index i (1 if owned, 0 if not owned)
        mark_owned_segments(clients, rank, i);
    }

    for (int i = 0; i < no_of_wanted_files; i++) {
        // Download segments for missing files
        int got_swarm = 0;

        // For each segment
        for (int j = 0; j < clients[rank].wanted_files[i].no_of_segments; j++) {
            char *filename;
            if (clients[rank].consecutive_segments == 10 || got_swarm == 0) {
                // Get Swarm File (got_swarm is used only once for the very first SWARM request)
                got_swarm = 1;

                // Reset counter
                clients[rank].consecutive_segments = 0;

                // Send "SWARM" message
                char *message = "SWARM";
                MPI_Send(message, 6, MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);

                // Send filename length
                int length = strlen(clients[rank].wanted_files[i].file_name) + 1;
                MPI_Send(&length, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);

                // Send filename
                filename = clients[rank].wanted_files[i].file_name;
                MPI_Send(filename, length, MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);

                // Receive the seed/peer swarms
                MPI_Recv(&clients[rank].wanted_files[i].no_of_swarm_clients, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD, &status);

                for (int j = 0; j < clients[rank].wanted_files[i].no_of_swarm_clients; j++) {
                    MPI_Recv(&clients[rank].wanted_files[i].swarm_clients[j], 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD, &status);
                }
            }

            if (j == 0) {
                clients[rank].last_swarm_idx = -1;
            }

            char *segment = clients[rank].wanted_files[i].downloaded_segments[j];
            download_segments(tracker, clients, rank, filename, segment, j);
        }
    }

    // If the while has been exited, it means that the client has finished downloading
    // all the wanted files, so the client needs to send a message to the tracker

    char *message = "CLOSE";
    MPI_Send(message, strlen(message) + 1, MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);

    return NULL;
}

void *upload_thread_func(void *arg)
{
    int rank = *(int*) arg;
    MPI_Status status;

    // The following while stops when tracker sends "EXIT" message
    while (1) {
        char message[20];
        MPI_Recv(message, 5, MPI_CHAR, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &status);

        int sender_rank = status.MPI_SOURCE;

        if (strcmp(message, "EXIT") == 0) {
            // Exit
            break;
        } else if (strcmp(message, "SEGM") == 0) {
            int length = 0;
            MPI_Recv(&length, 1, MPI_INT, sender_rank, 1, MPI_COMM_WORLD, &status);

            char filename[length];
            MPI_Recv(filename, length, MPI_CHAR, sender_rank, 1, MPI_COMM_WORLD, &status);

            char segment[HASH_SIZE + 1];
            MPI_Recv(segment, HASH_SIZE + 1, MPI_CHAR, sender_rank, 1, MPI_COMM_WORLD, &status);

            int owned = 0;
            // Look through both owned files and owned-downloaded files
            for (int i = 0; i < clients[rank].no_of_owned_files; i++) {
                if (strcmp(clients[rank].files[i].file_name, filename) == 0) {
                    for (int j = 0; j < clients[rank].files[i].no_of_segments; j++) {
                        if (strcmp(clients[rank].files[i].file_segments[j], segment) == 0) {
                            owned = 1;
                        }
                    }
                }
            }

            for (int i = 0; i < clients[rank].no_of_wanted_files; i++) {
                if (strcmp(clients[rank].wanted_files[i].file_name, filename) == 0) {
                    for (int j = 0; j < clients[rank].wanted_files[i].no_of_segments; j++) {
                        if (strcmp(clients[rank].wanted_files[i].downloaded_segments[j], segment) == 0) {
                            // Verify if it is owned
                            if (clients[rank].wanted_files[i].owned_segments[j] == 1) {
                                owned = 1;
                            }
                        }
                    }
                }
            }

            if (owned == 1) {
                // Send confirmation message
                char message[4] = "ACK";
                MPI_Send(message, 4, MPI_CHAR, sender_rank, 2, MPI_COMM_WORLD);
            } else {
                // "SNO" stands for Segment Not Owned
                char message[4] = "SNO";
                MPI_Send(message, 4, MPI_CHAR, sender_rank, 2, MPI_COMM_WORLD);
            }
        }
    }

    return NULL;
}

void tracking(Tracker *tracker, int no_of_clients, int numtasks, int rank) {
    // UNCOMMENT FOR DEBUGGING (prints main client fields)
    // printf("Number of clients is: %d\n\n", no_of_clients);
    // print_clients(clients, no_of_clients);

    // Receive all file information from clients
    receive_file_info(tracker, no_of_clients);

    // UNCOMMENT FOR DEBUGGING (prints all tracker details)
    // print_tracker(tracker);

    // Signal the clients that they can start communicating
    char start_signal[4] = "ACK";
    for (int i = 1; i <= no_of_clients; i++) {
        MPI_Send(start_signal, 4, MPI_CHAR, i, 0, MPI_COMM_WORLD);
    }

    // Sends segments to clients for the requested files
    for (int i = 1; i <= no_of_clients; i++) {
        receive_and_deal_requests(tracker, clients, i);
    }

    // This while stops only when all clients have finished downloading
    while (all_clients_finished(tracker, no_of_clients) == 0) {
        for (int i = 1; i <= no_of_clients; i++) {
            MPI_Status status;
            char message[no_of_clients][6];

            // Receive the type of message that the client sent
            if (tracker->clients[i] == 0) {
                MPI_Recv(message[i], 6, MPI_CHAR, i, 0, MPI_COMM_WORLD, &status);
            }

            if (strcmp(message[i], "SWARM") == 0) {
                // Receive the file information to send the swarm for
                int length;
                MPI_Recv(&length, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);

                char filename[MAX_FILENAME];
                MPI_Recv(filename, length, MPI_CHAR, i, 0, MPI_COMM_WORLD, &status);

                // Send the swarm file information to source client
                send_swarm_info(tracker, filename, i, 0);
            } else if (strcmp(message[i], "CLOSE") == 0) {
                // This means that the client has finished downloading all the wanted files
                tracker->clients[i] = 1;
            }
        }
    }

    // Send EXIT message to clients so that they can close upload threads
    for (int i = 1; i <= no_of_clients; i++) {
        char message[5] = "EXIT";
        MPI_Send(message, strlen(message) + 1, MPI_CHAR, i, 1, MPI_COMM_WORLD);
    }
}

void peer(Client *clients, int numtasks, int rank) {
    // The clients have to send their files and segments to the tracker (rank 0)
    send_file_info_to_tracker(clients, rank);

    // Clients wait for an ACK message from the tracker
    MPI_Status mpi_status;
    char message[4];
    MPI_Recv(message, 4, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &mpi_status);

    // If message is "ACK", proceed
    if (strcmp(message, "ACK") == 0) {
        pthread_t download_thread;
        pthread_t upload_thread;
        void *status;
        int r;

        r = pthread_create(&download_thread, NULL, download_thread_func, (void *) &rank);
        if (r) {
            printf("Eroare la crearea thread-ului de download\n");
            exit(-1);
        }

        r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *) &rank);
        if (r) {
            printf("Eroare la crearea thread-ului de upload\n");
            exit(-1);
        }

        r = pthread_join(download_thread, &status);
        if (r) {
            printf("Eroare la asteptarea thread-ului de download\n");
            exit(-1);
        }

        r = pthread_join(upload_thread, &status);
        if (r) {
            printf("Eroare la asteptarea thread-ului de upload\n");
            exit(-1);
        }
    }
}
 
int main (int argc, char *argv[]) {
    int numtasks, rank;
 
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // Create an array of clients
    clients = init_all_clients(&no_of_clients, rank);

    // Creates the output files for the client
    if (rank > 0) {
        create_output_files(clients, rank);
    }

    if (rank == TRACKER_RANK) {
        // Create the tracker
        tracker = init_tracker(no_of_clients);
        tracking(tracker, no_of_clients, numtasks, rank);
    } else {
        peer(clients, numtasks, rank);
    }

    MPI_Finalize();
}
