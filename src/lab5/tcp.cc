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

#define D_TCP
#ifdef D_TCP
#define trace cout
#else
#define trace if(false) cout
#endif
/****************** TCP DEFINITION SECTION *************************/

//----------------------------------------------------------------------------
//
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
//
void
TCP::deleteConnection(TCPConnection *theConnection) {
    cout << "deleteconnection" << endl;
    myConnectionList.Remove(theConnection);
    delete theConnection;
}

bool
TCP::acceptConnection(uword portNo) {
    return portNo == 7;
}

// Is true when a connection is accepted on port portNo.
void
TCP::connectionEstablished(TCPConnection *theConnection) {
    cout << "TCP connectionEstablished" << endl;
    if (theConnection->serverPortNumber() == 7) {


        TCPSocket *aSocket = new TCPSocket(theConnection);
        theConnection->registerSocket(aSocket);
        Job::schedule(new SimpleApplication(aSocket));
        // Create and start an application for the connection.
    }
}
// Create a new TCPSocket. Register it in TCPConnection.
// Create and start a SimpleApplication.

//----------------------------------------------------------------------------
//
retransmitTimer::retransmitTimer(TCPConnection *theConnection,
                                 Duration retransmitTime) : myConnection(theConnection),
                                                            myRetransmitTime(retransmitTime) {

}

void retransmitTimer::start() {
    this->timeOutAfter(myRetransmitTime);
}

void retransmitTimer::cancel() {
    this->resetTimeOut();
}

void retransmitTimer::timeOut() {
    myConnection->sendNext = myConnection->sentUnAcked;
    cout << "retransmitTimer timeOut" << endl;
    myConnection->myTCPSender->sendFromQueue();
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
}

//----------------------------------------------------------------------------
//
TCPConnection::~TCPConnection() {
    cout << "TCP connection destroyed" << endl;
    delete mySocket;
    delete myTCPSender;
    delete timer;
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
    myState->Kill(this);

    //TODO - Styänga TCP-connection rensa minne.
}

// Handle incoming data
void TCPConnection::Receive(udword theSynchronizationNumber,
                            byte *theData,
                            udword theLength) {
    myState->Receive(this, theSynchronizationNumber, theData, theLength);
}

// Handle incoming Acknowledgement
void TCPConnection::Acknowledge(udword theAcknowledgementNumber) {

    if (sentUnAcked < theAcknowledgementNumber) {
        sentUnAcked = theAcknowledgementNumber;
        // cout << "sentUnAcked " << sentUnAcked << " sentMaxSeq " << sentMaxSeq << endl;
        if (sentUnAcked == sentMaxSeq) {
            timer->cancel();
        }
    }
    myState->Acknowledge(this, theAcknowledgementNumber);
}

// Send outgoing data
void TCPConnection::Send(byte *theData,
                         udword theLength) {

    myState->Send(this, theData, theLength);

    // timer->start();
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
    cout << "TCPState::Kill" << endl;
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

    // cout << "got SYN on ECHO port" << endl;
    theConnection->receiveNext = theSynchronizationNumber + 1;
    theConnection->receiveWindow = 8 * 1024;
    theConnection->sendNext = get_time();
    theConnection->sentMaxSeq = theConnection->sendNext;
    // Next reply to be sent.
    theConnection->sentUnAcked = theConnection->sendNext;
    // Send a segment with the SYN and ACK flags set.
    theConnection->myTCPSender->sendFlags(0x12);
    // Prepare for the next send operation.
    theConnection->sendNext += 1;
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
    theConnection->sentUnAcked = theAcknowledgementNumber;
    theConnection->myState = EstablishedState::instance();
    TCP::instance().connectionEstablished(theConnection);
    theConnection->firstSeq = 0;
    theConnection->queueLength = 0;
    // cout << "SynRecvdState::ACK - sentUnAcked " << theConnection->sentUnAcked << " sendNext " << theConnection->sendNext << endl;

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
    theConnection->sendNext += 1;
    // cout << "inside establisheds APPCLOSE" << endl;
    theConnection->myState = FinWait1State::instance();
}

void EstablishedState::Acknowledge(TCPConnection *theConnection,
                                   udword theAcknowledgementNumber) {
    // cout << "EstablishedState::Acknowledge nbr " << theAcknowledgementNumber << endl;

    // Kolla om det sista har blivit ackat
    if (theConnection->queueLength + theConnection->firstSeq == theAcknowledgementNumber) {
        theConnection->mySocket->socketDataSent();
    }
    else {
        // cout << "EstablishedState::Acknowledge sendFromQueue " << endl;
        theConnection->myTCPSender->sendFromQueue();
    }


}

void EstablishedState::Send(TCPConnection *theConnection,
                            byte *theData,
                            udword theLength) {
    // cout << " EstablishedState::Send ";
    // Ethernet::instance().bytesOffsetPrintOut(theData,0,10);
    theConnection->transmitQueue = theData;
    theConnection->queueLength = theLength;
    theConnection->firstSeq = theConnection->sendNext;
    theConnection->theOffset = 0;
    theConnection->theFirst = theConnection->transmitQueue;
    theConnection->theSendLength = 1460;
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
    theConnection->mySocket->socketEof();
    theConnection->myState = CloseWaitState::instance();
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
    TCPHeader *tempheader = (TCPHeader *) theData;
    byte headerLengthInBytes = ((tempheader->headerLength & 0xF0) >> 4) * 4;
    theLength -= headerLengthInBytes;
    // cout << "EstablishedState::Receive" << endl;
    theConnection->receiveNext += theLength;
    theConnection->myTCPSender->sendFlags(0x010);
    theData = (byte * )(tempheader + 1);

    if ((tempheader->flags & 0x8) != 0) {
        theConnection->mySocket->socketDataReceived(theData, theLength);
    } else {
        //TODO: Byt ut denna mot en buffring av applikationsdata.

        theConnection->mySocket->socketDataReceived(theData, theLength);
    }

    // Delayed ACK is not implemented, simply acknowledge the data
    // by sending an ACK segment, then echo the data using Send.
}

//----------------------------------------------------------------------------
//
CloseWaitState *
CloseWaitState::instance() {
    static CloseWaitState myInstance;
    return &myInstance;
}

void CloseWaitState::AppClose(TCPConnection *theConnection) {
    //TODO do something more here?

    theConnection->myState = LastAckState::instance();

    theConnection->myTCPSender->sendFlags(0x11); //sending a FIN, ACK 
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
    theConnection->sentUnAcked = theAcknowledgementNumber;
    if (theConnection->sendNext + 1 == theAcknowledgementNumber) {
        theConnection->Kill();
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
        theConnection->NetClose();

    }
}

FinWait2State *
FinWait2State::instance() {
    static FinWait2State myInstance;
    return &myInstance;
}

void FinWait2State::NetClose(TCPConnection *theConnection) {
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
    counter = 0;
}

//----------------------------------------------------------------------------
//

void TCPSender::sendFlags(byte theFlags) {
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

    if ((theFlags & 0x010) != 0)
        aTCPHeader->acknowledgementNumber = LHILO(myConnection->receiveNext);
    else
        aTCPHeader->acknowledgementNumber = 0;
    aTCPHeader->headerLength = 0x50; //since 6 bits are shitty
    aTCPHeader->flags = theFlags;
    aTCPHeader->windowSize = HILO(myConnection->receiveWindow);
    //TODO IF the URG flag is set do something here
    aTCPHeader->urgentPointer = 0;
    aTCPHeader->checksum = calculateChecksum(anAnswer,
                                             TCP::tcpHeaderLength,
                                             pseudosum);
    // cout << "sending flags" << endl;
    // Ethernet::instance().bytesOffsetPrintOut(anAnswer, 0, 20);
    myAnswerChain->answer(anAnswer, myAnswerChain->headerOffset());
    // Deallocate the dynamic memory
    delete anAnswer;
}

void TCPSender::sendData(byte *theData, udword theLength) {
    // FUNCTION TO DROP EVERY THIRTIETH DATA PACKET:
    // if(counter++ % 30 == 0){ //TODO SHOULDNT BE USED IN PRODUCTION
    //   // cout << "dropping packet! sendNext: " <<  myConnection->sendNext << endl;
    //   myConnection->sendNext += theLength;
    //   return;
    // }



    byte *anAnswer = new byte[myAnswerChain->headerOffset() + theLength];
    TCPPseudoHeader *aPseudoHeader =
            new TCPPseudoHeader(myConnection->hisAddress,
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
    aTCPHeader->flags = 0x18; //TODO  maybe just and 0x010? change to dynamic if no ack should be
    aTCPHeader->windowSize = HILO(myConnection->receiveWindow);
    //TODO IF the URG flag is set do something here
    aTCPHeader->urgentPointer = 0;

    memcpy((byte * )(aTCPHeader + 1), theData, theLength);
    aTCPHeader->checksum = calculateChecksum(anAnswer,
                                             theLength + TCP::tcpHeaderLength,
                                             pseudosum);
    myAnswerChain->answer(anAnswer, myAnswerChain->headerOffset() + theLength);
    myConnection->timer->start();
    delete anAnswer;
    myConnection->sendNext += theLength;
    if (myConnection->sentMaxSeq < myConnection->sendNext) {
        myConnection->sentMaxSeq = myConnection->sendNext;
    }
}

void TCPSender::sendFromQueue() {

    udword tempWindowSize = myConnection->myWindowSize - (myConnection->sendNext - myConnection->sentUnAcked);
    if (tempWindowSize > myConnection->myWindowSize) {
        tempWindowSize = 0;
    }


    udword bytesLeft = myConnection->queueLength - myConnection->theOffset;
    // if(myConnection->theOffset == 0){
    //   cout << "OFFSET 0 address of theFirst";
    //   Ethernet::instance().bytesOffsetPrintOut(myConnection->theFirst,myConnection->queueLength-2000,2000);
    //   // Ethernet::instance().bytesOffsetPrintOut(myConnection->theFirst,0,0);
    // }
    // if(myConnection->theOffset > 1048576-2000){
    //   cout << "TCPSender::sendFromQueue starting while with tempWindowSize: " << tempWindowSize << " offset: " << myConnection->theOffset
    //   << " queueLength " << myConnection->queueLength <<
    //   " bytesLeft: " << bytesLeft <<
    //   " sentMaxSeq " << myConnection->sentMaxSeq << " sendNext " << myConnection->sendNext << endl;
    //   cout << "OFFSET ALOT address of theFirst:";
    //   Ethernet::instance().bytesOffsetPrintOut(myConnection->theFirst,0,myConnection->queueLength - myConnection->theOffset);
    // }

    //Check for retransmission
    // cout << "check for retransmission " << myConnection->sentMaxSeq << " > " << myConnection->sendNext <<endl;
    if (myConnection->sentMaxSeq > myConnection->sendNext) {
        // cout << "resending packet " << myConnection->sendNext << endl;
        udword length = myConnection->sentMaxSeq - myConnection->sendNext;
        if (length > myConnection->theSendLength) {
            length = myConnection->theSendLength;
        }
        sendData(myConnection->transmitQueue + (myConnection->sendNext - myConnection->firstSeq), length);
        myConnection->sendNext = myConnection->sentMaxSeq;
        return;
    }

    while (tempWindowSize > 0 && bytesLeft > 0) {
        udword length = myConnection->theSendLength;
        if (myConnection->queueLength < myConnection->theOffset + length) {
            length = myConnection->queueLength - myConnection->theOffset;
            if (length == 0) {
                // cout << "returning from while" << endl;

                return;
            }
        }
        if (length > tempWindowSize) {
            length = tempWindowSize;
        }
        sendData(myConnection->theFirst, length);
        myConnection->theOffset += length;
        myConnection->theFirst += length;
        tempWindowSize -= length;
    }
    // cout << "sendFromQueue finished" << endl;
}

TCPSender::~TCPSender() {
    cout << "deleting TCP Sender" << endl;
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


    // cout << "TCP Port: "<< myDestinationPort << endl;
    // Ethernet::instance().bytesOffsetPrintOut(tcpheader,0,40);




    TCPConnection *aConnection =
            TCP::instance().getConnection(mySourceAddress,
                                          mySourcePort,
                                          myDestinationPort);
    aConnection->myWindowSize = HILO(tcpheader->windowSize);
    // cout << "Trying to decode TCPInPacket" << endl;
    if (!aConnection) {
        // Establish a new connection.
        // cout << "Creating TCPConnection" << endl;
        aConnection =
                TCP::instance().createConnection(mySourceAddress,
                                                 mySourcePort,
                                                 myDestinationPort,
                                                 this);
        /*
           TODO : check that the connection are in the correct state when checking flags
        */

        if ((tcpheader->flags & 0x02) != 0 && TCP::instance().acceptConnection(aConnection->myPort)) {
            // State LISTEN. Received a SYN flag.
            aConnection->Synchronize(mySequenceNumber);
        }

        else {
            // State LISTEN. No SYN flag. Impossible to continue.
            aConnection->Kill();
        }
    }
    else {
        if (myLength != headerLengthInBytes) {
            // cout << " TCP decode, contains DATA" << endl;

            aConnection->Receive(mySequenceNumber, (byte * )(myData), myLength);
            if ((tcpheader->flags & 0x01) != 0 && aConnection->receiveNext == mySequenceNumber) { //FIN FLAG IS SET
                // cout << " TCP Decode, FIN FLAG SET WHEN SENDING DATA" << endl;
                aConnection->NetClose();
            }
        }
            // else if ( tcpheader->flags == 0x11 ){ // FIN+ACK
            //           // aConnection->Acknowledge(myAcknowledgementNumber);
            //           aConnection->NetClose();
            // }
            // else if ( tcpheader->flags == 0x10 ){ // ACK FLAG
            //           // aConnection->Acknowledge(myAcknowledgementNumber);
            //           aConnection->Acknowledge(myAcknowledgementNumber);
            // }
        else if ((tcpheader->flags & 0x01) != 0) {
            // cout << " TCP Decode, FIN FLAG SET" << endl;
            if (aConnection->receiveNext == mySequenceNumber) {
                // cout << "net close " << endl;
                aConnection->NetClose();
            }
            else {
                // cout << "ACK" << endl;
                aConnection->Acknowledge(aConnection->receiveNext);
            }
            if (tcpheader->flags == 0x11) {
                // cout << "calling Acknowledge" << endl;
                aConnection->Acknowledge(myAcknowledgementNumber);
            }
        }
        else if ((tcpheader->flags & 0x010) != 0) {
            // cout << "TCP decode ACK FLAG" << endl;
            aConnection->Acknowledge(myAcknowledgementNumber);
        }
        // om ack påx  sekvens som finns i tcp: gå till established

        //
        // Connection was established. Handle all states.
        // Kolla alla states vad som ska göras typ
        //TODO

    }

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

