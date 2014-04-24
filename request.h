#ifndef __REQUEST_H__

void requestHandle(int fd);
long sizeOfFileRequested(int fd);
char*  requestedFileName(int fd);
int requestedFileNameLength(int fd);
#endif
