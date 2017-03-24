#include <iostream>
#include <string>
#include "params.h"
#include <fstream>
#include <cctype>
#include <vector>
#include <cerrno>
#include <cstring>
#include <queue>
#include <unistd.h>
#include <pthread.h>
#include <curl/curl.h>
#include <ctime>
#include <csignal>

using namespace std;

// Structs
struct MemoryStruct {
  char *memory;
  size_t size;
};

typedef struct our_string {
	string data;
	bool success;
} our_string;

struct queueDObject{
	string data;
	string URL;
};

// Prototypes
void parse_config(string fileName, params &paramO);
bool test_digit(string word);
void* timer_func(void*);
void* threadFetch(void*);
void* threadParse(void*);
our_string our_curl(string website);
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);
string get_time(void);
void* parseResults(void*);
string form_time(struct tm*);
void quit_handler(int s);
void alarmHandler(int sig);



// Global Variables
queue<string> sitesQ;
queue<queueDObject> dataQ;
pthread_mutex_t sitesQLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t dataQLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t resultsLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t alarmLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t sitesQCond = PTHREAD_COND_INITIALIZER;
pthread_cond_t dataQCond = PTHREAD_COND_INITIALIZER;
pthread_cond_t resultsCond = PTHREAD_COND_INITIALIZER;
pthread_cond_t alarmCond = PTHREAD_COND_INITIALIZER;
string results;
int nDone;
bool Running;
params paramO;

int main(int argc, char *argv[])
{
	string configFN;// Configuration File name
	Running = true;
	if(argc == 2)
	{
		configFN = argv[1];
	}
	else
	{
		cout << "Usage: " << argv[0] << " configuration_file_name" << endl;
		exit(1);
	}
	
	parse_config(configFN, paramO);	

	// Set signal to catch ctrl+c
	signal(SIGINT, quit_handler);

	// Initialize curl
	curl_global_init(CURL_GLOBAL_ALL);	

	// Launch Timer
	pthread_t timer_t;
	pthread_create(&timer_t, NULL, timer_func, &paramO);

	// Launch Fetch Threads
	pthread_t *threadFetchArray = new pthread_t[sizeof(pthread_t) * paramO.numFetch];

	for(int i=0; i<paramO.numFetch; i++)
	{
		pthread_create(&threadFetchArray[i], NULL, threadFetch, NULL);
	}

	// Launch Parse Threads
	pthread_t *threadParseArray = new pthread_t[sizeof(pthread_t) * paramO.numParse];
	for(int i=0; i<paramO.numParse; i++)
	{
		pthread_create(&threadParseArray[i], NULL, threadParse, &paramO);
	}

	// Launch Results printer
	pthread_t parseResults_t;
	pthread_create(&parseResults_t, NULL, parseResults, &paramO);

	// Join threads
	pthread_join(timer_t, NULL);
	for(int i=0; i<paramO.numParse; i++)
	{
		pthread_join(threadParseArray[i], NULL);
	}
	for(int i=0; i<paramO.numFetch; i++)
	{
		pthread_join(threadFetchArray[i], NULL);
	}
	pthread_join(parseResults_t, NULL);
	

	// Clean up
	delete threadFetchArray;
	delete threadParseArray;

	// Close curl	
	curl_global_cleanup();
}

void quit_handler(int s)
{
	cout << "Quitting..." << endl;
	// Signal all conds
	Running = false;
	pthread_cond_broadcast(&sitesQCond);
	pthread_cond_broadcast(&dataQCond);
	pthread_cond_broadcast(&resultsCond);
	pthread_cond_broadcast(&alarmCond);
	alarm(0);
}

void alarmHandler(int sig)
{
	pthread_cond_signal(&alarmCond);
	cout << "TIMER_FUNC" << endl;
	cout << "Timer Reached 0: Searching all sites..." << endl;
	pthread_mutex_lock(&sitesQLock);
	for(string site :paramO.sitesV)
	{
		sitesQ.push(site);
		pthread_cond_signal(&sitesQCond);
	}
	pthread_mutex_unlock(&sitesQLock);
}

void *timer_func(void *arg)
{
	params * paramsO = (params *) arg;
	signal(SIGALRM, alarmHandler);
	alarmHandler(0);
	while(Running)
	{
		pthread_mutex_lock(&alarmLock);
		alarm(paramsO->period);
		pthread_cond_wait(&alarmCond, &alarmLock);
		pthread_mutex_unlock(&alarmLock);
	}
}

// Curl websites
void* threadFetch(void* arg)
{
	string item;
	our_string ret_data;

	while (Running)
	{
		struct queueDObject dqo;
		pthread_mutex_lock(&sitesQLock);	
		while(sitesQ.empty() && Running)
			pthread_cond_wait(&sitesQCond, &sitesQLock);
		if(!Running) // We add this extra test to avoid SEG FAULTS when quitting
		{
			pthread_mutex_unlock(&sitesQLock);
			break;
		}
		item = sitesQ.front();
		sitesQ.pop();
		pthread_mutex_unlock(&sitesQLock);
		
		ret_data = our_curl(item);
		if( !ret_data.success)
		{
			cout << "\tSkipping " << item << "..." << endl;
			continue;
		}
		dqo.data = ret_data.data;
		dqo.URL = item;
		
		pthread_mutex_lock(&dataQLock);
		dataQ.push(dqo);
		pthread_cond_signal(&dataQCond);
		pthread_mutex_unlock(&dataQLock);
	}
}

// Search for search term in the website
void* threadParse(void* arg)
{
	params* p = (params*) arg;

	while(Running)
	{
		pthread_mutex_lock(&dataQLock);
		while(dataQ.empty() && Running)
			pthread_cond_wait(&dataQCond, &dataQLock);
		if(!Running)
		{
			pthread_mutex_unlock(&dataQLock);
			break;
		}
		
		queueDObject dqo = dataQ.front();
		dataQ.pop();
		pthread_mutex_unlock(&dataQLock);

		size_t pos;
		size_t count;
		string current = dqo.data.c_str();
		string lresult = "";

		for(string term : p->searchesV)
		{
			count = 0;
			pos = dqo.data.find(term); // returns position of 'term' in 'dqo.data'
			while(pos != string::npos)
			{
				count += 1;
				current = current.substr(pos + 1);
				pos = current.find(term);
			}
			time_t rawtime;
			struct tm * myTM;
			time(&rawtime);
			myTM = localtime(&rawtime);
			lresult.append(form_time(myTM));
			lresult.append(",");
			lresult.append(term);
			lresult.append(",");
			lresult.append(dqo.URL);
			lresult.append(",");
			lresult.append(to_string(count));
			lresult.append("\n");
			if(!Running)
				break;
		}


		// write in results
		pthread_mutex_lock(&resultsLock);
		results += lresult;
		nDone += 1;
		pthread_cond_signal(&resultsCond);
		pthread_mutex_unlock(&resultsLock);
	}

}

// Write results to csv file
void* parseResults(void *arg)
{
	int iteration = 0;
	results = "";
	
	params *paramO = (params *) arg;
	while(Running)
	{
		iteration += 1;
		pthread_mutex_lock(&resultsLock);
		while(nDone < paramO->sitesV.size()  && Running)
		{
			pthread_cond_wait(&resultsCond, &resultsLock);
		}
		// Results should now be in global string 'results'. Write to csv file
		string filename = "";
		filename.append(to_string(iteration));
		filename.append(".csv");
		ofstream rfile(filename);
		rfile.write(results.c_str(), results.size());
		results = "";
		nDone = 0;
		pthread_mutex_unlock(&resultsLock);
	}
}

// Returns string of time
string form_time(struct tm* t)
{
	string s = "";
	s.append(to_string(t->tm_hour));
	s.append(":");
	s.append(to_string(t->tm_min));
	s.append(":");
	s.append(to_string(t->tm_sec));
	s.append(" ");
	s.append(to_string(t->tm_mon + 1));
	s.append("/");
	s.append(to_string(t->tm_mday));
	s.append("/");
	s.append(to_string(t->tm_year + 1900));
	return s;
}

// Parse configuration text file
void parse_config(string fileName, params &paramO)
{
	string line;
	size_t eqIndex;
	ifstream configFile(fileName, ios::in);
	if(configFile.is_open())
	{
		while( getline(configFile,line))
		{
				eqIndex = line.find('=');
				if(eqIndex != -1)
				{
					string paramName= line.substr(0,eqIndex);
					string val = line.substr(eqIndex+1);
					if(paramName== "PERIOD_FETCH")
					{
						if(test_digit(val))
						{
							paramO.period = atoi(val.c_str());
						}
						else
						{
							cout << "The value for PERIOD_FETCH is not valid" << endl;
							exit(1);
						}
					}
					else if(paramName ==  "NUM_FETCH")
					{
						if(test_digit(val))
						{
							paramO.numFetch = atoi(val.c_str());
							if(paramO.numFetch > 8 || paramO.numFetch < 1)
							{
								cout << " Incorrent value for NUM_FETCH. Must be between 1 and 8. Setting value to 1" << endl;
								paramO.numFetch = 1;
							}
						}
						else
						{
							cout << "The value for NUM_FETCH is not valid" << endl;
							exit(1);
						}
					}
					else if(paramName == "NUM_PARSE")
					{
						if(test_digit(val))
						{
							paramO.numParse = atoi(val.c_str());
							if(paramO.numParse > 8 || paramO.numParse < 1)
							{
								cout << " Incorrent value for NUM_PARSE. Must be between 1 and 8. Setting value to 1" << endl;
								paramO.numParse = 1;
							}
						}
						else
						{
							cout << "The value of NUM_PARSE is not valid" << endl;
							exit(1);
						}
					}
					else if(paramName == "SEARCH_FILE")
					{
						if(val.length() > 0)
						{
							paramO.searchFile = val;
							
						}
						else
						{
							cout << "The value of SEARCH_FILE is not valid" << endl;
							exit(1);
						}
					}
					else if(paramName == "SITE_FILE")
					{
						if(val.length() >0)
						{
							paramO.siteFile = val;
						}
						else
						{
							cout << "The value of SITE_FILE is not valid" << endl;
							exit(1);
						}
					}
					else
					{
						cout << "WARNING: One of the parameters is not recognized" << endl;
					}
				}
				else
				{
					cout << "Incorrectly formated Parameterfile." << endl;
					cout << "Quiting..." << endl;
					exit(1);
				}
		} 
		if(!paramO.parseSiteFile())
		{
			cout << "Could not open SITE_FILE \"" << paramO.siteFile << "\": "<< strerror(errno) << endl;
			exit(1);
		}
		if(!paramO.parseSearchFile())
		{
			cout << "Could not open SEARCH_FILE \"" << paramO.siteFile << "\":"<< strerror(errno) << endl;
			exit(1);
		}
	
	}
	else
	{
		cerr << "Could not Open config file \"" << fileName << "\": " << strerror(errno) << endl;
	}

}

// Check if given string is a number
bool test_digit(string word)
{
	if(word.length() < 1)
	{
		return false;
	}
	for(int i = 0; i < word.length(); i++)
	{
		if(!isdigit(word[i]) && word[i] != '-')
			return false;
	}
	return true;
}

// Part of Curl
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
 
  mem->memory = (char *)realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL) {
    /* out of memory! */ 
    cout << "not enough memory (realloc returned NULL)\n";
    return 0;
  }
 
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
 
  return realsize;
}

 // Curl, but edited to return our_string so we can check for success in threadFetch and cleanup in main
our_string our_curl(string website)
{
  CURL *curl_handle;
  CURLcode res;
 
  struct MemoryStruct chunk;
 
  chunk.memory = ((char*) malloc(1));  /* will be grown as needed by the realloc above */ 
  chunk.size = 0;    /* no data at this point */ 
 
 
  /* init the curl session */ 
  curl_handle = curl_easy_init();
 
  /* specify URL to get */ 
  curl_easy_setopt(curl_handle, CURLOPT_URL, website.c_str());
 
  /* send all data to this function  */ 
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
 
  /* we pass our 'chunk' struct to the callback function */ 
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
 
  /* some servers don't like requests that are made without a user-agent
     field, so we provide one */ 
  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
  //curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 10L);

  //curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, "libcurl-agent/1.0");

  /* get it! */ 
  res = curl_easy_perform(curl_handle);
  
  /* cleanup curl stuff */ 
  curl_easy_cleanup(curl_handle);
 
  /* check for errors */ 
  if(res != CURLE_OK) {
    cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << endl;
    free(chunk.memory);
    our_string s;
    s.data = "";
    s.success = false;
    return s;
  }
  else {
    /*
     * Now, our chunk.memory points to a memory block that is chunk.size
     * bytes big and contains the remote file.
     *
     * Do something nice with it!
     */ 
    our_string s;
    s.data.assign(chunk.memory, chunk.size);
    s.success = true;
    free(chunk.memory);
    return s;
  }
  
 
}


string get_time()
{
	time_t rawtime;
	struct tm * timeinfo;

	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	return string(asctime(timeinfo));

}
