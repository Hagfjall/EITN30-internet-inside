/*!***************************************************************************
*!
*! FILE NAME  : http.cc
*!
*! DESCRIPTION: HTTP, Hyper text transfer protocol.
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
}

#include "iostream.hh"
#include "tcpsocket.hh"
#include "ethernet.hh"
#include "http.hh"
#include "fs.hh"
#include "tcp.hh"

//#define D_HTTP
#ifdef D_HTTP
#define trace cout
#else
#define trace if(false) cout
#endif

/****************** HTTPServer DEFINITION SECTION ***************************/


HTTPServer::HTTPServer(TCPSocket *theSocket) : mySocket(theSocket) {
    myAllocateMemorySem = Semaphore::createQueueSemaphore("aaaaa", 0);
    myMemoryTimer = new AllocateMemoryTimer(myAllocateMemorySem);
    // myFileSystem = &FileSystem::instance();
}

void HTTPServer::doit() {
    udword p = mySocket->myConnection->hisPort;
    udword aLength;
    byte *aData;
    bool done = false;
    byte *startOfHeader;
    byte *holdRequest;
    udword requestLengthSoFar = 0;
// cout <<"entering While in DOIT " << "#" << p << endl;
// cout <<"number of TCP connections " << TCP::instance().myConnectionList.Length() << endl;
    while (!done && !mySocket->isEof()) {
        // cout << " HTTPServer::doit waiting for read " << endl;
        aData = mySocket->Read(aLength);
        // cout <<" --A!!!---!!! DATA " << endl;
        // Ethernet::instance().bytesOffsetPrintOut(aData,0,aLength);

        // cout << " HTTPServer::doit data read" << endl;
        byte *theData = new byte[aLength];
        memcpy(theData, aData, aLength);
        delete[] aData;
        // cout <<"HTTPServer::doit() with " << aLength
        // << " nbr of bytes in packet" << "#" << p << endl;
        // cout << "Free mem " << ax_coreleft_total() << " bytes" << endl;
        if (aLength > 0) {
            // cout << "cheking for gotWholeHeader" << endl;
            if (gotWholeHeader(theData, aLength)) {
                if (holdRequest != NULL) {
                    startOfHeader = allocate(requestLengthSoFar + aLength);
                    memcpy(startOfHeader, holdRequest, requestLengthSoFar);
                    memcpy(startOfHeader + requestLengthSoFar, theData, aLength);
                    delete[] theData;
                    delete[] holdRequest;
                    theData = startOfHeader;
                }
                // cout <<"Got whole header"  << "#" << p << " with length: " << aLength << " requestLengthSoFar: " << requestLengthSoFar << endl;
                handleHeader(theData, aLength + requestLengthSoFar);
                done = true;
            } else {
                startOfHeader = allocate(requestLengthSoFar + aLength);
                if (holdRequest) {
                    memcpy(startOfHeader, holdRequest, requestLengthSoFar);
                }
                memcpy(startOfHeader + requestLengthSoFar, theData, aLength);
                delete[] holdRequest;
                holdRequest = startOfHeader;
                if (gotWholeHeader(startOfHeader, aLength + requestLengthSoFar)) {
                    // cout <<"finally got whole header" << "#" << p << endl;
                    handleHeader(startOfHeader, aLength + requestLengthSoFar);
                    done = true;
                }
            }
        }
        requestLengthSoFar += aLength;
        delete[] theData;
    }
// cout <<"HTTP::server doit checking for isEof" << "#" << p << endl;
    if (!mySocket->isEof()) {
        mySocket->Close();
    }
// cout <<"HTTPServer::doit() finished" << "#" << p << endl;
    delete myMemoryTimer;
    delete myAllocateMemorySem;
    delete[] startOfHeader;
}

byte *HTTPServer::allocate(udword length) {
    // cout << "HTTPServer::allocate with length "<< length << endl;
    byte *ret = new byte[length];
    while (ret == NULL) {
        // cout <<"HTTPServer::allocate got null, need to wait to get enough memory" << endl;
        myMemoryTimer->start();
        myAllocateMemorySem->wait();
        delete[] ret;
        ret = new byte[length];
    }
    // cout << "HTTPServer::allocate done" << endl;
    return ret;
}
//----------------------------------------------------------------------------
//
// Allocates a new null terminated string containing a copy of the data at
// 'thePosition', 'theLength' characters long. The string must be deleted by
// the caller.
//

void HTTPServer::handleHeader(byte *theData, udword theLength) {
    udword index = 0;
    udword p = mySocket->myConnection->hisPort;
    bool postFound = false, getFound = false, headFound = false;
    const static char *postString = "POST ";
    const static char *getString = "GET ";
    const static char *headString = "HEAD ";


    if (strncmp(theData, postString, strlen(postString)) == 0) {
        postFound = true;
    } else if (strncmp(theData, getString, strlen(getString)) == 0) {
        getFound = true;
    } else if (strncmp(theData, headString, strlen(headString)) == 0) {
        headFound = true;
    }

    char *header;
    char *pathName;
    char *fileName;
    udword aLength;

    if (postFound || getFound || headFound) {
        header = extractString((char *) theData, theLength);
        // cout <<"theLength of header: " << theLength << endl;
        // cout <<"HEADER: " << header << endl;
        // printf("--HEADER:%s\n----------\n", header);
        pathName = findPathName(header);
        char *firstPos = strchr(header, ' ');
        firstPos++;
        char *lastPos = strchr(firstPos, ' ');
        char *lastSlash = strchr(header, '/');
        while (true) {
            char *temp = strchr(lastSlash + 1, '/');
            // cout << "inside while" << endl;
            if (temp == NULL || temp > lastPos) {
                break;
            }
            lastSlash = temp;
        }
        if (lastPos - lastSlash > 1) {
            fileName = extractString(lastSlash + 1, (int) lastPos - (int) lastSlash);
        } else {
            fileName = "index.htm";
        }
    }
    byte *responseData = FileSystem::instance().readFile(pathName, fileName, aLength);

    if (getFound || headFound) {
        //AUTHENTICATION
        if (strncmp(pathName, "private", 7) == 0 && strncmp(fileName, "private.htm", 11) == 0) {

            /*CHECKING IF HEADER HAS Authorization:*/
            if (userAuthenticated(header)) {
                // cout <<"USER IS AUTHENTICATION" << endl;
                udword dynamicPageLength = 0;
                char *dynamicPage = (char *) FileSystem::instance().readFile("dynamic\xff", "dynamic.htm",
                                                                             dynamicPageLength);
                // cout <<"read dynamic with " << dynamicPageLength << " bytes" <<endl;
                char *postForm = new char[dynamicPageLength + 309]; //the size of the FORM around the actual file
                strcpy(postForm,
                       "<html><head><title>Form</title></head><body><center><h1>Form for the dynamic page</h1>Here you can edit the dynamic page<form method=\"POST\"><p><hl><p><input type=submit value=\"Submit the form\"><p>Edit your dynamic page:<p><textarea name=\"dynamic.htm\" rows=15 cols=30>");
                postForm += 267;
                strncpy(postForm, dynamicPage, dynamicPageLength);
                postForm += dynamicPageLength;
                strcpy(postForm, "</textarea></form></center></body></html>");
                postForm -= (267 + dynamicPageLength);

                char *intermediateResponseString = new char[75];
                char *builtResponse = "HTTP/1.0 200 OK\r\n";
                char *contentType = "Content-Type: text/html\r\n";
                sprintf(intermediateResponseString, "%s%sContent-Length: %ld\r\n\r\n", builtResponse, contentType,
                        dynamicPageLength + 308);
                mySocket->Write((byte *) intermediateResponseString, (udword) strlen(intermediateResponseString));
                mySocket->Write((byte *) postForm, strlen(postForm));
                delete[] postForm;
                delete[] intermediateResponseString;

            } else {
                char *response401 = "HTTP/1.0 401 Unauthorized\r\nContent-Type: text/html\r\nWWW-Authenticate: Basic realm=\"private\"\r\n\r\n<html><head><title>401 Unauthorized</title></head>\r\n<body><h1>401 Unauthorized</h1></body></html>";
                mySocket->Write((byte *) response401, (udword) strlen(response401));
            }
        }
            // If no file was found - Return 404 Error
        else if (responseData == NULL) {
            // cout <<"404 Error on " << pathName << "/" << fileName << endl;
            char *http404error = "HTTP/1.0 404 Not found\r\nContent-Type: text/html\r\nContent-Length: 92\r\n\r\n<html><head><title>File not found</title></head><body><h1>404 Not found</h1></body></html>";
            mySocket->Write((byte *) http404error, (udword) strlen(http404error));
        }
            // Else a file was found and authentication was not needed.
        else {
            char *builtResponse = "HTTP/1.0 200 OK\r\n";
            char *contentType;

            char *intermediateResponseString = new char[75];
            if (strstr(fileName, ".jpg") != NULL) {
                contentType = "Content-Type: image/jpeg\r\n";
            }
            else if (strstr(fileName, ".gif") != NULL) {
                contentType = "Content-Type: image/gif\r\n";
            } else {
                contentType = "Content-Type: text/html\r\n";
            }
            sprintf(intermediateResponseString, "%s%sContent-Length: %ld\r\n\r\n", builtResponse, contentType, aLength);
            mySocket->Write((byte *) intermediateResponseString, (udword) strlen(intermediateResponseString));
            // cout << "Headfound" << headFound << "path+filename " << pathName << "/" << fileName << endl;
            delete[] intermediateResponseString;
            // cout <<
            if (getFound) {
                // cout <<"sending responseData for #" << p << endl;
                mySocket->Write((byte *) responseData, aLength);
            }
        }
    }
    else if (postFound) {
        byte *startOfData = strstr(header, "\r\n\r\n");
        udword receiveDataSoFar = 0;
        char *encodedForm;
        bool containsData = false;
        if (startOfData != NULL && theLength - ((int) startOfData - (int) ((byte *) header)) > 4) {
            startOfData += 4;
            containsData = true;
            receiveDataSoFar = theLength - ((int) startOfData - (int) ((byte *) header));
        }
        udword contLen = contentLength(header, theLength);
        udword readLength;
        if (containsData && receiveDataSoFar == contLen) {
            encodedForm = decodeForm((char *) startOfData);
            FileSystem::instance().writeFile(pathName, fileName, encodedForm, (udword) strlen(encodedForm));
        }
        else {
            byte *postData = new byte[contLen];
            if (containsData && receiveDataSoFar < contLen) {
                memcpy(postData, startOfData, receiveDataSoFar);
            }
            while (receiveDataSoFar < contLen && !mySocket->isEof()) {
                byte *recData = mySocket->Read(readLength);
                memcpy(postData + receiveDataSoFar, recData, readLength);
                receiveDataSoFar += readLength;
                delete[] recData;
            }
            char *postForm = extractString((char *) postData, contLen);
            encodedForm = decodeForm(postForm);
            delete[] postForm;
            delete[] postData;
            FileSystem::instance().writeFile(pathName, fileName, encodedForm, (udword) strlen(encodedForm));

        }
        mySocket->Write((byte *) "HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n", 44);
        delete[] encodedForm;
    }

    delete[] postString;
    delete[] getString;
    delete[] headString;
    delete[] header;
    delete[] pathName;
    delete[] fileName;

}

bool
HTTPServer::userAuthenticated(char *header) {
    const static char *user1 = "micke:password";
    const static char *user2 = "fredrik:haxx";
    char *indexOfAuth = strstr(header, "Authorization: Basic ");
    if (indexOfAuth != NULL) {
        indexOfAuth += 21;
        char *indexOfBR = strstr(indexOfAuth, "\r\n");
        char *encodedAuth = extractString(indexOfAuth, (int) indexOfBR - (int) indexOfAuth);
        char *decodedAuth = decodeBase64(encodedAuth);
        delete[] encodedAuth;
        bool ret = strcmp(decodedAuth, user1) == 0 || strcmp(decodedAuth, user2) == 0;
        delete[] decodedAuth;
        return ret;
    }
    else return false;
}

char *
HTTPServer::extractString(char *thePosition, udword theLength) {
    char *aString = new char[theLength + 1];
    strncpy(aString, thePosition, theLength);
    aString[theLength] = '\0';
    return aString;
}

char *
HTTPServer::findPathName(char *str) {
    char *firstPos = strchr(str, ' ');     // First space on line
    firstPos++;                            // Pointer to first /
    char *lastPos = strchr(firstPos, ' '); // Last space on line
    char *thePath = 0;                     // Result path
    if ((lastPos - firstPos) == 1) {
        // Is / only
        thePath = 0;                         // Return NULL
    }
    else {
        // Is an absolute path. Skip first /.
        thePath = extractString((char *) (firstPos + 1),
                                lastPos - firstPos);
        if ((lastPos = strrchr(thePath, '/')) != 0) {
            // Found a path. Insert -1 as terminator.
            *lastPos = '\xff';
            *(lastPos + 1) = '\0';
            while ((firstPos = strchr(thePath, '/')) != 0) {
                // Insert -1 as separator.
                *firstPos = '\xff';
            }
        }
        else {
            // Is /index.html
            delete thePath;
            thePath = 0; // Return NULL
        }
    }
    return thePath;
}

bool HTTPServer::gotWholeHeader(byte *theData, udword theLength) {
    udword index = 0;
    bool lenFound = false;
    const char *aSearchString = "\r\n\r\n";
    while ((index++ < theLength) && !lenFound) {
        lenFound = (strncmp(theData + index,
                            aSearchString,
                            strlen(aSearchString)) == 0);
    }
    return lenFound;
}

//----------------------------------------------------------------------------
//
// Will look for the 'Content-Length' field in the request header and convert
// the length to a udword
// theData is a pointer to the request. theLength is the total length of the
// request.
//
udword
HTTPServer::contentLength(char *theData, udword theLength) {
    udword index = 0;
    bool lenFound = false;
    const char *aSearchString = "Content-Length: ";
    while ((index++ < theLength) && !lenFound) {
        lenFound = (strncmp(theData + index,
                            aSearchString,
                            strlen(aSearchString)) == 0);
    }
    if (!lenFound) {
        return 0;
    }
    trace << "Found Content-Length!" << endl;
    index += strlen(aSearchString) - 1;
    char *lenStart = theData + index;
    char *lenEnd = strchr(theData + index, '\r');
    char *lenString = this->extractString(lenStart, lenEnd - lenStart);
    udword contLen = atoi(lenString);
    trace << "lenString: " << lenString << " is len: " << contLen << endl;
    delete[] lenString;
    return contLen;
}

//----------------------------------------------------------------------------
//
// Decode user and password for basic authentication.
// returns a decoded string that must be deleted by the caller.
//
char *
HTTPServer::decodeBase64(char *theEncodedString) {
    static const char *someValidCharacters =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

    int aCharsToDecode;
    int k = 0;
    char aTmpStorage[4];
    int aValue;
    char *aResult = new char[80];

    // Original code by JH, found on the net years later (!).
    // Modify on your own risk.

    for (unsigned int i = 0; i < strlen(theEncodedString); i += 4) {
        aValue = 0;
        aCharsToDecode = 3;
        if (theEncodedString[i + 2] == '=') {
            aCharsToDecode = 1;
        }
        else if (theEncodedString[i + 3] == '=') {
            aCharsToDecode = 2;
        }

        for (int j = 0; j <= aCharsToDecode; j++) {
            int aDecodedValue;
            aDecodedValue = strchr(someValidCharacters, theEncodedString[i + j])
                            - someValidCharacters;
            aDecodedValue <<= ((3 - j) * 6);
            aValue += aDecodedValue;
        }
        for (int jj = 2; jj >= 0; jj--) {
            aTmpStorage[jj] = aValue & 255;
            aValue >>= 8;
        }
        aResult[k++] = aTmpStorage[0];
        aResult[k++] = aTmpStorage[1];
        aResult[k++] = aTmpStorage[2];
    }
    aResult[k] = 0; // zero terminate string

    return aResult;
}

//------------------------------------------------------------------------
//
// Decode the URL encoded data submitted in a POST.
//
char *
HTTPServer::decodeForm(char *theEncodedForm) {
    char *anEncodedFile = strchr(theEncodedForm, '=');
    anEncodedFile++;
    char *aForm = new char[strlen(anEncodedFile) * 2];
    // Serious overkill, but what the heck, we've got plenty of memory here!
    udword aSourceIndex = 0;
    udword aDestIndex = 0;

    while (aSourceIndex < strlen(anEncodedFile)) {
        char aChar = *(anEncodedFile + aSourceIndex++);
        switch (aChar) {
            case '&':
                *(aForm + aDestIndex++) = '\r';
                *(aForm + aDestIndex++) = '\n';
                break;
            case '+':
                *(aForm + aDestIndex++) = ' ';
                break;
            case '%':
                char aTemp[5];
                aTemp[0] = '0';
                aTemp[1] = 'x';
                aTemp[2] = *(anEncodedFile + aSourceIndex++);
                aTemp[3] = *(anEncodedFile + aSourceIndex++);
                aTemp[4] = '\0';
                udword anUdword;
                anUdword = strtoul((char *) &aTemp, 0, 0);
                *(aForm + aDestIndex++) = (char) anUdword;
                break;
            default:
                *(aForm + aDestIndex++) = aChar;
                break;
        }
    }
    *(aForm + aDestIndex++) = '\0';
    return aForm;
}








/************** END OF FILE http.cc *************************************/
