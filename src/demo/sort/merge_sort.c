#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>

static int merge(int *array, int min, int mid, int max);

int merge_sort(int *array, int min, int max)
{
	int mid = (min+max)/2;

	if(max <= min) return 0;

	merge_sort(array, min, mid);
	merge_sort(array, mid+1, max);

	merge(array, min, mid, max);

	return 0;
}

int merge(int *array, int min, int mid, int max)
{
	int i=0, j=0, idx=0;
	int n1 = mid-min+1, n2 = max-mid;
	char *L=NULL, *R=NULL;

	L = calloc(1, n1*sizeof(int));
	if(NULL == L)
	{
		return -1;
	}

	R = calloc(1, n2*sizeof(int));
	if(NULL == R)
	{
		return -1;
	}

	/* Set L and R array */
	for(i=0; i<n1; i++)
	{
		L[i] = array[min+i];
	}

	for(j=0; j<n2; j++)
	{
		R[j] = array[mid+1+j];
	}

	/* Sort */
	i = 0;
	j = 0;
	idx = min;
	while(i<n1 && j<n2)
	{
		if(L[i] <= R[j])
		{
			array[idx++] = L[i++];
		}
		else
		{
			array[idx++] = R[j++];
		}

	}

	if(i < n1)
	{
		for(; i<n1; i++)
		{
			array[idx++] = L[i];
		}
	}
	else
	{
		for(; j<n2; j++)
		{
			array[idx++] = R[j];
		}
	}

	return 0;
}

#define ARRAY_NUM (10)

int main(int argc, void *argv[])
{
	int idx=0, min=0, max=ARRAY_NUM-1;
	int array[ARRAY_NUM] = {0, 9, 8, 7, 6, 5, 4, 3, 2, 1};

	merge_sort(array, min, max);

	for(idx=min; idx<ARRAY_NUM; idx++)
	{
		fprintf(stdout, "array[%d] = %d\n", idx, array[idx]);
	}

	return 0;
}
