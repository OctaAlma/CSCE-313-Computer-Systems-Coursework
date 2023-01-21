/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Octavio Almanza
	UIN: 628005492
	Date: 9/15/2022
*/
#include "common.h"
#include "FIFORequestChannel.h"

using namespace std;


int main (int argc, char *argv[]) {
	int opt;
	int p = -1;
	double t = -1;
	int e = -1;
	int m = MAX_MESSAGE; // Enables messages to be transfered at maximum capacity
	bool newChannel = false;
	vector<FIFORequestChannel*> availableChannels;
	string filename;

	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			case 'm':
				m = atoi (optarg);
				break;
			case 'c':
				newChannel = true;
				break;
		}
	}
	// PARENT RUNS THE CLIENT FORK RUNS SERVER
	
	char* cmd[4] = {(char*)"./server", (char*)"-m", (char*)(to_string(m).c_str()), nullptr};
	int parentID = fork();
	if (!parentID){ // We are in the server
		/*
		MaxSize should be casted as a char* using c_str using the cstring 
		This can then be passed onto the server via execvp
		*/

		// Give arguments for the server
		// Note the server needs the "./server", "-m", "<val for -m arg>", 'NULL'
		// "-m" represents the maximum amount of bytes that the server can send to the client
		//Start the fork
		// In the child, you run execvp using the server arguments 
		execvp(cmd[0],cmd);
		
}
	FIFORequestChannel controlChan("control", FIFORequestChannel::CLIENT_SIDE);

	availableChannels.push_back(&controlChan);

	char chanName[30];

	if (newChannel){
		//TODO: CREATE A NEW CHANNEL	
		MESSAGE_TYPE nc = NEWCHANNEL_MSG;
		controlChan.cwrite(&nc, sizeof(MESSAGE_TYPE));

		// We need to remember the name of the new channel with a string or char*
		// "cread" the response from the server
		controlChan.cread(&chanName,30);

		// call the FIFORequestChannel constructor with the name of the server 
		// Use "new" so the channel can live outside of the if-statement
		FIFORequestChannel* newChan = new FIFORequestChannel(chanName, FIFORequestChannel::CLIENT_SIDE);

		// PUSH THE CHANNEL INTO THE "availableChannels" VECTOR!!!
		availableChannels.push_back(newChan);
	
	}

	FIFORequestChannel chan = *(availableChannels.back());

	char buf[MAX_MESSAGE];

	if ((p != -1) && (t != -1) && (e != -1)){
		// We only run the following code if we are asking for a single data point:	
		//TO DO: SWITCH FROM HARDCODING TO USER VALUES!!!
		datamsg x(p,t,e); //datamsg x(1, 0.0, 1); 

		memcpy(buf, &x, sizeof(datamsg));
		chan.cwrite(buf, sizeof(datamsg)); // question
		
		double reply;
		chan.cread(&reply, sizeof(double)); //answer
		cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;

	}else if ((p != -1) && (t == -1) && (e == -1) && (filename.empty())){
		// If p != -1, request 1000 data points from the server 
		// Loop over the first 1000 lines of the specified person, and request 2000 requests: cg1, cg2
		// Write the data gathered to received/x1.csv
		// Each line in the file will be of the format time,ecg1,ecg2
		ofstream oFile;
		oFile.open("received/x1.csv");
		double time = 0.0;

		double r1,r2;

		//cout << "cool" << endl;

		for (int i = 0; i < 1000; i++){

			datamsg e1(p,time,1);
			memcpy(buf, &e1, sizeof(datamsg));
			chan.cwrite(buf, sizeof(datamsg)); // question
			chan.cread(&r1, sizeof(double)); //answer

			datamsg e2(p,time,2);
			memcpy(buf, &e2, sizeof(datamsg));
			chan.cwrite(buf, sizeof(datamsg)); // question
			chan.cread(&r2, sizeof(double)); //answer

			oFile << time << ',' << r1 << ',' << r2 << '\n';

			time += 0.004;
		}

		oFile.close();
	}else if (!(filename.empty())){		
		//TODO: The last type of request is requesting a full file
		/* If the client has the -f flag, it is requesting an ENTIRE FILE from the server
		* The client can request any file from the BIMDC folder, not necessarily CSVs only!
		*  - Because of this, the client will receive chunks of a char *
		* 
		*/

		// closing the channel    

		// sending a non-sense message, you need to change this

		ofstream timingInfo;
		timingInfo.open("timingInfo" + filename + ".txt");

		filemsg fm(0, 0);
		string fname = filename;
		ofstream oFile;
		FILE* dest = fopen((char *)(((string)"received/").append(fname)).c_str(),"wb");

		int len = sizeof(filemsg) + (fname.size() + 1);
		char* buf2 = new char[len];
		memcpy(buf2, &fm, sizeof(filemsg));
		strcpy(buf2 + sizeof(filemsg), fname.c_str());
		chan.cwrite(buf2, len);  // I want the file length;

		int64_t filesize = 0;
		chan.cread(&filesize, sizeof(int64_t));

		char* buf3 = new char[m];

		// Now we know the file size, we can know how many times we are supposed to request (loop value)
		// This value is (filesize / buffer capacity (aka m))
		int64_t currOffset = 0;
		int64_t remaining = filesize;
		
		for (int i = 0; i < (filesize/m)+1; i++){
			// In the loop, create the file message instance
			filemsg* fileRequest = (filemsg*)buf2; // <-- Casting a char* into a filemsg* !!!
			
			fileRequest->offset = currOffset; // set offset in the file

			if (remaining >= m){
				fileRequest->length = m; // set the length. CAREFUL WITH THE LAST SEGMENT 
			}else{
				fileRequest->length = remaining;
			}

			chan.cwrite(buf2, len); //Receive the response

			// TODO:
			/* "cread" the server response and put it into buf3, which is of length fileRequest->length  
			*   Write buf3 into the file: "received/filename"
			*/
			chan.cread(buf3,fileRequest->length);
			fwrite(buf3,1,fileRequest->length,dest);
			currOffset += m;
			remaining-=m;
		}

		delete[] buf2;
		delete[] buf3;

		fclose(dest);
	}

	MESSAGE_TYPE mess = QUIT_MSG;

	if (newChannel){
		availableChannels.at(0)->cwrite(&mess,sizeof(MESSAGE_TYPE));
		availableChannels.at(1)->cwrite(&mess, sizeof(MESSAGE_TYPE));
		delete availableChannels.at(1);
	}else{
		chan.cwrite(&mess, sizeof(MESSAGE_TYPE));
	}

	pid_t wait();
}