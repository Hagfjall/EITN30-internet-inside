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
//#define D_LLC
#ifdef D_IP
#define trace cout
#else
#define trace if(false) cout
#endif

uword IPIdentificationId(1);

IPInPacket::IPInPacket(byte *theData, udword theLength, InPacket *theFrame) : InPacket(theData, theLength, theFrame) {
}

void IPInPacket::decode() {
    IPHeader *ipheader = (IPHeader *) myData;
    IPAddress myIp(130, 235, 200, 118);
    if ((ipheader->destinationIPAddress == myIp) == false || ipheader->versionNHeaderLength != 0x45 ||
        (ipheader->fragmentFlagsNOffset & 0x3FFF) != 0) {
        return;
    }
    uword realPacketLength = HILO(ipheader->totalLength);
    myProtocol = ipheader->protocol;
    mySourceIPAddress = ipheader->sourceIPAddress;
    if (ipheader->protocol == 1) {
        //new icmp packe
        ICMPInPacket packet((byte *)(ipheader + 1), realPacketLength
        -IP::ipHeaderLength, this);
        packet.decode();
    }

    // Här kollar vi tcp och udp.
}

void IPInPacket::answer(byte *theData, udword theLength) {
    IPHeader *replyHeader = (IPHeader *) (theData) - 1;

    replyHeader->protocol = myProtocol;
    replyHeader->destinationIPAddress = mySourceIPAddress;
    replyHeader->fragmentFlagsNOffset = 0;
    replyHeader->totalLength = HILO(theLength + IP::ipHeaderLength);
    replyHeader->identification = IPIdentificationId;
    IPIdentificationId = (IPIdentificationId + 1 % 65536);
    replyHeader->timeToLive = 64;
    replyHeader->versionNHeaderLength = 0x45;
    replyHeader->TypeOfService = 0;
    replyHeader->sourceIPAddress = IPAddress(130, 235, 200, 118);
    replyHeader->headerChecksum = 0;
    replyHeader->headerChecksum = calculateChecksum((byte *) replyHeader, IP::ipHeaderLength, 0);
    myFrame->answer((byte *) replyHeader, theLength + IP::ipHeaderLength);

}

uword IPInPacket::headerOffset() {
    return myFrame->headerOffset() + 20;
}
