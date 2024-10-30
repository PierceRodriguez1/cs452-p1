#include <stdlib.h>
#include <sys/time.h> /* for gettimeofday system call */
#include "lab.h"
#include <stdio.h>

/* Standard insertion sort */
static void insertion_sort(int A[], int p, int r)
{
    int j;
    for (j = p + 1; j <= r; j++)
    {
        int key = A[j];
        int i = j - 1;
        while ((i >= p) && (A[i] > key))
        {
            A[i + 1] = A[i];
            i--;
        }
        A[i + 1] = key;
    }
}

void mergesort_s(int A[], int p, int r)
{
    if (r - p + 1 <= INSERTION_SORT_THRESHOLD)
    {
        insertion_sort(A, p, r);
    }
    else
    {
        int q = (p + r) / 2;
        mergesort_s(A, p, q);
        mergesort_s(A, q + 1, r);
        int *B = (int *)malloc(sizeof(int) * (r - p + 1));
        merge_s(A, p, q, r);
        free(B);
    }
}

void merge_s(int A[], int p, int q, int r)
{
  int *B = (int *)malloc(sizeof(int) * (r - p + 1));

  int i = p;
  int j = q + 1;
  int k = 0;
  int l;

  /* as long as both lists have unexamined elements */
  while ((i <= q) && (j <= r))
    {
      if (A[i] < A[j])
        {
          B[k] = A[i];
          i++;
        }
      else
        {
          B[k] = A[j];
          j++;
        }
      k++;
    }

  /* copy remaining elements from the first list */
  while (i <= q)
    B[k++] = A[i++];

  /* copy remaining elements from the second list */
  while (j <= r)
    B[k++] = A[j++];

  /* copy merged output from array B back to array A */
  for (l = p, k = 0; l <= r; l++, k++)
    A[l] = B[k];

  free(B);
}

void mergesort_mt(int *A, int n, int num_thread) {
    struct parallel_args *args = malloc(sizeof(struct parallel_args) * num_thread);
    int chunk_size = n / num_thread;
    int i;

    // Divide the array into chunks and assign each to a thread
    for (i = 0; i < num_thread; i++) {
        args[i].A = A;
        args[i].start = i * chunk_size;
        args[i].end = (i + 1) * chunk_size - 1;
        if (i == num_thread - 1) {
            args[i].end = n - 1; // Ensure last thread processes to the end
        }
        pthread_create(&args[i].tid, NULL, parallel_mergesort, &args[i]);
    }
    
    // Join all threads after they have completed sorting their sections
    for (i = 0; i < num_thread; i++) {
        pthread_join(args[i].tid, NULL);
    }
    
    // Perform merging in stages
    int step = chunk_size;
    while (step < n) {
        for (i = 0; i + step < n; i += 2 * step) {
            int end = (i + 2 * step - 1 < n) ? i + 2 * step - 1 : n - 1;
            merge_s(A, i, i + step - 1, end);
        }
        step *= 2;
    }

    free(args);
}

double getMilliSeconds()
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return (double)now.tv_sec * 1000.0 + now.tv_usec / 1000.0;
}

void *parallel_mergesort(void *args)
{
    struct parallel_args *pargs = (struct parallel_args *)args;
    mergesort_s(pargs->A, pargs->start, pargs->end);
    return NULL;
}

int myMain(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("usage: %s <array_size> <num_threads>\n", argv[0]);
        return 1;
    }
    int size = atoi(argv[1]);
    int t = atoi(argv[2]);

    int *A_ = malloc(sizeof(int) * size);
    if (A_ == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    srandom(1);
    for (int i = 0; i < size; i++)
        A_[i] = random() % 100000;

    double start = getMilliSeconds();
    mergesort_mt(A_, size, t);
    double end = getMilliSeconds();

    printf("Time: %f ms, Threads: %d\n", end - start, t);

    free(A_);
    return 0;
}
