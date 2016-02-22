#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int bubble_sort(int *array, int num)
{
	int j=0, idx=0, max=0;

	for (idx=1; idx<num; idx++) {
		array[0] = array[1];

		max = num-idx+1;
		for (j=2; j<max; j++) {
			if(array[j] > array[0]) {
				array[j-1] = array[0];
				array[0] = array[j];
			}
			else {
				array[j-1] = array[j];
				array[j] = array[0];
			}
		}
		array[j-1] = array[0];
	}
	return 0;
}

int main(int argc, char *argv[])
{
	int num=10, idx=0;
	int array[] = {0, 9, 8, 7, 6, 5, 4, 3, 2, 1};

	bubble_sort(array, num);

	for (idx=1; idx<num; idx++) {
		fprintf(stdout, "array[%d] = %d\n", idx, array[idx]);
	}

	return 0;
}
