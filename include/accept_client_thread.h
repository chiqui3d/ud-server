#ifndef ACCEPT_CLIENT_THREAD_H
#define ACCEPT_CLIENT_THREAD_H


void acceptClientsThread(int socketServerFd);
void *handleClient(void *threadDataArg);

#endif // ACCEPT_CLIENT_THREAD_H
