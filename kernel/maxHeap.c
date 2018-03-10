#include "hilevel.h"

heap* newHeap(int size) {
	heap* h = malloc(sizeof(heap));
	h->heapSize = 0;
  // h->arraySize = 0;
	h->heapArray = (pcb_t*) malloc(size * sizeof(pcb_t));
	return h;
}

int parent(int i) {
	return (i - 1)/2;
}

int left(int i) {
	return (2 * i) + 1;
}

int right(int i) {
	return (2 * i) + 2;
}

void swap(pcb_t *p1, pcb_t *p2) {
    pcb_t temp = *p1 ;
    *p1 = *p2 ;
    *p2 = temp ;
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
		swap(&h->heapArray[i], &h->heapArray[largest]);
    maxHeapify(h, largest);
	}
}

void buildMaxHeap(heap* h) {
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
				swap(&h->heapArray[i], &h->heapArray[parent(i)]);
        i = parent(i);
    }
}

void heapInsert(heap* h, pcb_t key) {
    h->heapSize += 1;
    // h->arraySize += 1;
    h->heapArray[h->heapSize - 1].priority = INT_MIN;
    heapIncreaseKey(h, h->heapSize - 1, key);
}

void heapSort(heap* h) {
    buildMaxHeap(h);
    for (int i = h->heapSize - 1; i > 0; i--) {
				swap(&h->heapArray[i], &h->heapArray[0]);
        pcb_t tmp = h->heapArray[i];
        h->heapArray[i] = h->heapArray[0];
        h->heapArray[0] = tmp;
        h->heapSize -= 1;
        maxHeapify(h, 0);
    }
}
