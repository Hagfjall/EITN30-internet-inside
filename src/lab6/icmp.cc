#include "compiler.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern "C"
{
#include "system.h"
}

#include "iostream.hh"
#include "icmp.hh"
//#define D_LLC
#ifdef D_ICMP
#define trace cout
#else
#define trace if(false) cout
#endif


ICMPInPacket::ICMPInPacket(byte *theData, udword theLength, InPacket *theFrame) : InPacket(theData, theLength,
                                                                                           theFrame) {

}

void ICMPInPacket::decode() {
    cout << "ICMPInPacket decode myLength " << myLength << endl;
    ICMPHeader *header = (ICMPHeader *) myData;
    // Ethernet::instance().bytesOffsetPrintOut(header,8,myLength-8);
    ICMPHeader *temp = header + 1;
    ICMPECHOHeader *echoHeader = (ICMPECHOHeader *) temp;
    if (header->type == 0x8) {
        header->checksum = header->checksum;
        uword hoffs = myFrame->headerOffset();
        cout << "headerOffset " << hoffs << endl;
        byte *temp = new byte[myLength + hoffs];
        byte *aReply = temp + hoffs;
        myData[0] = 0; //sets the type flag to echo reply
        header->checksum = ((ICMPHeader *) myData)->checksum + 0x08;
        memcpy(aReply, myData, myLength);
        answer(aReply, myLength + hoffs);
    }
}

void ICMPInPacket::answer(byte *theData, udword theLength) {
    myFrame->answer(theData, theLength);
}

uword ICMPInPacket::headerOffset() {
    return myFrame->headerOffset() + 8;
}
