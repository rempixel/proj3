#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <semaphore.h>
#include <stdbool.h>

#define RANGE 1
#define ALLDONE 2
#define GO 3
#define GENDONE 4 //Generation Done
#define MAXTHREAD 10
#define MAXGRID 40
#define ON '1'
#define OFF '0'

struct msg {
	int iSender; // sender of the message (0 .. number-of-threads) 
	int type;    // its type 
	int value1;  // first value 
	int value2;  // second value 
};

// global variables
char **A; // board 1
char **B; // board 2
char **C; // board 3
struct msg **mailboxes; 
pthread_t **threads;       
sem_t **senderSem;
sem_t **ReceSem;
char **boards[3];
int inThreads; 
int rows; 
int columns;
long iteration, generations;

void SendMsg(int iTo, struct msg *pMsg) {
	sem_wait(senderSem[iTo]); // wait for mailbox to be available
	mailboxes[iTo] = pMsg;
	sem_post(ReceSem[iTo]); // release mailbox semaphore

}

void RecvMsg(int iFrom, struct msg *pMsg) {
	sem_wait(ReceSem[iFrom]); // wait for message
	*pMsg = *mailboxes[iFrom];
	sem_post(senderSem[iFrom]); // release the mailbox semaphore

}

//print the board
void printGrid(char **board, long iteration) {

	printf("Generation %ld:\n", iteration);
	for (int j = 0; j < rows; j++) {
		for (int i = 0; i < columns; i++) {
			if (board[i][j] == ON)
				printf("1 ");
			else
				printf("0 ");
		}
		printf("\n");
	}
	printf("\n");
}

int getNeighbors(unsigned xCell, unsigned yCell, char *grid[]) {
    const int dx[] = {-1, 0, 1, 1, 1, 0, -1, -1};
    const int dy[] = {-1, -1, -1, 0, 1, 1, 1, 0};
    int neighborCount = 0;

    for (int i = 0; i < 8; ++i) {
        int newX = xCell + dx[i];
        int newY = yCell + dy[i];

        // Check if the neighbor coordinates are within grid bounds and ON
        if (newX >= 0 && newX < columns && newY >= 0 && newY < rows && grid[newX][newY] == ON) {
            ++neighborCount;
        }
    }

    return neighborCount;
}

// waits for a message from parent thread, then plays the grid for the given rows
void *playLife(void *arg) {
    
	int threadId = (int)(intptr_t)arg;

    struct msg *message;

	message = (struct msg *)malloc(sizeof(struct msg));

	RecvMsg(threadId, message); // wait for message from parent

	int bot = message->value1;
	int top = message->value2;

	for (long cGeneration = 0; cGeneration <= generations; cGeneration++) {
		RecvMsg(threadId, message); // wait for message from parent

		//one generation (at a time)
		for (int j = bot; j <= top; j++) {
			for (int i = 0; i < columns; i++) {
                int neighbors;
				if (((neighbors = getNeighbors(i, j, boards[cGeneration % 3])) == 3) ||
				    (neighbors == 2 && boards[cGeneration % 3][i][j] == ON)) {
					boards[(cGeneration + 1) % 3][i][j] = ON;
				} else {
					boards[(cGeneration + 1) % 3][i][j] = OFF;
				}
			}
		}

		
		message->iSender = threadId;
		message->type = GENDONE;
		
		SendMsg(0, message); // send GENDONE back
	}

	// send ALLDONE message
	message->iSender = threadId;
	message->type = ALLDONE;

	SendMsg(0, message);

	pthread_exit(NULL);
}

int arrayEquals(char *A[], char *B[], int rows, int columns) {
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < columns; col++) {
            if (A[row][col] != B[row][col])
                return 0;
        }
    }
    return 1;
}
// returns 0 for not done, 1 for loop, 2 for oscillate
int checkDone(char *A[], char *B[], char *C[]) {
	if (arrayEquals(A, B, rows, columns))
		return 1;
	else if (arrayEquals(C, B, rows, columns))
		return 2;
	else
		return 0;
}


int main(int argc, char *argv[]) {
	// parse cmd line args
	if (argc < 4) {
		printf("Usage: %s <numThreads>, <filename>, <generations>, <print>, <pause>\n", argv[0]);
		exit(1);
	}

	inThreads = atoi(argv[1]);
	generations = atol(argv[3]);

	if(inThreads < 1 || inThreads > MAXTHREAD){
        printf("Number of threads must be between 1 and %d.\n", MAXTHREAD);
        exit(1);
    }

	//check to print/pause
	int doPrint = 0;
	int doPause = 0;
	if (argc > 4){
		doPrint = (argv[4][0] == 'y');
	}
	if (argc > 5){
		doPause = (argv[5][0] == 'y');
	}

	// open file
	FILE *fp;
	if ((fp = fopen(argv[2], "r")) == NULL) {
		printf("File open error\n");
		exit(1);
	}
	//count row columns
	char *line = NULL;
	size_t len = 0;
	int temp;
	rows = 0;
	columns = 0;
	while ((temp = getline(&line, &len, fp)) != -1) {
		temp = temp / 2;
		if (temp > columns) columns = temp;
		rows++;
	}

	// mem alloc
	char **A = (char **)malloc(columns * sizeof(char *));
	char **B = (char **)malloc(columns * sizeof(char *));
	char **C = (char **)malloc(columns * sizeof(char *));
	mailboxes = (struct msg **)malloc((inThreads + 1) * sizeof(struct msg *));
	struct msg **mailToSend = (struct msg **)malloc(inThreads * sizeof(struct msg *));
	struct msg *received = (struct msg *)malloc(sizeof(struct msg));
	senderSem = (sem_t **)malloc((inThreads + 1) * sizeof(sem_t *));
	ReceSem = (sem_t **)malloc((inThreads + 1) * sizeof(sem_t *));
	threads = (pthread_t **)malloc(inThreads * sizeof(pthread_t *));
	for (int i = 0; i < columns; i++) {
		A[i] = (char *)malloc(rows * sizeof(char));
		B[i] = (char *)malloc(rows * sizeof(char));
		C[i] = (char *)malloc(rows * sizeof(char));
	}

	// make the mailboxes
	for (int i = 0; i <= inThreads; i++) {
		senderSem[i] = (sem_t *)malloc(sizeof(sem_t));
		ReceSem[i] = (sem_t *)malloc(sizeof(sem_t));
		sem_init(senderSem[i], 0, 1);
		sem_init(ReceSem[i], 0, 0);
	}

	// make the messages to send
	int step = rows / inThreads;
	temp = 0;
	for (int i = 0; i < inThreads; i++) {
		mailToSend[i] = (struct msg *)malloc(sizeof(struct msg *));
		mailToSend[i]->iSender = 0;
		mailToSend[i]->type = RANGE;
		mailToSend[i]->value1 = temp;
		temp += step;
		mailToSend[i]->value2 = (i == inThreads - 1) ? rows - 1 : temp;
	}

	for (int i = 0; i < inThreads; i++) {
		SendMsg(i + 1, mailToSend[i]);
	}


	for (int j = 0; j < rows; j++) {
		for (int i = 0; i < columns; i++) {
			A[i][j] = OFF;
			B[i][j] = OFF;
			C[i][j] = OFF;
		}
	}

	// write file into grid
	rewind(fp);
	int i = 0;
	int j = 0;
	char fileChar;
	while ((fileChar = fgetc(fp)) != EOF) {
		if (i >= columns) {
			i = 0;
			j++;
			while (fileChar != '\n') fileChar = fgetc(fp);
		} else if (fileChar == '1') {
			A[i][j] = ON;
			i++;
		} else if (fileChar == '0') {
			i++;
		} else if (fileChar == '\n') {
			i = 0;
			j++;
		}
	}

	//Prepare board for start
	iteration = 0;
	boards[0] = A;
	boards[1] = B;
	boards[2] = C;

	// create threads
	for (int i = 0; i < inThreads; i++) {
		threads[i] = (pthread_t *)malloc(sizeof(pthread_t *));
		if (pthread_create(threads[i], NULL, &playLife, (void *)(intptr_t)(i + 1))) {
			printf("error creating thread %d\n", i + 1);
		}
	}

	// print gen 0
	printGrid(A, iteration); 
	//Start
	while (iteration < generations) {
    // Send GO message to worker threads
    for (int i = 1; i <= inThreads; i++) {
        mailToSend[i - 1]->type = GO;
        mailToSend[i - 1]->iSender = 0;
        SendMsg(i, mailToSend[i - 1]);
    }

    // Receive results from worker threads
    for (int i = 1; i <= inThreads; i++) {
        RecvMsg(0, received);
    }

    // Print grid if required and iteration is greater than 0
    if (doPrint && iteration > 0) {
        printGrid(boards[iteration % 3], iteration);
    }

    // Check if the simulation is done
    int done = checkDone(boards[iteration % 3], boards[(iteration + 1) % 3], boards[(iteration + 2) % 3]);

    if (done == 1 || done == 2) {
        break;
    }

    // Pause if required
    if (doPause) {
        printf("Press Enter to continue.\n");
        getchar();
    }

    // Move to the next iteration
    iteration++;
}

	//Print the final board. 
	printf("Final Board:\n");
	printGrid(boards[iteration % 3], iteration);


	// clean up 
	for (int i = 0; i < columns; i++) {
		free(A[i]);
		free(B[i]);
		free(C[i]);
	}
	for (int i = 0; i < inThreads; i++) {
		free(mailToSend[i]);
		sem_destroy(senderSem[i]);
		sem_destroy(ReceSem[i]);
		free(threads[i]);
	}
	free(A);
	free(B);
	free(C);
	free(mailboxes);
	free(mailToSend);
	sem_destroy(senderSem[inThreads]);
	sem_destroy(ReceSem[inThreads]);
	free(senderSem);
	free(ReceSem);
	free(threads);
	free(received);

	return 0;
}

