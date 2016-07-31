/*!***************************************************************************
*!
*! FILE NAME  : FrontPanel.cc
*!
*! DESCRIPTION: Handles the LED:s
*!
*!***************************************************************************/

/****************** INCLUDE FILES SECTION ***********************************/

#include "compiler.h"

#include "iostream.hh"
#include "frontpanel.hh"

//#define D_FP
#ifdef D_FP
#define trace cout
#else
#define trace if(false) cout
#endif

/****************** FrontPanel DEFINITION SECTION ***************************/

//----------------------------------------------------------------------------
//
byte LED::writeOutRegisterShadow = 0x38;

LED::LED(byte theLedNumber) : myLedBit(theLedNumber) {

}

static byte write_out_register_shadow = 0x78;

void LED::on() {
    uword led = 4 << myLedBit;  /* convert LED number to bit weight */
    *(VOLATILE
    byte *)0x80000000 = write_out_register_shadow &= ~led; /* Kanske ska byta till writeoutregistershadow*/
    iAmOn = true;
}

void LED::off() {
    uword led = 4 << myLedBit;  /* convert LED number to bit weight */
    *(VOLATILE
    byte *)0x80000000 = write_out_register_shadow |= led;
    iAmOn = false;
}

void LED::toggle() {
    if (iAmOn) {
        off();

    }
    else { on(); }

}

//----------------------------------------------------------------------------
//
NetworkLEDTimer::NetworkLEDTimer(Duration blinkTime) : myBlinkTime(blinkTime) { }

void NetworkLEDTimer::start() {
    this->timeOutAfter(myBlinkTime);
}

void NetworkLEDTimer::timeOut() {
    FrontPanel::instance().notifyLedEvent(FrontPanel::networkLedId);
}
//----------------------------------------------------------------------------
//

CDLEDTimer::CDLEDTimer(Duration blinkPeriod) {
    this->timerInterval(blinkPeriod);
    this->startPeriodicTimer();
}

void CDLEDTimer::timerNotify() {
    FrontPanel::instance().notifyLedEvent(FrontPanel::cdLedId);
    //cout << "Free mem " << ax_coreleft_total() << " bytes" << endl;
}

//----------------------------------------------------------------------------
//

StatusLEDTimer::StatusLEDTimer(Duration blinkPeriod) {
    this->timerInterval(blinkPeriod);
    this->startPeriodicTimer();
}

void StatusLEDTimer::timerNotify() {
    FrontPanel::instance().notifyLedEvent(FrontPanel::statusLedId);
}

//----------------------------------------------------------------------------
//


FrontPanel::FrontPanel()
        : Job(), mySemaphore(Semaphore::createQueueSemaphore("FP", 0)), myNetworkLED(networkLedId), myCDLED(cdLedId),
          myStatusLED(statusLedId) {
    cout << "FrontPanel created." << endl;
    netLedEvent = false;
    cdLedEvent = false;
    statusLedEvent = false;
    Job::schedule(this);
}

// Singleton for frontpanel
FrontPanel &FrontPanel::instance() {
    static FrontPanel fp;
    return fp;
}

void FrontPanel::packetReceived() {
    myNetworkLED.on();
    myNetworkLEDTimer->start();
    //Turns on network led on and start nework led timer
}

void FrontPanel::notifyLedEvent(uword theLedId) {   // Called from the timers to notify that a timer has expired.
    // Sets an event flag and signals the semaphore.
    //cout << "notifyEvent ";
    if (theLedId == networkLedId) {
        // cout << " networkId" << endl;
        netLedEvent = true;
    } else if (theLedId == cdLedId) {
        cdLedEvent = true;
        //cout << " cdId" << endl;
    } else if (theLedId == statusLedId) {
        statusLedEvent = true;
        //cout << " status" << endl;
    }
    mySemaphore->signal();
}


void FrontPanel::doit() {
    myNetworkLEDTimer = new NetworkLEDTimer(Clock::seconds * 5);
    myCDLEDTimer = new CDLEDTimer(Clock::seconds * 2);
    myStatusLEDTimer = new StatusLEDTimer(Clock::tics * 20);
    packetReceived();

    while (true) {
        mySemaphore->wait();
        if (netLedEvent) {
            myNetworkLED.off();
            netLedEvent = false;

            //cout << "net toggle" << endl;
        }

        if (cdLedEvent) {
            myCDLED.toggle();
            cdLedEvent = false;

            //cout << "cd toggle" << endl;
        }

        if (statusLedEvent) {
            myStatusLED.toggle();
            statusLedEvent = false;

            //cout << "status toggle" << endl;
        }
    }
}



/****************** END OF FILE FrontPanel.cc ********************************/
