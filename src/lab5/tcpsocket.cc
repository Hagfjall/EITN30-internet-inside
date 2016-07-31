#include "compiler.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern "C"
{
#include "system.h"
#include "timr.h"
}

#include "iostream.hh"
#include "tcp.hh"
#include "ip.hh"
#include "tcpsocket.hh"
#include "threads.hh"

#define D_TCPSOCKET
#ifdef D_TCPSOCKET
#define trace cout
#else
#define trace if(false) cout
#endif

TCPSocket::TCPSocket(TCPConnection *theConnection) : myConnection(theConnection),
                                                     myReadSemaphore(Semaphore::createQueueSemaphore("myReadSem", 0)),
                                                     myWriteSemaphore(
                                                             Semaphore::createQueueSemaphore("myWriteSemaphorez", 0)) {
}

TCPSocket::~TCPSocket() {
    delete myReadSemaphore;
    delete myWriteSemaphore;

    //TODO maybe delete myConnection as well? MyReadData
}


byte *
TCPSocket::Read(udword &theLength) {
    // cout << "read wait" << endl;
    myReadSemaphore->wait(); // Wait for available data
    // cout << "read wait finished" << endl;
    theLength = myReadLength;
    byte *aData = myReadData;
    myReadLength = 0;
    myReadData = 0;
    return aData;
}

bool TCPSocket::isEof() {
    return eofFound;
}

void
TCPSocket::socketDataReceived(byte *theData, udword theLength) {
    while (ax_coreleft_total() < theLength) {
        os_delay(1);
    }
    myReadData = new byte[theLength];
    memcpy(myReadData, theData, theLength);
    myReadLength = theLength;
    myReadSemaphore->signal(); // Data is available
}

void
TCPSocket::Write(byte *theData, udword theLength) {
    // cout << "TCPSocket::Write " << endl;
    myConnection->Send(theData, theLength);
    // cout << "TCPSocket::Write wait " << endl;
    myWriteSemaphore->wait(); // Wait until the data is acknowledged
    // cout << "TCPSocket::Write wait finished " << endl;
}

void
TCPSocket::socketDataSent() {
    myWriteSemaphore->signal(); // The data has been acknowledged
}


void
TCPSocket::socketEof() {
    eofFound = true;
    myReadSemaphore->signal();
}

void TCPSocket::Close() {
    // myConnection->myState = CloseWaitState::instance();
    myConnection->AppClose();
}

SimpleApplication::SimpleApplication(TCPSocket *theSocket) : mySocket(theSocket) {
    myTimerSemaphore = Semaphore::createQueueSemaphore("myTimerSemaphore", 0);
    myMemoryTimer = new AllocateMemoryTimer(myTimerSemaphore);

}

void SimpleApplication::doit() {
    udword aLength;
    byte *aData;
    bool done = false;
    while (!done && !mySocket->isEof()) {
        aData = mySocket->Read(aLength);
        if (aLength > 0) {
            if ((char) *aData == 'q' && aLength == 3) {
                // cout << "done = true" << endl;
                done = true;
            }
            else if ((char) *aData == 's' && aLength == 3) {
                udword totaltLength = 10240;
                delete[] aData;
                aData = new byte[totaltLength];
                while (aData == NULL) {
                    myMemoryTimer->start();
                    myTimerSemaphore->wait();
                    delete[] aData;
                    aData = new byte[totaltLength];
                }
                generateBigData(totaltLength, aData);
                // cout << "s response " << endl;
                mySocket->Write(aData, totaltLength);
                // cout << "s response finished" << endl;
            }
            else if ((char) *aData == 'a' && aLength == 3) {
                udword totaltLength = 1024;
                delete[] aData;
                aData = new byte[totaltLength];
                while (aData == NULL) {
                    myMemoryTimer->start();
                    myTimerSemaphore->wait();
                    delete[] aData;
                    aData = new byte[totaltLength];
                }
                generateBigData(totaltLength, aData);
                // cout << "a response " << endl;
                mySocket->Write(aData, totaltLength);
                // cout << "a response finished" << endl;
            }
            else if ((char) *aData == 'r' && aLength == 3) {
                udword totaltLength = 1048576;
                delete[] aData;
                aData = new byte[totaltLength];
                while (aData == NULL) {
                    myMemoryTimer->start();
                    myTimerSemaphore->wait();
                    delete[] aData;
                    aData = new byte[totaltLength];
                }
                generateBigData(totaltLength, aData);
                // cout << "r response " << endl;
                mySocket->Write(aData, totaltLength);
                // cout << "r response finished" << endl;
            } else {//REPLY
                mySocket->Write(aData, aLength);
            }
            delete[] aData;
        }
    }
    delete myTimerSemaphore;
    delete myMemoryTimer;
    mySocket->Close();
}

void SimpleApplication::generateBigData(udword theLength, byte *theData) {
    byte character = 'A';
    for (udword i = 0; i != theLength; ++i) {
        theData[i] = character++;
        if (character == 'Z' + 1) {
            character = 'A';
        }
    }
    theData[theLength] = '\0';

}

AllocateMemoryTimer::AllocateMemoryTimer(Semaphore *theSemaphore) : mySemaphore(theSemaphore), myCounter(rand() % 255) {

}


void AllocateMemoryTimer::start() {
    this->timeOutAfter(Clock::tics * (rand() % 30 + 10));
}

void AllocateMemoryTimer::timeOut() {
    mySemaphore->signal();
}




