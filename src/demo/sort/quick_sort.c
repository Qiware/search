#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>

static int _quick_sort(int *array, int low, int high)
{
    int v = array[high];
    int i = low, j = high;

    while (i < j) {
        while ((i < j) && (array[i] < v)) { ++i; }
        if (i < j) {
            array[j] = array[i];
        }

        while ((i < j) && (array[j] > v)) { --j; }
        if (i < j) {
            array[i] = array[j];
        }
    }

    array[j] = v;

	return j;
}

int quick_sort(int *array, int low, int high)
{
    int p;

    if (low < high) {
        p = _quick_sort(array, low, high);

        quick_sort(array, low, p-1);
        quick_sort(array, p+1, high);
    }

    return 0;
}

int main(int argc, void *argv[])
{
	int idx=0, low=0, high;
	int array[] = {0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 10, 50, 49, 38, 81, 99, 23};

    high = sizeof(array)/sizeof(int) - 1;

	quick_sort(array, low, high);

	for(idx=low; idx<=high; idx++) {
		fprintf(stdout, "array[%d] = %d\n", idx, array[idx]);
	}

	return 0;
}
