#include "compiler.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern "C"
{
#include "system.h"
}

#include "iostream.hh"
#include "arp.hh"
//#define D_LLC
#ifdef D_ARP
#define trace cout
#else
#define trace if(false) cout
#endif


ARPInPacket::ARPInPacket(byte *theData, udword theLength, InPacket *theFrame) : InPacket(theData, theLength, theFrame) {

}

void ARPInPacket::decode() {
    ARPHeader *header = (ARPHeader *) myData;
    IPAddress myIp = IPAddress(130, 235, 200, 118);
    if (header->targetIPAddress == myIp) {
        uword hoffs = myFrame->headerOffset();
        byte *temp = new byte[myLength + hoffs];
        byte *aReply = temp + hoffs;
        memcpy(aReply, myData, myLength);
        ARPHeader *headerReply = (ARPHeader *) aReply;
        headerReply->targetIPAddress = header->senderIPAddress;
        headerReply->targetEthAddress = header->senderEthAddress;
        headerReply->senderEthAddress = Ethernet::instance().myAddress();
        headerReply->op = HILO(0x0002);
        headerReply->senderIPAddress = myIp;
        answer(aReply, myLength + hoffs);
    }

}

void ARPInPacket::answer(byte *theData, udword theLength) {
    myFrame->answer(theData, theLength);
}

uword ARPInPacket::headerOffset() {
    return 28 + myFrame->headerOffset();
}
