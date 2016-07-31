#include "compiler.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern "C"
{
#include "system.h"
}

#include "iostream.hh"
#include "ip.hh"
#include "icmp.hh"
#include "inpacket.hh"
#include "ipaddr.hh"
#include "tcp.hh"
//#define D_LLC
#ifdef D_IP
#define trace cout
#else
#define trace if(false) cout
#endif

uword IPIdentificationId(1);

IPInPacket::IPInPacket(byte *theData, udword theLength, InPacket *theFrame) : InPacket(theData, theLength, theFrame) {
}

IP::IP() {
    myIPAddress = new IPAddress(130, 235, 200, 118);
}

IP &IP::instance() {
    static IP myInstance;
    return myInstance;
}

const IPAddress &IP::myAddress() {
    return *myIPAddress;
}

void IPInPacket::decode() {
    IPHeader *ipheader = (IPHeader *) myData;
    IPAddress myIp(130, 235, 200, 118);
    if ((ipheader->destinationIPAddress == myIp) == false ||
        ipheader->versionNHeaderLength != 0x45 ||
        (ipheader->fragmentFlagsNOffset & 0xFF3F) != 0) {
        return;
    }
    uword realPacketLength = HILO(ipheader->totalLength);
    if (realPacketLength > 1500)
        cout << "IPInPacket realPacketLength " << realPacketLength << endl;
    myProtocol = ipheader->protocol;
    mySourceIPAddress = ipheader->sourceIPAddress;


    if (ipheader->protocol == 1) { //ICMP

        ICMPInPacket packet((byte *)(ipheader + 1), realPacketLength
        -IP::ipHeaderLength, this);
        packet.decode();
    }
    else if (ipheader->protocol == 6) { //TCP
        // cout << "Entering TCP" << endl;
        TCPInPacket packet((byte *)(ipheader + 1),
        realPacketLength - IP::ipHeaderLength,
                this, ipheader->sourceIPAddress);
        packet.decode();
    }

}

void IPInPacket::answer(byte *theData, udword theLength) {
    // cout << "IPInPacket theLength " << dec << theLength << endl;
    IPHeader *replyHeader = (IPHeader *) (theData) - 1;

    replyHeader->protocol = myProtocol;
    replyHeader->destinationIPAddress = mySourceIPAddress;
    replyHeader->fragmentFlagsNOffset = 0;
    replyHeader->totalLength = HILO(theLength - myFrame->headerOffset());
    replyHeader->identification = IPIdentificationId;
    IPIdentificationId = (IPIdentificationId + 1 % 65536);
    replyHeader->timeToLive = 64;
    replyHeader->versionNHeaderLength = 0x45;
    replyHeader->TypeOfService = 0;
    replyHeader->sourceIPAddress = IPAddress(130, 235, 200, 118);
    replyHeader->headerChecksum = 0;
    replyHeader->headerChecksum = calculateChecksum((byte *) replyHeader, IP::ipHeaderLength, 0);
    myFrame->answer((byte *) replyHeader, theLength);

}

uword IPInPacket::headerOffset() {
    return myFrame->headerOffset() + 20;
}

InPacket *IPInPacket::copyAnswerChain() {
    IPInPacket *anAnswerPacket = new IPInPacket(*this);
    anAnswerPacket->setNewFrame(myFrame->copyAnswerChain());
    return anAnswerPacket;
}
