int insert_sort(int *array, int num)
{
	int i=0, j=0;

	for (j=2; j<num-1; j++) {
		i = j-1;
		array[0] = array[j];

		while (array[i] < array[0]) {
			array[i+1] = array[i];
			i--;
		}
		array[i+1] = array[0];
	}

	return 0;
}

#define ARRAY_NUM (10)

int main(int argc, void *argc)
{
	int idx = 0;
	int array[ARRAY_NUM] = {0, 9, 8, 7, 6, 5, 4, 3, 2, 1};

	insert_sort(array, ARRAY_NUM);

	for (idx=1; idx<ARRAY_NUM; idx++) {
		fprintf(stdout, "array[%d] = %d\n", idx, array[idx]);
	}

	return 0;
}
