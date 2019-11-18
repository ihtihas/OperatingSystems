#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <stdint.h>
#include <math.h>
#include <error.h>
#include <iostream>
#include <fstream>

using namespace std;

// Global constants

#define MAX 1000

// Global variables

int count = 0;      // variable to be incremented by each thread
sem_t mutex, barrierTop, barrierEnd; // global semaphore
pthread_t tid[MAX];
int size = 0, phase = 0;
int a[MAX];
char mode;

void *sort(void *arg)
{
  int id = (intptr_t)arg;

  int q = 1;
  bool sorted = true;

  do
  {

    for (int p = 1; p <= (log2(size)); p++) // (log N phases)
    {

      sem_wait(&mutex);
      count += 1;
      if (count == (size / 2)) //Once last thread passes, it opens the barrier for all the threads
      {
        for (int i = 0; i < size / 2; i++)
        {
          sem_post(&barrierTop);
        }
      }
      sem_post(&mutex);
      sem_wait(&barrierTop); //Initially barrier is closed

      int num_groups = pow(2, (p - 1)); // you can start from 1, and set num_groups *= 2 after each phase
      int group_size = size / num_groups;
      int gindex = id / (group_size / 2);
      int mindex = id % (group_size / 2); // mindex: group member index
      int group_start = gindex * group_size;
      int group_end = ((gindex + 1) * group_size) - 1;
      int data1 = a[group_start + mindex];
      int data2 = a[group_end - mindex];
      if (data1 > data2)
      {
        int tmp = data1;
        data1 = data2;
        data2 = tmp;
      }
      a[group_start + mindex] = data1;
      a[group_end - mindex] = data2;
      for (int i = 0; i < (data1 + data2); i++)
      {
      } // iterations doing nothing (just to introduce different computation times);

      sem_wait(&mutex);
      count -= 1;
      if (mode == 'o')
      {
        cout << "Thread " << id << " finished stage " << q << " phase " << p << endl;
      }
      if (count == 0)
      {
        if (mode == 'o')
        {
          for (int i = 0; i < size; i++)
          {
            cout << a[i] << " ";
            if ((i + 1) % 8 == 0 && (i + 1 < size))
              cout << endl;
          }
          cout << endl;
        }
        for (int i = 0; i < size / 2; i++)
        {
          sem_post(&barrierEnd); //once last thread finishes , it opens the bottom barrier for all threads
        }
      }

      sem_post(&mutex);
      sem_wait(&barrierEnd);
    }

    sorted = true;
    for (int i = 0; i < size - 1; i++)
    {

      if (a[i] > a[i + 1])
      {
        sorted = false;
        break;
      }
    }

    q += 1;

  } while (!sorted);

  return NULL;
}

// ***********************************************************************
// The main program:
// ***********************************************************************

int main(int argc, char *argv[])
{
  //Get and open the file
  if (argc < 3)
  {
    cerr << "***Too less arguments.. Filename and reading mode should be provided***" << endl;
    exit(1);
  }
  string filename = argv[1];
  mode = *argv[2];
  string semFile = "sema.init";
  ifstream fin;
  ifstream semin; // declare an input file stream
  int n = 0;
  // open file file_name for input
  fin.open(filename.c_str(), ios::in);
  // check if file is opened for input
  if (!fin.is_open())
  {
    cerr << "Unable to open Input file " << filename << endl;
    exit(0);
  }
  fin >> n;

  semin.open(semFile.c_str(), ios::in);
  if (!semin.is_open())
  {
    cerr << "Unable to open Initialization file " << semFile << endl;
    exit(0);
  }

  int initValues[MAX];
  int m;
  semin >> m;

  for (int i = 0; i < m; i++)
  {
    semin >> initValues[i];
  }

  while (n != 0)
  {
    //read N integer numbers and store them in an array
    size = n;

    for (int i = 0; i < n; i++)
    {
      a[i] = 0;
      fin >> a[i];
    }
    for (int i = 0; i < size; i++)
    {
      cout << a[i] << " ";
      if ((i + 1) % 8 == 0 && (i + 1 < size))
        cout << endl;
    }
    cout << endl;
    //create and initialize the semaphores necessary for synchronization

    sem_init(&mutex, 0, initValues[0]);
    sem_init(&barrierTop, 0, initValues[1]);
    sem_init(&barrierEnd, 0, initValues[2]);

    //create N/2 threads to sort the array using balanced sort algorithm

    for (int i = 0; i < n / 2; i++)
    {
      pthread_create(&tid[i], NULL, sort, (void *)(intptr_t)i);
    }

    //wait for all the threads to finish
    for (int i = 0; i < n / 2; i++)
    {
      pthread_join(tid[i], NULL);
    }

    
      for (int i = 0; i < size; i++)
      {
        cout << a[i] << " ";
        if ((i + 1) % 8 == 0 && (i + 1 < size))
          cout << endl;
      }
      cout << endl;
    

    cout << "-------------------------------------------------------------------------" << endl;
    //read the number of integers in the input list, N
    fin >> n;
  }
}
