#include <iostream>
#include <pthread.h>
#include <stdexcept>

using namespace std;

class Court {
private:
    int playerCount; // Stores the number of players to conduct a match
    int currentPlayers; // Stores the number of players currently on the court
    int leftBeforeReferee = 0; // Tracks the number of players waiting for the referee to leave. 

    bool hasReferee; // Flag indicating whether a referee is present
    bool matchInProgress = false; // Flag indicating whether a match is currently in progress
    bool hasRefereeLeft = false; // Flag indicating whether the referee has left the court

    sem_t courtAccess; // Semaphore to control access to the court
    sem_t mutex; // Binary semaphore to provide atomicity for the enter and leave methods
    sem_t matchEnd; // Semaphore to make sure the players entering the enter method sleep while there is an ongoing match
    sem_t referee; // Semaphore to make sure the players leave after the referee 

    pthread_barrier_t gameStartBarrier; // Barrier to synchronize the start of the game

    pthread_t refereeID;  // Stores the ID of the referee if present

public:

    void play();

    // Constructor to initialize the Court 
    Court(int playersNeeded, int refereeNeeded) {
        // Throws an exception if the entered arguments are invalid
        if (playersNeeded <= 0 || !(refereeNeeded == 0 || refereeNeeded == 1)) {
            throw invalid_argument("An error occurred.");
        }

        // Initialize the values using arguments
        playerCount = playersNeeded;
        hasReferee = (refereeNeeded == 1);
        
        // Initialize the semaphores
        sem_init(&courtAccess, 0, playersNeeded + (hasReferee ? 1 : 0)); // Controls access to the court, including referee
        sem_init(&mutex, 0, 1); 
        sem_init(&matchEnd, 0, 0); 
        sem_init(&referee, 0, 0); 

        // Initialize the barrier
        pthread_barrier_init(&gameStartBarrier, nullptr, 1); // Barrier to start the game synchronously
    }

    ~Court() {
        // Destroy semaphores and barrier at destruction to release resources
        sem_destroy(&courtAccess);
        sem_destroy(&mutex);
        sem_destroy(&matchEnd);
        sem_destroy(&referee);
        pthread_barrier_destroy(&gameStartBarrier);
    }

    void enter() {

        pthread_t tid = pthread_self();
        printf("Thread ID: %ld, I have arrived at the court.\n", tid);

        // If a match is in progress
        if (matchInProgress) {
            sem_wait(&matchEnd); // Wait until current match ends
        }
        
        sem_wait(&courtAccess);  // Wait for availability in the court
        sem_wait(&mutex); // Wait to grab the mutex to ensure atomicity

        ++currentPlayers; // Increment the current players in the court

        if (currentPlayers == (playerCount + (hasReferee ? 1 : 0))) {

            matchInProgress = true; // Start match if enough players are present

            if (hasReferee) {
                refereeID = tid; // Set current thread as referee
            }
            else {
                refereeID = 0;
            }

            printf("Thread ID: %ld, There are enough players, starting a match.\n", tid);

            pthread_barrier_wait(&gameStartBarrier); // Ensure all players are ready before starting
            
            sem_post(&mutex); // Release the mutex
        } 
        else {
            printf("Thread ID: %ld, There are only %d players, passing some time.\n", tid, currentPlayers);
            sem_post(&mutex); // Release the mutex
        }
    }

    // Method for a player to leave the court
    void leave() {

        pthread_t tid = pthread_self();

        sem_wait(&mutex); // Wait to grab the mutex to ensure atomicity

        if (!matchInProgress) { // If match didn't start, just leave
            --currentPlayers; // Decrement the count of players at the court
            printf("Thread ID: %ld, I was not able to find a match and I have to leave.\n", tid);
            sem_post(&courtAccess); // Release court for others to enter
            sem_post(&mutex); // Release the mutex
        }

        else {
            if (pthread_equal(tid, refereeID)) { // If the current thread is the referee
                hasRefereeLeft = true; // Set the flag that the referee has left
                --currentPlayers; // Decrement the count of players at the court
                printf("Thread ID: %ld, I am the referee and now, match is over. I am leaving.\n", tid);
                for (int i = 0; i < leftBeforeReferee; i++) {
                    sem_post(&referee); // Release the players waiting for the referee to leave
                }
                sem_post(&mutex); // Release the mutex
            }
            else {
                if (hasReferee && !hasRefereeLeft) { // If the players must wait for the referee to leave first
                    ++leftBeforeReferee; // Increment the counter of the players trying to leave before the referee
                    sem_post(&mutex); // Release the mutex
                    sem_wait(&referee); // Wait for the referee to leave
                    sem_wait(&mutex); // Acquire the mutex
                }
                --currentPlayers; // Decrement the count of players at the court
                printf("Thread ID: %ld, I am a player and now, I am leaving.\n", tid);
                if (currentPlayers == 0) { // If all players have left
                    matchInProgress = false;
                    printf("Thread ID: %ld, everybody left, letting any waiting people know.\n", tid);
                    for (int i = 0; i < playerCount + (hasReferee ? 1 : 0); i++) {
                        sem_post(&matchEnd); // Signal end of match to waiting players
                        sem_post(&courtAccess); // Signal that court can be accessed by waiting players
                    }

                    // Reset variables
                    leftBeforeReferee = 0;
                    hasRefereeLeft = false;
                }

                sem_post(&mutex); // Release the mutex
            }
        }
    }
};
