/*!***************************************************************************
*!
*! FILE NAME  : tcp.cc
*!
*! DESCRIPTION: TCP, Transport control protocol
*!
*!***************************************************************************/

/****************** INCLUDE FILES SECTION ***********************************/

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
#include "http.hh"

#define D_TCP
#ifdef D_TCP
#define trace cout
#else
#define trace if(false) cout
#endif
/****************** TCP DEFINITION SECTION *************************/
//----------------------------------------------------------------------------
//
static udword counter = 0;

TCP::TCP() {
    // trace << "TCP created." << endl;
}

//----------------------------------------------------------------------------
//
TCP &
TCP::instance() {
    static TCP myInstance;
    return myInstance;
}

//----------------------------------------------------------------------------
//
TCPConnection *
TCP::getConnection(IPAddress &theSourceAddress,
                   uword theSourcePort,
                   uword theDestinationPort) {
    TCPConnection *aConnection = NULL;
    // Find among open connections
    uword queueLength = myConnectionList.Length();
    myConnectionList.ResetIterator();
    bool connectionFound = false;
    while ((queueLength-- > 0) && !connectionFound) {
        aConnection = myConnectionList.Next();
        connectionFound = aConnection->tryConnection(theSourceAddress,
                                                     theSourcePort,
                                                     theDestinationPort);
    }
    if (!connectionFound) {
        // trace << "Connection not found!" << endl;
        aConnection = NULL;
    }
    else {
        // trace << "Found connection in queue" << endl;
    }
    return aConnection;
}

//----------------------------------------------------------------------------
//
TCPConnection *
TCP::createConnection(IPAddress &theSourceAddress,
                      uword theSourcePort,
                      uword theDestinationPort,
                      InPacket *theCreator) {
    TCPConnection *aConnection = new TCPConnection(theSourceAddress,
                                                   theSourcePort,
                                                   theDestinationPort,
                                                   theCreator);
    myConnectionList.Append(aConnection);
    return aConnection;
}

//----------------------------------------------------------------------------

void
TCP::deleteConnection(TCPConnection *theConnection) {
// cout <<"deleteconnection portnr " << theConnection->hisPort << " # of connections: " << TCP::instance().myConnectionList.Length() <<  endl;
    myConnectionList.Remove(theConnection);
    delete theConnection;
// cout <<"number of connections now after delete: " << TCP::instance().myConnectionList.Length() << endl;
}

bool
TCP::acceptConnection(uword portNo) {
    return portNo == 7 || portNo == 80;
}

// Is true when a connection is accepted on port portNo.
void
TCP::connectionEstablished(TCPConnection *theConnection) {
    // cout << "TCP connectionEstablished connector port: " <<  theConnection->hisPort << endl;
    if (theConnection->serverPortNumber() == 7) {
        TCPSocket *aSocket = new TCPSocket(theConnection);
        theConnection->registerSocket(aSocket);
        Job::schedule(new SimpleApplication(aSocket));
        // Create and start an application for the connection.
    } else if (theConnection->serverPortNumber() == 80) {
        TCPSocket *aSocket = new TCPSocket(theConnection);
        theConnection->registerSocket(aSocket);
        Job::schedule(new HTTPServer(aSocket));
    }
}
// Create a new TCPSocket. Register it in TCPConnection.
// Create and start a SimpleApplication.

//----------------------------------------------------------------------------
//
retransmitTimer::retransmitTimer(TCPConnection *theConnection,
                                 Duration retransmitTime) : myConnection(theConnection),
                                                            myRetransmitTime(retransmitTime) { }

retransmitTimer::~retransmitTimer() {

}

void retransmitTimer::start() {
    // cout << "Starting retransmitttimer" << endl;
    this->timeOutAfter(myRetransmitTime);
}

void retransmitTimer::cancel() {
    this->resetTimeOut();
}

void retransmitTimer::timeOut() {
    // cout << "retransmitttimer timeOut for " << myConnection->hisPort << endl;
    myConnection->sendNext = myConnection->sentUnAcked;
    myConnection->myTCPSender->sendFromQueue();

}

connectionTimeoutTimer::connectionTimeoutTimer(TCPConnection *theConnection, Duration timeoutTime) : myConnection(
        theConnection), myTimeoutTime(timeoutTime) { }

connectionTimeoutTimer::~connectionTimeoutTimer() {

}

void connectionTimeoutTimer::start() {
    // cout << "Timeouttimer started on #" << myConnection->hisPort << endl;
    this->timeOutAfter(myTimeoutTime);

}

void connectionTimeoutTimer::cancel() {

    // cout << "Timeouttimer cancled on #" << myConnection->hisPort << endl;
    this->resetTimeOut();
}

void connectionTimeoutTimer::timeOut() {
    // cout << "Timeouttimer sending RST-flag on #" << myConnection->hisPort << endl;
    if (myConnection->myPort == 80) {
        myConnection->receiveNext -= 1;
        myConnection->myTCPSender->sendFlags(0x14);
        myConnection->Kill();
    }
}

TCPConnection::TCPConnection(IPAddress &theSourceAddress,
                             uword theSourcePort,
                             uword theDestinationPort,
                             InPacket *theCreator) :
        hisAddress(theSourceAddress),
        hisPort(theSourcePort),
        myPort(theDestinationPort) {
    // trace << "TCP connection created" << endl;
    myTCPSender = new TCPSender(this, theCreator),
            myState = ListenState::instance();
    timer = new retransmitTimer(this, Clock::seconds);
    timeOutTimer = new connectionTimeoutTimer(this, Clock::seconds * 5);
    RSTflag = false;
    mySocket = NULL;
}

//----------------------------------------------------------------------------
//
TCPConnection::~TCPConnection() {
    // cout << "TCP connection destroyed" << endl;
    if (mySocket) {
        mySocket->socketDataSent();
        mySocket->socketEof();
        delete mySocket;
    }
    delete myTCPSender;
    // cout << "delete timer" << endl;
    delete timer;
    delete timeOutTimer;
    // cout << "TCP connection destroyed finished" << endl;
}

//----------------------------------------------------------------------------
//
bool
TCPConnection::tryConnection(IPAddress &theSourceAddress,
                             uword theSourcePort,
                             uword theDestinationPort) {
    return (theSourcePort == hisPort) &&
           (theDestinationPort == myPort) &&
           (theSourceAddress == hisAddress);
}


// TCPConnection cont...

// Handle an incoming SYN segment
void TCPConnection::Synchronize(udword theSynchronizationNumber) {
    timeOutTimer->start();
    myState->Synchronize(this, theSynchronizationNumber);


}

// Handle an incoming FIN segment
void TCPConnection::NetClose() {
    // cout << "TCPConnection::NetClose" << endl;
    myState->NetClose(this);

}

// Handle close from application
void TCPConnection::AppClose() {
    // cout << "TCPConnection::AppClose" << endl;
    myState->AppClose(this);
}

// Handle an incoming RST segment, can also called in other error conditions
void TCPConnection::Kill() {
    // cout <<"KILL on #" << hisPort << " with " << TCP::instance().myConnectionList.Length() << " nbr of connections" << endl;
    if (mySocket) {
        mySocket->socketEof();
    }
// cout <<"Kill after SOCKETEOF #" << hisPort << " with " << TCP::instance().myConnectionList.Length() << " nbr of connections" << endl;
    myState->Kill(this);
// cout <<"TCPConnection::Kill finished" << endl;
}

// Handle incoming data
void TCPConnection::Receive(udword theSynchronizationNumber,
                            byte *theData,
                            udword theLength) {
    myState->Receive(this, theSynchronizationNumber, theData, theLength);
}

// Handle incoming Acknowledgement
void TCPConnection::Acknowledge(udword theAcknowledgementNumber) {
    timeOutTimer->cancel();
    timeOutTimer->start();
    if (sentUnAcked < theAcknowledgementNumber) {
        sentUnAcked = theAcknowledgementNumber;
        // cout << "sentUnAcked " << sentUnAcked << " sentMaxSeq " << sentMaxSeq << endl;
        if (sendNext < sentMaxSeq) {
            sendNext = sentUnAcked;
        }

        if (sentUnAcked == sentMaxSeq) {
            timer->cancel();
        }
        myState->Acknowledge(this, theAcknowledgementNumber);
    }

}

// Send outgoing data
void TCPConnection::Send(byte *theData,
                         udword theLength) {

    myState->Send(this, theData, theLength);

    //timer->start();
    // myState->Send(this, theData, theLength);
}

uword TCPConnection::serverPortNumber() {
    return myPort;
}

// Return myPort.
void TCPConnection::registerSocket(TCPSocket *theSocket) {
    mySocket = theSocket;

}  // Set mySocket to theSocket. 

//----------------------------------------------------------------------------
// TCPState contains dummies for all the operations, only the interesting ones
// gets overloaded by the various sub classes.

//----------------------------------------------------------------------------
//
void
TCPState::Kill(TCPConnection *theConnection) {
    // cout << "TCPState::Kill" << endl;

    TCP::instance().deleteConnection(theConnection);
}

void
TCPState::Synchronize(TCPConnection *theConnection,
                      udword theSynchronizationNumber) {

}

void TCPState::NetClose(TCPConnection *theConnection) { }

void TCPState::AppClose(TCPConnection *theConnection) { }

void TCPState::Receive(TCPConnection *theConnection,
                       udword theSynchronizationNumber,
                       byte *theData,
                       udword theLength) { }

void TCPState::Acknowledge(TCPConnection *theConnection,
                           udword theAcknowledgementNumber) { }

void TCPState::Send(TCPConnection *theConnection,
                    byte *theData,
                    udword theLength) { }


//----------------------------------------------------------------------------
//
ListenState *
ListenState::instance() {
    static ListenState myInstance;
    return &myInstance;
}

void ListenState::Synchronize(TCPConnection *theConnection,
                              udword theSynchronizationNumber) {

    theConnection->receiveNext = theSynchronizationNumber + 1;
    theConnection->receiveWindow = 8 * 1024;
    theConnection->sendNext = get_time();
    theConnection->sentMaxSeq = theConnection->sendNext;
    // Next reply to be sent
    theConnection->sentUnAcked = theConnection->sendNext;
    // Send a segment with the SYN and ACK flags set
    theConnection->myTCPSender->sendFlags(0x12);
    // Change state
    theConnection->myState = SynRecvdState::instance();
}


//----------------------------------------------------------------------------
//
SynRecvdState *
SynRecvdState::instance() {
    static SynRecvdState myInstance;
    return &myInstance;
}


void SynRecvdState::Acknowledge(TCPConnection *theConnection,
                                udword theAcknowledgementNumber) {
    theConnection->myState = EstablishedState::instance();
    TCP::instance().connectionEstablished(theConnection);
}


//----------------------------------------------------------------------------
//
EstablishedState *
EstablishedState::instance() {
    static EstablishedState myInstance;
    return &myInstance;
}

void
EstablishedState::AppClose(TCPConnection *theConnection) {

    theConnection->myTCPSender->sendFlags(0x11);

    // Switch to Fin Wait 1 state
    theConnection->myState = FinWait1State::instance();
}

void EstablishedState::Acknowledge(TCPConnection *theConnection,
                                   udword theAcknowledgementNumber) {
    if (theConnection->theOffset >= theConnection->queueLength &&
        theAcknowledgementNumber == theConnection->firstSeq + theConnection->theOffset) {
        // TODO: Reset state variables
        // cout << "AN ACK BAAD  #" << theAcknowledgementNumber << " theOffset = " << theConnection->theOffset << " queueLength = " << theConnection->queueLength << endl;
        theConnection->mySocket->socketDataSent();
    } else {
        theConnection->myTCPSender->sendFromQueue();
    }

}

void EstablishedState::Send(TCPConnection *theConnection,
                            byte *theData,
                            udword theLength) {
    theConnection->transmitQueue = theData;
    theConnection->queueLength = theLength;
    theConnection->firstSeq = theConnection->sendNext;
    theConnection->theOffset = 0;
    theConnection->theSendLength = 1460;
    // cout << "EstablishedState::Send with queueLength= " << theConnection->queueLength << endl;

    theConnection->myTCPSender->sendFromQueue();

}

//----------------------------------------------------------------------------
//
void
EstablishedState::NetClose(TCPConnection *theConnection) {
    // trace << "EstablishedState::NetClose" << endl;

    // Update connection variables and send an ACK
    // cout << "trying to send ACK on FIN "<<endl;

    theConnection->receiveNext += 1;

    theConnection->myTCPSender->sendFlags(0x10); //sending an ACK
    // Go to NetClose wait state, inform application
    theConnection->myState = CloseWaitState::instance();
    // theConnection->mySocket->socketEof();
    // Normally the application would be notified next and nothing
    // happen until the application calls appClose on the connection.
    // Since we don't have an application we simply call appClose here instead.

    // Simulate application Close...
    theConnection->AppClose();
}

//----------------------------------------------------------------------------
//
void
EstablishedState::Receive(TCPConnection *theConnection,
                          udword theSynchronizationNumber,
                          byte *theData,
                          udword theLength) {
    if (theSynchronizationNumber == theConnection->receiveNext) {
        theConnection->receiveNext += (theLength - TCP::tcpHeaderLength);
        theConnection->myTCPSender->sendFlags(0x10); // Sending an ACK

        theConnection->mySocket->socketDataReceived(theData + TCP::tcpHeaderLength, theLength - TCP::tcpHeaderLength);
    }
    // TODO maybe add this!
    /*else if (theSynchronizationNumber > theConnection->receiveNext) { // Some packet got lost or arrived in wrong order
      theConnection->mySocket->socketDataReceived(0, 0);
    }
    */
}

//----------------------------------------------------------------------------
//
CloseWaitState *
CloseWaitState::instance() {
    static CloseWaitState myInstance;
    return &myInstance;
}

void CloseWaitState::AppClose(TCPConnection *theConnection) {

    theConnection->myState = LastAckState::instance();
    theConnection->mySocket->socketEof();
    theConnection->myTCPSender->sendFlags(0x11); //sending a FIN, ACK 
    // theConnection->sendNext += 1;
}


//----------------------------------------------------------------------------
//
LastAckState *
LastAckState::instance() {
    static LastAckState myInstance;
    return &myInstance;
}

void LastAckState::Acknowledge(TCPConnection *theConnection,
                               udword theAcknowledgementNumber) {
    // cout <<"LastAckState::Acknowledge sentUnAcked: " << theConnection->sentUnAcked
    // << " theAcknowledgementNumber " << theAcknowledgementNumber << endl;
    theConnection->sentUnAcked = theAcknowledgementNumber;
    if (theConnection->sendNext == theAcknowledgementNumber) {
        // theConnection->myTCPSender->sendFlags(0x10);
        theConnection->Kill();
    } else {
        // cout << "LastAckState:Acknowledge but not in if, sendNext=" << theConnection->sendNext
        // << " theAcknowledgementNumber = " << theAcknowledgementNumber << endl;
    }
}

FinWait1State *
FinWait1State::instance() {
    static FinWait1State myInstance;
    return &myInstance;
}

void FinWait1State::Acknowledge(TCPConnection *theConnection,
                                udword theAcknowledgementNumber) {
    // cout << "FinWait1State::Acknowledge " << theAcknowledgementNumber << " = " << theConnection->sendNext << endl;
    if (theAcknowledgementNumber == theConnection->sendNext) {
        theConnection->receiveNext += 1;
        theConnection->myState = FinWait2State::instance();

    } else {
        // cout << "FinWait1State:Acknowledge wrong theAcknowledgementNumber=" << theAcknowledgementNumber
        // << " and sendNext=" << theConnection->sendNext << endl;
    }
}

FinWait2State *
FinWait2State::instance() {
    static FinWait2State myInstance;
    return &myInstance;
}

void FinWait2State::NetClose(TCPConnection *theConnection) {
    // cout <<" FinWait2State NetClose" << endl;
    theConnection->myTCPSender->sendFlags(0x10);
    theConnection->mySocket->socketEof();
    theConnection->Kill();
}

//----------------------------------------------------------------------------
//
TCPSender::TCPSender(TCPConnection *theConnection,
                     InPacket *theCreator) :
        myConnection(theConnection),
        myAnswerChain(theCreator->copyAnswerChain()) // Copies InPacket chain!
{
}

//----------------------------------------------------------------------------
//

void TCPSender::sendFlags(byte theFlags) {
    if (myConnection->RSTflag) {
        // cout << "Returning from sendFlags due to RSTFlag true" << endl;
        return;
    }
// Decide on the value of the length totalSegmentLength. 
// Allocate a TCP segment.
    byte *anAnswer = new byte[myAnswerChain->headerOffset()];
// cout << "Size of packet: " << myAnswerChain->headerOffset() << endl;
    anAnswer = anAnswer + myAnswerChain->headerOffset() - TCP::tcpHeaderLength;
// Calculate the pseudo header checksum 
    TCPPseudoHeader *aPseudoHeader =
            new TCPPseudoHeader(myConnection->hisAddress,
                                TCP::tcpHeaderLength);
    uword pseudosum = aPseudoHeader->checksum();
    delete aPseudoHeader;
// Create the TCP segment. 
// Calculate the final checksum. 
    TCPHeader *aTCPHeader = (TCPHeader *) anAnswer;
    aTCPHeader->checksum = 0;


// Send the TCP segment.
    aTCPHeader->sourcePort = HILO(myConnection->myPort);
    aTCPHeader->destinationPort = HILO(myConnection->hisPort);
    aTCPHeader->sequenceNumber = LHILO(myConnection->sendNext);
    aTCPHeader->acknowledgementNumber = LHILO(myConnection->receiveNext);


    aTCPHeader->headerLength = 0x50; //since 6 bits are shitty
    aTCPHeader->flags = theFlags;
    aTCPHeader->windowSize = HILO(myConnection->receiveWindow);
    aTCPHeader->urgentPointer = 0;
    aTCPHeader->checksum = calculateChecksum(anAnswer,
                                             TCP::tcpHeaderLength,
                                             pseudosum);
    // cout << "sending flags" << endl;
    // Ethernet::instance().bytesOffsetPrintOut(anAnswer, 0, 20);
    myAnswerChain->answer(anAnswer, myAnswerChain->headerOffset());
    // Deallocate the dynamic memory
    if (theFlags != 0x010) {
        myConnection->sendNext += 1;
        myConnection->sentMaxSeq = myConnection->sendNext;
    }
    delete anAnswer;
}

void TCPSender::sendData(byte *theData, udword theLength) {

    if (myConnection->RSTflag) {
        // cout << "Returning from sendData due to RSTFlag true" << endl;
        return;
    }


    byte *anAnswer = new byte[myAnswerChain->headerOffset() + theLength];
    // cout << "anAnswer ";
    // Ethernet::instance().bytesOffsetPrintOut(anAnswer,0,0);
    // cout << "theData ";
    // Ethernet::instance().bytesOffsetPrintOut(theData,0,20);
    TCPPseudoHeader *aPseudoHeader = new TCPPseudoHeader(myConnection->hisAddress,
                                                         theLength + TCP::tcpHeaderLength);
    uword pseudosum = aPseudoHeader->checksum();
    delete aPseudoHeader;
    anAnswer = anAnswer + myAnswerChain->headerOffset() - TCP::tcpHeaderLength;
    TCPHeader *aTCPHeader = (TCPHeader *) anAnswer;
    aTCPHeader->checksum = 0;
    aTCPHeader->sourcePort = HILO(myConnection->myPort);
    aTCPHeader->destinationPort = HILO(myConnection->hisPort);
    aTCPHeader->sequenceNumber = LHILO(myConnection->sendNext);
    aTCPHeader->acknowledgementNumber = LHILO(myConnection->receiveNext);
    aTCPHeader->headerLength = 0x50; //since 6 bits are shitty
    aTCPHeader->flags = 0x18; //PSH-ACK always set. Can change so PSH is only set when max packet size is not achieved.
    aTCPHeader->windowSize = HILO(myConnection->receiveWindow);
    aTCPHeader->urgentPointer = 0;

    memcpy((byte * )(aTCPHeader + 1), theData, theLength);
    // cout << "SendData with length  " << theLength << endl;

    // Ethernet::instance().bytesOffsetPrintOut((byte*) (aTCPHeader+1),theLength-20,20);
    aTCPHeader->checksum = calculateChecksum(anAnswer,
                                             theLength + TCP::tcpHeaderLength,
                                             pseudosum);
    myConnection->timer->start();


    myConnection->sendNext += theLength;
    if (myConnection->sentMaxSeq < myConnection->sendNext) {
        myConnection->sentMaxSeq = myConnection->sendNext;
    } else {
        // cout << "TCPSender:sendData wont set sentMaxSeq since its higher than sendNext, perhaps retransmitt?" << endl;
    }

    // if(counter++ % 30 == 0){ // SHOULDNT BE USED IN PRODUCTION
    // cout <<"dropping packet! sendNext: " <<  myConnection->sendNext << endl;
    //   delete[] anAnswer;
    //   return;
    // }


    myAnswerChain->answer(anAnswer, myAnswerChain->headerOffset() + theLength);

    delete[] anAnswer;

}

void TCPSender::sendFromQueue() {
    // cout << "sendFromQueue called " << endl;
    udword theWindowSize = myConnection->myWindowSize - (myConnection->sendNext - myConnection->sentUnAcked);
    if (theWindowSize > myConnection->myWindowSize) {
        theWindowSize = 0;
    }

    udword dataToSend = myConnection->queueLength - myConnection->theOffset;

    // Check if we have to retransmitt a segment
    // cout << "Checking if segment has to be retransmitted on #" << myConnection->hisPort << "with sendnext < sentmaxseq: " << myConnection->sendNext << " < " << myConnection->sentMaxSeq << endl;
    if (myConnection->sendNext < myConnection->sentMaxSeq) {

        udword leftLengthToMaxSeq = myConnection->sentMaxSeq - myConnection->sendNext;


        udword lengthToRetransmitt = leftLengthToMaxSeq < myConnection->theSendLength ?
                                     leftLengthToMaxSeq : myConnection->theSendLength;
        // cout << "Segment has to be retransmitted on #" << myConnection->hisPort << endl;
        sendData(myConnection->transmitQueue + (myConnection->sendNext - myConnection->firstSeq), lengthToRetransmitt);
    }

    else if (dataToSend < myConnection->theSendLength && dataToSend > 0) {
        // cout << "sendFromQueue not whole packet sendNext" << myConnection->sendNext << " and length " << dataToSend << endl;
        sendData(myConnection->transmitQueue + myConnection->theOffset, dataToSend);
        myConnection->theOffset += dataToSend;
    } else {
        udword segmentsToSend = MIN(dataToSend, theWindowSize) / myConnection->theSendLength;
        udword dataleft = dataToSend - (myConnection->theSendLength * segmentsToSend);
        dataleft = MIN(dataleft, theWindowSize);

        for (udword i = 0; i < segmentsToSend; ++i) {
            // cout << "sendFromQueue wrapped, #" << i << endl;
            // Ethernet::instance().bytesOffsetPrintOut(myConnection->transmitQueue + myConnection->theOffset,myConnection->theSendLength-20,20);
            sendData(myConnection->transmitQueue + myConnection->theOffset, myConnection->theSendLength);
            myConnection->theOffset += myConnection->theSendLength;
        }
        if (MIN(dataToSend, theWindowSize) == dataToSend && dataleft > 0) {
            // cout << "sendFromQueue rest of current sendNext" << myConnection->sendNext << " and length " << dataleft << endl;
            sendData(myConnection->transmitQueue + myConnection->theOffset, dataleft);
            myConnection->theOffset += dataleft;
        }
    }
}

TCPSender::~TCPSender() {
// cout <<"deleting TCP Sender" << endl;
    myAnswerChain->deleteAnswerChain();
}


//----------------------------------------------------------------------------
//
TCPInPacket::TCPInPacket(byte *theData,
                         udword theLength,
                         InPacket *theFrame,
                         IPAddress &theSourceAddress) :
        InPacket(theData, theLength, theFrame),
        mySourceAddress(theSourceAddress) {
}

//----------------------------------------------------------------------------
//

// IMPLEMENTERA
void TCPInPacket::decode() {
// Extract the parameters from the TCP header which define the 
    // connection.
    TCPHeader *tcpheader = (TCPHeader *) myData;
    myDestinationPort = HILO(tcpheader->destinationPort);
    mySourcePort = HILO(tcpheader->sourcePort);
    mySequenceNumber = LHILO(tcpheader->sequenceNumber);
    myAcknowledgementNumber = LHILO(tcpheader->acknowledgementNumber);
    byte headerLengthInBytes = ((tcpheader->headerLength & 0xF0) >> 4) * 4;
// cout <<endl << "TCPInPacket::decode portnr " << mySourcePort << " mySequenceNumber: "
    // << mySequenceNumber << "length of packet is " << myLength << endl;
    // cout << "Free mem " << ax_coreleft_total() << " bytes" << endl;

    // cout << "TCP Port: "<< myDestinationPort << endl;
    // Ethernet::instance().bytesOffsetPrintOut(tcpheader,0,40);




    TCPConnection *aConnection =
            TCP::instance().getConnection(mySourceAddress,
                                          mySourcePort,
                                          myDestinationPort);
    aConnection->myWindowSize = HILO(tcpheader->windowSize);
    // cout << "Trying to decode TCPInPacket" << endl;
    if (!aConnection) {
        if (TCP::instance().myConnectionList.Length() > 10) {
            // cout <<"MORE THAN 20 CONNECTIONS, return!" << endl;
            // aConnection->Kill();
            return;
        }
        // Establish a new connection.
        // cout << "Creating TCPConnection" << "on port " << myDestinationPort << endl;
        aConnection =
                TCP::instance().createConnection(mySourceAddress,
                                                 mySourcePort,
                                                 myDestinationPort,
                                                 this);


        if ((tcpheader->flags & 0x02) != 0 && TCP::instance().acceptConnection(aConnection->myPort)) {
            // State LISTEN. Received a SYN flag.
            aConnection->Synchronize(mySequenceNumber);
        }

        else {
            // State LISTEN. No SYN flag. Impossible to continue.
            aConnection->Kill();
            // cout <<"State LISTEN. No SYN flag. Impossible to continue. TCPInPacket decode kill finished" << endl;
        }
    }
    else {
        // ACK-flag is set
        if ((tcpheader->flags & 0x010) != 0) {
            // cout << "Acknowledge on #" << aConnection->hisPort << endl;
            aConnection->Acknowledge(myAcknowledgementNumber);
            if (myLength != headerLengthInBytes) {
                // cout << " TCP decode, contains DATA" << endl;

                aConnection->Receive(mySequenceNumber, (byte * )(myData), myLength);
            }
        }
        // FIN-Flag
        if ((tcpheader->flags & 0x01) != 0) {
            aConnection->NetClose();
        }
        //RST FLAG
        if ((tcpheader->flags & 0x4) != 0) {
            // cout << "TCP decode RST FLAG" << endl;
            aConnection->RSTflag = true;
            aConnection->mySocket->socketEof();
        }
    }
    aConnection->myWindowSize = HILO(tcpheader->windowSize);
}


void TCPInPacket::answer(byte *theData, udword theLength) {
    myFrame->answer(theData, theLength);
}

uword TCPInPacket::headerOffset() {
    // TCPHeader* header = (TCPHeader*) myData;
    // byte optionsLength = (header->headerLength & 0xF0)>>4;
    return myFrame->headerOffset() + TCP::tcpHeaderLength;// + (optionsLength*4);

}

//----------------------------------------------------------------------------
//
InPacket *
TCPInPacket::copyAnswerChain() {
    TCPInPacket *anAnswerPacket = new TCPInPacket(*this);
    anAnswerPacket->setNewFrame(myFrame->copyAnswerChain());
    return anAnswerPacket;
}

//----------------------------------------------------------------------------
//
TCPPseudoHeader::TCPPseudoHeader(IPAddress &theDestination,
                                 uword theLength) :
        sourceIPAddress(IP::instance().myAddress()),
        destinationIPAddress(theDestination),
        zero(0),
        protocol(6) {
    tcpLength = HILO(theLength);
}

//----------------------------------------------------------------------------
//
uword
TCPPseudoHeader::checksum() {
    return calculateChecksum((byte * )
    this, 12);
}


/****************** END OF FILE tcp.cc *************************************/

