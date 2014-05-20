
#include <stdio.h>

void __attribute__((noinline)) func1(int arg1) {
	int stuff1 = 3,stuff4 = 4;

	printf("%u/%u/%u\n",stuff1,stuff4,arg1);
}

int main() {
	int alloc1 = 2,alloc2 = 4;

	printf("%u/%u\n",alloc1,alloc2);

	{
		int alloc3 = 22,alloc4 = 44;
		printf("%u/%u\n",alloc3,alloc4);
	}

	func1(99);
	return 0;
}

