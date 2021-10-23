#include "common.h"
#include "FIFOreqchannel.h"
#include <sys/wait.h>
#include <chrono>
using namespace std::chrono;

using namespace std;

int main(int argc, char *argv[]){

	auto start = high_resolution_clock::now();
	int opt;
	int p = 1;
	double t = 0.0;
	int e = 1;
	string filename = "";
	int buffercapacity = MAX_MESSAGE;
	string bcapstring = "236";
	bool isNewChan = false;
	// take all the arguments first because some of these may go to the server
	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		switch (opt) {
			case 'f':
				filename = optarg;
				cout << "filename: " << filename << endl;
				break;
			case 'p':
				p = atoi(optarg);
				// cout << "p: " << p << endl;
				break;
			case 't':
				t = atof(optarg);
				// cout << "t: " << t << endl;
				break;
			case 'm':
				bcapstring = optarg;
			 	buffercapacity = atoi(optarg);
				// cout << "m: " << optarg << endl; 
				break;
			case 'e':
				e = atoi(optarg);
				break;
			case 'c':
				isNewChan = true;
				// cout << "c: " << isNewChan << endl;
				break;
		}
	}

	int pid = fork ();
	if (pid < 0){
		EXITONERROR ("Could not create a child process for running the server");
	}
	if (!pid){ // The server runs in the child process
		char serverName[] = "./server";
		char* args[] = {"./server", "-m",(char*)to_string(buffercapacity).c_str(), nullptr};
		
		if (execvp(args[0], args) < 0){
			EXITONERROR ("Could not launch the server");
		}
	}

	FIFORequestChannel* chan = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
	FIFORequestChannel* chanTemp;
	if (isNewChan){
		Request nc(NEWCHAN_REQ_TYPE);
		chan->cwrite(&nc, sizeof(nc));
		char chanName [1024];
		chan->cread(chanName, sizeof(chanName));
		FIFORequestChannel * newchan = new FIFORequestChannel(chanName,FIFORequestChannel::CLIENT_SIDE);

		chanTemp = chan;
		chan = newchan;
	}
	if (t == 0.0 && e == 1 && filename == ""){
		ofstream myfile;
		myfile.open ("compare.txt");
		int lineIndex = 1;
		for (int i = 0; i < 1000;i++){
			DataRequest d(p, i * 0.004, 1);
			chan->cwrite(&d, sizeof(DataRequest));
			double reply;
			chan->cread (&reply, sizeof(double)); //answer

			// cout<< reply;
			if (isValidResponse(&reply)){
				// cout << "line: " << lineIndex << " :" << reply << endl;
				// lineIndex++;
				myfile << i * 0.004 << "," << reply << ",";
				// cout << "For person " << p <<", at time " << t << ", the value of ecg "<< e <<" is " << reply << endl;
				// exit(0);
			}			

			DataRequest d2(p, i * 0.004, 2);
			chan->cwrite(&d2, sizeof(DataRequest));
			double reply2;
			chan->cread (&reply2, sizeof(double)); //answer

			// cout<< reply;
			if (isValidResponse(&reply2)){
				// cout << "line: " << lineIndex << " :" << reply << endl;
				// lineIndex++;
				myfile << reply2 << endl;
				// cout << "For person " << p <<", at time " << t << ", the value of ecg "<< e <<" is " << reply << endl;
				// exit(0);
			}		
		}
		myfile.close();
	}
	// cout << "test1 " << endl;
	if (filename == ""){
		DataRequest d (p, t, e);

		chan->cwrite (&d, sizeof (DataRequest)); // question
		double reply;

		chan->cread (&reply, sizeof(double)); //answer

		if (isValidResponse(&reply)){
			cout << "For person " << p <<", at time " << t << ", the value of ecg "<< e <<" is " << reply << endl;
		}
	}
	else{

	
	
	/* this section shows how to get the length of a file
	you have to obtain the entire file over multiple requests 
	(i.e., due to buffer space limitation) and assemble it
	such that it is identical to the original*/
	FileRequest fm (0,0);
	int len = sizeof (FileRequest) + filename.size()+1;
	char buf2 [len];
	memcpy (buf2, &fm, sizeof (FileRequest));
	strcpy (buf2 + sizeof (FileRequest), filename.c_str());
	chan->cwrite (buf2, len);  
	int64 filelen;
	chan->cread (&filelen, sizeof(int64));
	if (isValidResponse(&filelen)){
		cout << "File length is: " << filelen << " bytes" << endl;
	}

	int64 rem = filelen;
	FileRequest* f = (FileRequest*) buf2;
	
	filename = "received/" + filename;	
	ofstream of(filename);
	char recvbuf [buffercapacity];
	cout << filename;
	while (rem > 0 && filename != "received/"){
		f->length = min(rem, (int64) buffercapacity);
		chan->cwrite(buf2,len);
		chan->cread(recvbuf, buffercapacity);
		of.write(recvbuf, f->length);
		rem -= f->length;
		f->offset += f->length;
		if (f->offset + f->length > filelen){
			f->length = filelen - f->offset;
		}
	}
	}
	if (isNewChan){
		
		// DataRequest d (p, t, e);
		// chan->cwrite(&d, sizeof(d));
		// double response;
		// chan->cread(&response, sizeof(double));
		// cout << "Test data point: " << response << endl;


		Request q (QUIT_REQ_TYPE);
    	chanTemp->cwrite (&q, sizeof (Request));
	}

	
	// closing the channel    
    Request q (QUIT_REQ_TYPE);
    chan->cwrite (&q, sizeof (Request));
	// client waiting for the server process, which is the child, to terminate
	wait(0);
	cout << "Client process exited" << endl;

	auto stop = high_resolution_clock::now();
	auto duration = duration_cast<microseconds> (stop-start);

	cout << "time: " << duration.count() << " microseconds"<< endl;
}
