/**
 *
 * @brief Persistent connections queue by time (HTTP/1.1 keep-alive)
 *
 * Code to manage the queue of client connections through the min-heap data structure and an array
 * to store the index of the heap, in order to update the descriptor files, in case they need to update the time.
 * It may happen that they connect before they leave the queue and then the time needs to be updated.
 *
 * @version 0.2
 * @author chiqui3d
 * @date 2022-09-17
 *
 *
 * @copyright Copyright (c) 2022
 *
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../lib/logger/logger.h"
#include "helper.h"
#include "queue_connections.h"

struct QueueConnectionsType createQueueConnections() {
    struct QueueConnectionsType queueConnections;
    memset(queueConnections.connections, 0, MAX_CONNECTIONS);
    queueConnections.currentSize = 0;
    queueConnections.capacity = (int)MAX_CONNECTIONS;
    memset(queueConnections.indexQueue, -1, MAX_CONNECTIONS);
    return queueConnections;
}

void enqueueConnection(struct QueueConnectionsType *queueConnections, struct QueueConnectionElementType connection) {
    if (queueConnections->currentSize == queueConnections->capacity) {
        logWarning("Queue connection is full (max %d). The fd %d cannot be inserted\n", MAX_CONNECTIONS, connection.fd);
        return;
    }

    // insert in the last position
    queueConnections->connections[queueConnections->currentSize] = connection;
    queueConnections->currentSize++;
    int indexQueue = queueConnections->currentSize - 1; // last element
    // loop until the element is minor than its parent or it is the root
    // shiftUp
    while (indexQueue != 0 && difftime(connection.priorityTime, queueConnections->connections[parentHeap(indexQueue)].priorityTime) < 0) {
        swapConnectionElementHeap(&queueConnections->connections[indexQueue], &queueConnections->connections[parentHeap(indexQueue)]);
        indexQueue = parentHeap(indexQueue);
    }
    logDebug(GREEN "Enqueue connection fd %d in the index %d" RESET, connection.fd, indexQueue);
    // assign the index of the heap to the array
    queueConnections->indexQueue[connection.fd] = indexQueue;
}

void updateQueueConnection(struct QueueConnectionsType *queueConnections, int fd, time_t now) {

    logDebug(RED "Update queue connection fd %d" RESET, fd);

    int index = queueConnections->indexQueue[fd];
    time_t oldPriorityTime = queueConnections->connections[index].priorityTime;
    queueConnections->connections[index].priorityTime = now;

    if (difftime(now, oldPriorityTime) < 0) {
        logDebug(RED "Shift up" RESET);
        // shiftUp
        while (index != 0 && difftime(now, queueConnections->connections[parentHeap(index)].priorityTime) < 0) {
            swapConnectionElementHeap(&queueConnections->connections[index], &queueConnections->connections[parentHeap(index)]);
            index = parentHeap(index);
        }
        queueConnections->indexQueue[fd] = index;
    } else {
        logDebug(RED "Shift down" RESET);
        // shiftDown
        heapify(queueConnections, index);
    }
}

void dequeueConnection(struct QueueConnectionsType *queueConnections) {
    if (queueConnections->currentSize == 0) {
        logDebug("Queue is empty");
        return;
    }
    logDebug("Dequeue connection");

    int fd0 = queueConnections->connections[0].fd;
    int fdLast = queueConnections->connections[queueConnections->currentSize - 1].fd;
    // swap the last element with the first
    queueConnections->connections[0] = queueConnections->connections[queueConnections->currentSize - 1];
    queueConnections->connections[queueConnections->currentSize - 1] = (struct QueueConnectionElementType){0, 0};
    // remove the last element
    queueConnections->currentSize--;
    // remove the index of the heap from the array
    queueConnections->indexQueue[fd0] = -1;
    queueConnections->indexQueue[fdLast] = 0;
    // shiftDown
    heapify(queueConnections, 0);
}

void dequeueConnectionByFd(struct QueueConnectionsType *queueConnections, int fd) {
    if (queueConnections->currentSize == 0) {
        logDebug("Queue is empty");
        return;
    }
    logDebug("Dequeue connection fd %d", fd);
    int index = queueConnections->indexQueue[fd];
    if (index == -1) {
        logDebug(RED "The fd %d is not in the queue" RESET, fd);
        return;
    }
    int fdLast = queueConnections->connections[queueConnections->currentSize - 1].fd;
    // swap the last element with the first
    swapConnectionElementHeap(&queueConnections->connections[index], &queueConnections->connections[queueConnections->currentSize - 1]);
    queueConnections->connections[queueConnections->currentSize - 1] = (struct QueueConnectionElementType){0, 0};
    // remove the last element
    queueConnections->currentSize--;
    // remove the index of the heap from the array
    queueConnections->indexQueue[fd] = -1;
    queueConnections->indexQueue[fdLast] = index;
    // shiftDown
    heapify(queueConnections, index);
}

// Get the value of the front of the queue without removing it
struct QueueConnectionElementType peekQueueConnections(struct QueueConnectionsType *queueConnections) {
    if (queueConnections->currentSize == 0) {
        logDebug("Queue is empty");
        return (struct QueueConnectionElementType){0, 0};
    }

    return queueConnections->connections[0];
}

// shiftDown
void heapify(struct QueueConnectionsType *queueConnections, int index) {
    int left = leftChildHeap(index);
    int right = rightChildHeap(index);
    int smallest = index;

    if (left < queueConnections->currentSize && difftime(queueConnections->connections[left].priorityTime, queueConnections->connections[smallest].priorityTime) < 0) {
        smallest = left;
    }
    if (right < queueConnections->currentSize && difftime(queueConnections->connections[right].priorityTime, queueConnections->connections[smallest].priorityTime) < 0) {
        smallest = right;
    }
    if (smallest != index) { // if the smallest is not the current index, then swap
        swapConnectionElementHeap(&queueConnections->connections[index], &queueConnections->connections[smallest]);
        queueConnections->indexQueue[queueConnections->connections[index].fd] = index;
        queueConnections->indexQueue[queueConnections->connections[smallest].fd] = smallest;
        heapify(queueConnections, smallest);
    }
}

int leftChildHeap(int element) {
    return 2 * element + 1;
}

int rightChildHeap(int element) {
    return 2 * element + 2;
}

int parentHeap(int element) {
    return (element - 1) / 2;
}
void swapConnectionElementHeap(struct QueueConnectionElementType *a, struct QueueConnectionElementType *b) {
    struct QueueConnectionElementType temp = *a;
    *a = *b;
    *b = temp;
}

void printQueueConnections(struct QueueConnectionsType *queueConnections) {
    errno = 0;
    logDebug(RED "Size: %d, Capacity: %d" RESET, queueConnections->currentSize, queueConnections->capacity);
    int i;
    char date[20];
    for (i = 0; i < queueConnections->currentSize; i++) {
        int index = queueConnections->indexQueue[queueConnections->connections[i].fd];
        timeToDatetimeString(queueConnections->connections[i].priorityTime, date);
        // We show both the queue data and the array that stores the queue indexes,
        // to see if they match between updates and deletes.
        logDebug("index: %d, fd: %d, time: %ld, date: %s | index: %d, fd: %d, time: %ld",
                 i,
                 queueConnections->connections[i].fd,
                 queueConnections->connections[i].priorityTime,
                 date,
                 index,
                 queueConnections->connections[index].fd,
                 queueConnections->connections[index].priorityTime);
    }
    if (queueConnections->currentSize == 0) {
        logDebug(RED "Empty queue" RESET);
    }
}