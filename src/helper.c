#include <ctype.h>      // for tolower()
#include <errno.h>      // for errno
#include <fcntl.h>      // for fcntl() nonblocking socket
#include <stdio.h>      // for perror()
#include <stdlib.h>     // for malloc()
#include <string.h>     // for strlen()
#include <sys/socket.h> // for send()

#include "../lib/die/die.h"
#include "../lib/logger/logger.h"
#include "helper.h"

int makeSocketNonBlocking(int fd) {
    int flags, s;

    flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl");
        return -1;
    }

    flags |= O_NONBLOCK;
    s = fcntl(fd, F_SETFL, flags);
    if (s == -1) {
        perror("fcntl");
        return -1;
    }

    return 0;
}

void strCopySafe(char *dest, char *src) {
    size_t srcLen = strlen(src);
    strncpy(dest, src, srcLen + 1);
    dest[srcLen] = '\0';
}

char *timeToDatetimeString(time_t time, char *format) {
    struct tm *timeInfo = localtime(&time);
    strftime(format, DATETIME_HELPER_SIZE, DATETIME_HELPER_FORMAT, timeInfo);
    return format;
}

char *toLower(char *str, size_t len) {

    char *strLower = calloc(len + 1, sizeof(char));

    for (size_t i = 0; i < len; i++) {
        strLower[i] = tolower((unsigned char)str[i]);
    }
    return strLower;
}
char *toUpper(char *str, size_t len) {
    char *strUpper = calloc(len + 1, sizeof(char));

    for (size_t i = 0; i < len; i++) {
        strUpper[i] = toupper((unsigned char)str[i]);
    }
    return strUpper;
}

char *readAll(int fd, char *buffer, size_t bufferSize) {

    int totalBytesRead = 0;
    int restBytesRead = bufferSize - 1; // for add null terminator char
    while (restBytesRead > 0) {         // MSG_PEEK
        ssize_t bytesRead = recv(fd, buffer + totalBytesRead, restBytesRead, 0);
        if (bytesRead < 0) {
            if (errno == EINTR) {
                // interrupted by a signal before any data was read
                continue;
            }
            // Ignore EWOULDBLOCK|EAGAIN, it not mean you're disconnected, it just means there's nothing to read now
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                 break;
            }
            logError("recv() request failed");
           
        } else if (bytesRead == 0) {
            // client disconnected
            break;
        } else {
            totalBytesRead += bytesRead;
            restBytesRead -= bytesRead;
        }
    }
    buffer[totalBytesRead] = '\0';

    return buffer;
}

size_t sendAll(int fd, char *buffer, size_t count) {
    size_t leftToWrite = count;
    while (leftToWrite > 0) {
        size_t written = send(fd, buffer, count, 0);
        if (written == -1) {
            if (errno == EINTR) {
                // The send call was interrupted by a signal
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return written;
            }
            logError("sendAll() error");
            return -1;
        } else {
            leftToWrite -= written;
        }
    }

    return count;
}