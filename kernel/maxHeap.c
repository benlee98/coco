#include "hilevel.h"

heap* newHeap() {
	heap* h = malloc(sizeof(heap));
	h->heapSize = 0;
    h->arraySize = 0;
	h->heapArray = (pcb_t*) malloc(sizeof(pcb_t));
	return h;
}

int parent(int i) {
	return i/2;
}

int left(int i) {
	return 2 * i;
}

int right(int i) {
	return (2 * i) + 1;
}

void maxHeapify(heap* h, int i) {
	int largest;
	int l = left(i);
	int r = right(i);

    if((l <= h->heapSize - 1) && (h->heapArray[l].priority > h->heapArray[i].priority)) {
        largest = l;
    } else {
        largest = i;
    }
    if ((r <= h->heapSize - 1) && (h->heapArray[r].priority > h->heapArray[largest].priority)) {
        largest = r;
    }
	if (largest != i) {
		pcb_t tmp = h->heapArray[i];
		h->heapArray[i] = h->heapArray[largest];
		h->heapArray[largest] = tmp;
    maxHeapify(h, largest);
	}
}

heap* buildMaxHeap(heap* h) {
	for (int i = ((h->heapSize - 1)/2); i >= 0; i--) {
		maxHeapify(h, i);
	}
}

void heapIncreaseKey(heap* h, int i, pcb_t key) {
//    if (key < h->heapArray[i]) {
//        ;
//    }
    h->heapArray[i] = key;
    while (i > 0 &&  h->heapArray[parent(i)].priority < h->heapArray[i].priority) {
        pcb_t tmp = h->heapArray[i];
        h->heapArray[i] = h->heapArray[parent(i)];
        h->heapArray[parent(i)] = tmp;
        i = parent(i);
    }
}

void heapInsert(heap* h, pcb_t key) {
    h->heapSize += 1;
    h->arraySize += 1;
    h->heapArray[h->heapSize - 1].priority = INT_MIN;
    heapIncreaseKey(h, h->heapSize - 1, key);
}

void heapSort(heap* h) {
    buildMaxHeap(h);
    for (int i = h->heapSize - 1; i > 0; i--) {
        pcb_t tmp = h->heapArray[i];
        h->heapArray[i] = h->heapArray[0];
        h->heapArray[0] = tmp;
        h->heapSize -= 1;
        maxHeapify(h, 0);
    }
}
