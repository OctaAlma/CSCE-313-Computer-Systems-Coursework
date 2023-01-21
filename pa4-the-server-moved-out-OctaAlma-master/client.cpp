// your PA3 client code here

// Update ooptarg loop to have a:r for ipaddress and the port
// change every instance of FIFO request channel to TCP request channel

// DO NOT RUN SERVER AS A CHILD. SERVER SHOULD BE RUN SEPARATELY
// while testing, use 'localhost' as ipaddress
// Any port greater than 2000 should be cool

// Testing on two machines: Mary will set her machine up as a server

#include <fstream>
#include <iostream>
#include <thread>
#include <sys/time.h>
#include <sys/wait.h>

#include "BoundedBuffer.h"
#include "common.h"
#include "Histogram.h"
#include "HistogramCollection.h"
#include "TCPRequestChannel.h"

// ecgno to use for datamsgs
#define EGCNO 1

using namespace std;

struct Pair{
    int patientNo = -1;
    double ecg = -1;
    Pair(){
        patientNo = -1;
        ecg = -1;
    }
    Pair(int pNo, double val){
        patientNo=pNo;
        ecg = val;
    }
};

int numDataMessages = 0;

/*TODO: add necessary arguments */
void patient_thread_function (int patientNum, int numRequests, BoundedBuffer* reqBuff) {
    // functionality of the patient threads

    // Iterate over n for the given patient (one of the arguments will be which patient we are referring to)
    double curr_t = 0.0;

    for (int i = 0; i < numRequests; i++){
        // Create the datamsg
        datamsg reqData(patientNum, curr_t, EGCNO);

        // Push the datamsg to the request buffer
        reqBuff->push((char*)&reqData, sizeof(datamsg));

        // Increment t by 0.004
        curr_t += 0.004;
    }

}

/*TODO: add necessary arguments */
void file_thread_function (string fileName, BoundedBuffer* reqBuff, TCPRequestChannel* fileChannel, int maxMsgSize) {
    // functionality of the file thread
    int lengthOfFileMsgPlusFileName = sizeof(filemsg) + strlen(fileName.c_str()) + 1;

    char* dataReq = new char[lengthOfFileMsgPlusFileName];
    FILE* dest = fopen(((char*)(("received/"+fileName).c_str())),"w");
    fclose(dest);
    // Data request contains a file message and the name of the file.
    // We will send it as a file message with (0,0) to get the size of the file.

    // Whenever we push a filemsg of values (0,0), the server responds with the file size
    filemsg giveMeSize(0,0);
    memcpy(dataReq, &giveMeSize, sizeof(filemsg));
    strcpy(dataReq + sizeof(filemsg), fileName.c_str());
    fileChannel->cwrite(dataReq, lengthOfFileMsgPlusFileName);

    int64_t fileSize;
    fileChannel->cread(&fileSize, sizeof(int64_t));

    int64_t currOffset = 0;
    int64_t remaining = fileSize;

    // Iterate through the file, creating file requests
    for (int i = 0; i < (fileSize/maxMsgSize)+1; i++){
        filemsg* fileRequest = (filemsg*)dataReq;

        fileRequest->offset = currOffset;

        if (remaining >= maxMsgSize){
			fileRequest->length = maxMsgSize; // set the length. CAREFUL WITH THE LAST SEGMENT 
		}else{
			fileRequest->length = remaining;
		}

        // Instead of pushing to server, push to the request buffer
        reqBuff->push((char*)fileRequest, sizeof(filemsg));
        currOffset += maxMsgSize;
		remaining -= maxMsgSize;
    }

    delete [] dataReq;
}

/*TODO: add necessary arguments */
void worker_thread_function (string fileName, TCPRequestChannel* workerChan, BoundedBuffer* requestBuff, BoundedBuffer* responseBuff, int maxMsgSize) {
    // functionality of the worker threads
    Pair patientNoAndReply;

    char* currRequest = new char[maxMsgSize];  
    char* currResponse = new char[maxMsgSize];

    filemsg* currFileMsg;
    datamsg* currDataMsg;

    int reqSize, responseSize;
    MESSAGE_TYPE* msgType;

    while (true){
        memset(currRequest,0,maxMsgSize);
        // pop from the request buffer into a char* array (lets suppose its called "req")
        reqSize = requestBuff->pop(currRequest, maxMsgSize);

        // To find out what kind of request we have, cast char* into a MESSAGE_TYPE*
        msgType = (MESSAGE_TYPE*)currRequest;

        //TODO: Data request/file request handling

        // If we sent a file request, receive the buffer from the server and write the response to the file
        //      - We have to know which file we are writing to by passing it as an argument or ptr arithmetic
        //      - To find the offset, we can cast req into a filemsg* and get the "offset" variable value with pointer arithmetic
        //      - Once you know the offset, use seek to offset in the file
        if (*msgType == FILE_MSG){
            // Open the file if applicable:
            FILE* dest = nullptr;
            if (fileName.size()){ dest = fopen(((char*)(("received/"+fileName).c_str())),"r+"); };

            int lengthOfFileMsgPlusFileName = sizeof(filemsg) + strlen(fileName.c_str()) + 1;
        
            strcpy(currRequest+sizeof(filemsg), fileName.c_str());
            // Send the request (req) to the server through our channel
            workerChan->cwrite(currRequest, lengthOfFileMsgPlusFileName);
            currFileMsg = ((filemsg*)currRequest);
            
            memset(currResponse, 0, maxMsgSize);
            workerChan->cread(currResponse, currFileMsg->length);
        
            fseek(dest, currFileMsg->offset, SEEK_SET);

            fwrite(currResponse, 1, currFileMsg->length, dest);

            fclose(dest);
        }

        // If data request:
            // We will receive a double from the server
            // Then format for histogram thread. This will require knowledge on how the histogram_thread_function(int,double) works
                // int is the patient number while double is the seconds
                // We can get the patient number by casting req to datamsg and getting the person variable
            // Then we push to the response buffer
        else if (*msgType == DATA_MSG){
            currDataMsg = (datamsg*)currRequest;
            numDataMessages++;

            // FIVE HOURS OF DEBUGGING FOR THIS: 
            // OLD: workerChan->cwrite(currDataMsg, sizeof(DATA_MSG));
            workerChan->cwrite(currDataMsg, sizeof(datamsg));

            double reply;
            workerChan->cread(&reply, sizeof(double));

            Pair patientAndReply(currDataMsg->person, reply);

            responseBuff->push((char*)&patientAndReply,sizeof(Pair));
        }

        else if (*msgType == QUIT_MSG){
            break;
        }
    }

    delete [] currRequest;
    delete [] currResponse;
}

void histogram_thread_function (HistogramCollection* hist_col, BoundedBuffer* responseBuf, int maxMsgSize){
    // functionality of the histogram threads

    char* buf = new char[maxMsgSize];
    int pno;
    double val;

    // while true
    while (true){
        // pop from the response buffer
        responseBuf->pop(buf, sizeof(Pair));
        pno = ((Pair*)buf)->patientNo;
        val = ((Pair*)buf)->ecg;

        // If {-1,-1} break! 
        if ( (pno == -1) && (val == -1) ){ break; }

        // call histogram collection update 
        hist_col->update(pno, val);
    }

    delete [] buf;
}


int main (int argc, char* argv[]) {
    // Two additional arguments:
    string a = "";  // the IP address the server is running on
    string r = ""; // the port number the server is deploying its services to

    int n = 1000;	// default number of requests per "patient"
    int p = 10;		// number of patients [1,15]
    int w = 100;	// default number of worker threads
	int h = 20;		// default number of histogram threads
    int b = 20;		// default capacity of the request buffer (should be changed)
	int m = MAX_MESSAGE;	// default capacity of the message buffer
	string f = "";	// name of file to be transferred
    
    // read arguments
    int opt;
    
    // For data point transfers: a, r [with optional n, w, b, p, h, m arguments]
    // For file transfers: a, r, f [with optional w, b, m arguments]

	while ((opt = getopt(argc, argv, "a:r:f:b:m:w:n:p:h:")) != -1) {
		switch (opt) {
            // Required for both data transfers:
            case 'a':
                a = optarg;
                break;
            case 'r':
                r = optarg;
                break;
            
            // Required for file transfers:
            case 'f':
				f = optarg;
                break;
            
            // Optional arguments for both data transfers:
            case 'b':
				b = atoi(optarg);
                break;
			case 'm':
				m = atoi(optarg);
                break;
            case 'w':
				w = atoi(optarg);
                break;
            
            // Optional arguments for data point transfers:
			case 'n':
				n = atoi(optarg);
                break;
			case 'p':
				p = atoi(optarg);
                break;
			case 'h':
				h = atoi(optarg);
				break;
		}
	}
    
	// initialize overhead (including the control channel)
	TCPRequestChannel* chan = new TCPRequestChannel(a, r);
    BoundedBuffer request_buffer(b);
    BoundedBuffer response_buffer(b);
	HistogramCollection hc;

    // Create "w" worker channels by sending a new chan request to server, receiving the channel name, and calling the constructor
    // - Make sure the channels are dynamic (heap) since they are being created in a loop
    // - Delete them once you're done
    vector<TCPRequestChannel*> workerChannels;
    char chanName[30];
    for (int i = 0; i < w; i++){
        // //TODO: Create a channel for each worker
        TCPRequestChannel* workerChannel = new TCPRequestChannel(a,r);
        workerChannels.push_back(workerChannel);
    }

    // making histograms and adding to collection
    for (int i = 0; i < p; i++) {
        Histogram* h = new Histogram(10, -2.0, 2.0);
        hc.add(h);
    }
	
	// record start time
    struct timeval start, end;
    gettimeofday(&start, 0);

    /************************************* CREATING ALL THREADS ******************************************/

    // create w worker threads
    vector<thread> workerThreads;
    for (int i = 0; i < w; i++){
        workerThreads.push_back(thread(worker_thread_function, f, workerChannels.at(i), &request_buffer, &response_buffer, m));
    }

    vector<thread> patientThreads;
    vector<thread> histogramThreads;

    thread* fileThread = nullptr;
    TCPRequestChannel* fileChannel;

    if (f.empty()){
        // Create h histogram threads
        for (int i = 0; i < h; i++){
            histogramThreads.push_back(thread(histogram_thread_function, &hc, &response_buffer, m));
        }

        // create p patient threads
        for (int i = 0; i < p; i++){
            patientThreads.push_back(thread(patient_thread_function, i+1, n, &request_buffer));
        }
    }else{
        // Create the channel for the file thread function and the file thread itself:
        fileChannel = new TCPRequestChannel(a, r);
        fileThread = new thread(file_thread_function, f, &request_buffer, fileChannel, m);
    }

	/************************************* JOINING ALL THREADS ******************************************/
    datamsg quit(-1, -1, -1);
    quit.mtype = QUIT_MSG;

    // join the patient threads
    for (size_t i = 0; i < patientThreads.size(); i++){ patientThreads.at(i).join(); }

    // Join the file threads
    if (fileThread){
        // Join the dynamically allocated file thread then delete the file thread:
        fileThread->join();
        delete fileThread;
        // Send a quit message through the channel then delete the channel
        fileChannel->cwrite((char*)&quit, sizeof(datamsg));
        delete fileChannel;
    }

    // The worker threads have no reason to quit, so we must push "w" amount of quit messages into the request buffer
    
    for (int i = 0; i < w; i++){ request_buffer.push((char*)(&quit), sizeof(datamsg)); }
    
    // Join the worker threads
    for (int i = 0; i < w; i++){ workerThreads.at(i).join(); }
    // Delete the dynamically allocated worker pipes:
    for (int i = 0; i < w; i++){ 
        MESSAGE_TYPE qMess = QUIT_MSG;
        workerChannels.at(i)->cwrite(&qMess, sizeof(MESSAGE_TYPE));
        delete workerChannels.at(i); 
    }

    // Join the histogram threads
    // push {-1,-1} "h" times to the response buffer
    Pair exitPair;
    for (size_t i = 0; i < histogramThreads.size(); i++){ response_buffer.push((char*)&exitPair, sizeof(Pair));}
    for (size_t i = 0; i < histogramThreads.size(); i++) { histogramThreads.at(i).join(); }

	// record end time
    gettimeofday(&end, 0);

    // print the results
	if (f == "") {
		hc.print();
	}
    int secs = ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) / ((int) 1e6);
    int usecs = (int) ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) % ((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

	// quit and close control channel
    MESSAGE_TYPE q = QUIT_MSG;
    chan->cwrite ((char *) &q, sizeof (MESSAGE_TYPE));
    cout << "All Done!" << endl;
    delete chan;

	// wait for server to exit
	wait(nullptr);
}
