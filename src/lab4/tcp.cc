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

//----------------------------------------------------------------------------
//
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
}

//----------------------------------------------------------------------------
//
TCPConnection::~TCPConnection() {
    trace << "TCP connection destroyed" << endl;
    delete myTCPSender;
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
    myState->NetClose(this);

}

// Handle close from application
void TCPConnection::AppClose() {
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
    myState->Acknowledge(this, theAcknowledgementNumber);
}

// Send outgoing data
void TCPConnection::Send(byte *theData,
                         udword theLength) {
    myTCPSender->sendData(theData, theLength);
    //TODO
}



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
    switch (theConnection->myPort) {
        case 7:
            // cout << "got SYN on ECHO port" << endl;
            theConnection->receiveNext = theSynchronizationNumber + 1;
            theConnection->receiveWindow = 8 * 1024;
            theConnection->sendNext = get_time();
            // Next reply to be sent.
            theConnection->sentUnAcked = theConnection->sendNext;
            // Send a segment with the SYN and ACK flags set.
            theConnection->myTCPSender->sendFlags(0x12);
            // Prepare for the next send operation.
            theConnection->sendNext += 1;
            // Change state
            theConnection->myState = SynRecvdState::instance();
            break;
        default:
            // trace << "send RST..." << endl;
            theConnection->sendNext = 0;
            // Send a segment with the RST flag set.
            theConnection->myTCPSender->sendFlags(0x04);
            TCP::instance().deleteConnection(theConnection);
            break;
    }


    theConnection->Acknowledge(theConnection->receiveNext);

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


}


//----------------------------------------------------------------------------
//
EstablishedState *
EstablishedState::instance() {
    static EstablishedState myInstance;
    return &myInstance;
}

void EstablishedState::Acknowledge(TCPConnection *theConnection,
                                   udword theAcknowledgementNumber) {
    theConnection->sentUnAcked = theAcknowledgementNumber;

}

void EstablishedState::Send(TCPConnection *theConnection,
                            byte *theData,
                            udword theLength) {
    // cout << "inne och hänger i Send EstablishedState" << endl;

    theConnection->Send(theData, theLength);
    theConnection->sendNext += theLength;

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
    // trace << "EstablishedState::Receive" << endl;
    theConnection->receiveNext += theLength;
    theConnection->myTCPSender->sendFlags(0x010);
    Send(theConnection, theData, theLength);

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

    theConnection->myTCPSender->sendFlags(0x11); //sending a FIN 
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
    // cout << "sendData theLength " << theLength << endl;
    byte *anAnswer = new byte[myAnswerChain->headerOffset() + theLength];
    // cout << "Size of packet: " << myAnswerChain->headerOffset() << endl;
    anAnswer = anAnswer + myAnswerChain->headerOffset() - TCP::tcpHeaderLength;
    // Calculate the pseudo header checksum
    TCPPseudoHeader *aPseudoHeader =
            new TCPPseudoHeader(myConnection->hisAddress,
                                theLength + TCP::tcpHeaderLength);
    uword pseudosum = aPseudoHeader->checksum();
    delete aPseudoHeader;
    // Create the TCP segment.
    // Calculate the final checksum.
    TCPHeader *aTCPHeader = (TCPHeader *) anAnswer;



    // Send the TCP segment.
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
    aTCPHeader->checksum = 0;
    aTCPHeader->checksum = calculateChecksum(anAnswer,
                                             theLength + TCP::tcpHeaderLength,
                                             pseudosum);
    // cout << "sendData header: ";
    // Ethernet::instance().bytesOffsetPrintOut(aTCPHeader, 0,20);
    myAnswerChain->answer(anAnswer, myAnswerChain->headerOffset() + theLength);
    // Deallocate the dynamic memory
    delete anAnswer;

}

TCPSender::~TCPSender() {
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

        if ((tcpheader->flags & 0x02) != 0) {
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
            // cout << " rec flag" << endl;

            aConnection->Receive(mySequenceNumber, (byte * )(myData + headerLengthInBytes),
                                 myLength - headerLengthInBytes);
            if ((tcpheader->flags & 0x01) != 0 && aConnection->receiveNext == mySequenceNumber) { //FIN FLAG IS SET
                aConnection->NetClose();
            }
        }
        else if (tcpheader->flags == 0x010) {
            // cout << " ack flag" << endl;
            aConnection->Acknowledge(myAcknowledgementNumber);
        }
        else if ((tcpheader->flags & 0x01) != 0) {
            // cout << " fin flag" << endl;
            if (aConnection->receiveNext == mySequenceNumber)
                aConnection->NetClose();
            else {
                aConnection->Acknowledge(aConnection->receiveNext);
            }
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

