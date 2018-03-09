#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <stdio.h>

using namespace std;

unsigned int* assignShm(int dimension, int *idC){
    key_t key = IPC_PRIVATE;
    int shmflag = IPC_CREAT | 0666;
    int shmidC = shmget(key, sizeof(unsigned int) * dimension * dimension, shmflag);

    if(shmidC == -1){
        cout << "Error...\n";
        exit(0);
    }
    
    unsigned int *ptr = (unsigned int *)shmat(shmidC, NULL, 0);
    *idC = shmidC;

    return ptr;
}

int Shmdt(unsigned int *ptr){
	int detect = shmdt(ptr);
	if(detect == -1){
		perror("shmdt");
		exit(0);
	}
	return detect;
}

int processOne(int dimension, unsigned int *matrixA, unsigned int *matrixB){
	unsigned int checkSum = 0;
	for(int i=0;i<dimension;i++){
		for(int j=0;j<dimension;j++){
			for(int k=0;k<dimension;k++)
				checkSum += matrixA[i * dimension + k] * matrixB[k * dimension + j];
		}
	}
	
	return checkSum;
}

void processFour(int dimension, unsigned int *matrixA, unsigned int *matrixB, unsigned int *matrixC){
	unsigned int checkSum = 0;
	int deg = dimension / 4;

	int pid1;
	pid1 = fork();
	//cout << "pid1: " << pid1 << endl;

	if(pid1 == 0){
		//	child1 calculate
		int pid2;
		pid2 = fork();
		//cout << "pid2: " << pid2 << endl;

		if(pid2 == 0){
			//	child2 calculate
			for(int i=deg;i<deg*2;i++){
				for(int j=0;j<dimension;j++){
					for(int k=0;k<dimension;k++)
						matrixC[i * dimension + j] += matrixA[i * dimension + k] * matrixB[k * dimension + j];
				}
			}

			Shmdt(matrixA);
			Shmdt(matrixB);
			Shmdt(matrixC);
			exit(0);
		}
		else if(pid2 > 0){
			//	parent
			for(int i=deg*2;i<deg*3;i++){
				for(int j=0;j<dimension;j++){
					for(int k=0;k<dimension;k++)
						matrixC[i * dimension + j] += matrixA[i * dimension + k] * matrixB[k * dimension + j];
				}
			}
			wait(NULL);
		}
		else{
			cout << "Error...\n";
		}
		Shmdt(matrixA);
		Shmdt(matrixB);
		Shmdt(matrixC);
		exit(0);
	}
	else if(pid1 > 0){
		//	parent
		int pid3;
		pid3 = fork();
		//cout << "pid3: " << pid3 << endl;

		if(pid3 == 0){
			//	child
			for(int i=deg*3;i<dimension;i++){
				for(int j=0;j<dimension;j++){
					for(int k=0;k<dimension;k++)
						matrixC[i * dimension + j] += matrixA[i * dimension + k] * matrixB[k * dimension + j];
				}
			}
			Shmdt(matrixA);
			Shmdt(matrixB);
			Shmdt(matrixC);
			exit(0);
		}
		else if(pid3 > 0){
			//	parent
			for(int i=0;i<deg;i++){
				for(int j=0;j<dimension;j++){
					for(int k=0;k<dimension;k++)
						matrixC[i * dimension + j] += matrixA[i * dimension + k] * matrixB[k * dimension + j];
				}
			}
			wait(NULL);
		}
		else{
			cout << "Error...\n";
		}
		wait(NULL);
	}
	else{
		cout << "Error...\n";
	}

	return;
}


int main(void)
{
	int dimension, Aid, Bid, Cid;
	cout << "Dimension: ";
	cin >> dimension;

	struct timeval start, end;
	double seconds, usec;

	//	Build Matrix A
	int num = 0;
	unsigned int *matrixA = assignShm(dimension, &Aid);	// get shared memory
	for(int i=0;i<dimension;i++){
		for(int j=0;j<dimension;j++)
			matrixA[i * dimension + j] = num++;
	}

	//	Build Matrix B
	num = 0;
	unsigned int *matrixB = assignShm(dimension, &Bid);	// get shared memory
	for(int i=0;i<dimension;i++){
		for(int j=0;j<dimension;j++)
			matrixB[i * dimension + j] = num++;
	}

	//	Build Matrix C
	num = 0;
	unsigned int *matrixC = assignShm(dimension, &Cid);	// get shared memory
	for(int i=0;i<dimension;i++){
		for(int j=0;j<dimension;j++)
			matrixC[i * dimension + j] = 0;
	}


	//	Process One
	int checkSum =0;
	gettimeofday(&start, 0);
	checkSum = processOne(dimension, matrixA, matrixB);
	gettimeofday(&end, 0);
	cout << "1 process, checksum = " << checkSum << endl;
	cout << "elapsed " << (double)(1000000 * (end.tv_sec-start.tv_sec)+ end.tv_usec-start.tv_usec) / 1000000 << "  s" << endl;

	//	Process Four
	checkSum = 0;
	gettimeofday(&start, 0);
	processFour(dimension, matrixA, matrixB, matrixC);
	gettimeofday(&end, 0);
	for(int i=0;i<dimension;i++){
		for(int j=0;j<dimension;j++)
			checkSum += matrixC[i * dimension + j];
	}
	cout << "4 process, checksum = " << checkSum << endl;
	cout << "elapsed " << (double)(1000000 * (end.tv_sec-start.tv_sec)+ end.tv_usec-start.tv_usec) / 1000000 << "  s" << endl;

	shmctl(Aid, IPC_RMID, 0);
	shmctl(Bid, IPC_RMID, 0);
	shmctl(Cid, IPC_RMID, 0);

	//system("ps aux | grep 'Z'");
	return 0;
}