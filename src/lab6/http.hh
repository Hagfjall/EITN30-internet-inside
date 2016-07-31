#ifndef http_hh
#define http_hh

/****************** INCLUDE FILES SECTION ***********************************/

#include "job.hh"
#include "timer.hh"
#include "threads.hh"
#include "fs.hh"

class HTTPServer : public Job {

public:
    HTTPServer(TCPSocket *theSocket);
    // Constructor. The application is created by class TCP when a connection is
    // established.

    void doit();
    // Gets called when the application thread begins execution.
    // The SimpleApplication job is scheduled by TCP when a connection is
    // established.

    char *extractString(char *thePosition, udword theLength);

    udword contentLength(char *theData, udword theLength);

    char *decodeBase64(char *theEncodedString);

    char *decodeForm(char *theEncodedForm);

    void handleHeader(byte *theData, udword theLength);

    bool gotWholeHeader(byte *theData, udword theLength);

    byte *allocate(udword length);

    char *findPathName(char *str);

    bool userAuthenticated(char *header);

private:
    TCPSocket *mySocket;
    // Pointer to the application associated with this job.
    Semaphore *myAllocateMemorySem;
    AllocateMemoryTimer *myMemoryTimer;
    // FileSystem* 		myFileSystem;
};


#endif
