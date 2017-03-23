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

using namespace std;

// Prototypes
void parse_config(string fileName, params &paramO);
bool test_digit(string word);
void* timer_func(void*);
void* threadFetch(void*);
void* threadParse(void*);
string our_curl(string website);
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);
string get_time(void);
void* parseResults(void*);


// Structs
struct MemoryStruct {
  char *memory;
  size_t size;
};

struct queueDObject{
	string data;
	string URL;
};


// Global Variables
queue<string> sitesQ;
queue<queueDObject> dataQ;
pthread_mutex_t sitesQLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t dataQLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t resultsLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t sitesQCond = PTHREAD_COND_INITIALIZER;
pthread_cond_t dataQCond = PTHREAD_COND_INITIALIZER;
pthread_cond_t resultsCond = PTHREAD_COND_INITIALIZER;
string results;
int nDone;
bool Running;

int main(int argc, char *argv[])
{
	string configFN;// Configuration File name
	params paramO; // Params objects
	Running = true;
	if(argc == 2)
	{
		configFN = argv[1];
	}
	else
	{
		cout << "Usage: " << argv[0] << " configuration_file_name" << endl;
	}
	
	parse_config(configFN, paramO);
	
	// Initialize curl
	curl_global_init(CURL_GLOBAL_ALL);
	

	// Launch Timer
	pthread_t timer_t;
	pthread_create(&timer_t,NULL, timer_func, &paramO);


	// Launch Fetch Threads
	pthread_t threadFetch_t;
	pthread_create(&threadFetch_t, NULL, threadFetch, NULL);


	// Launch Parse Threads
	pthread_t threadParse_t;
	pthread_create(&threadParse_t, NULL, threadParse, &paramO);
	
	// Launch Results printer
	pthread_t parseResults_t;
	pthread_create(&parseResults_t, NULL, parseResults, &paramO);

	// Close curl	
	curl_global_cleanup();
}


void *timer_func(void *arg)
{
	params * paramsO = (params *) arg;
	while(Running)
	{
		pthread_mutex_lock(&sitesQLock);
		for(string site :paramsO->sitesV)
		{
			sitesQ.push(site);
			pthread_cond_signal(&sitesQCond);
		}
		pthread_mutex_unlock(&sitesQLock);
		sleep(paramsO->period);
	}
}

void* threadFetch(void* arg)
{
	string item;
	string data;
	
	while (Running)
	{
		struct queueDObject dqo;
		pthread_mutex_lock(&sitesQLock);	
		while(sitesQ.empty())
			pthread_cond_wait(&sitesQCond, &sitesQLock);
		item = sitesQ.front();
		sitesQ.pop();
		pthread_mutex_unlock(&sitesQLock);
		data = our_curl(item);
		dqo.data = data;
		dqo.URL = item;
		pthread_mutex_lock(&dataQLock);
		dataQ.push(dqo);
		pthread_cond_signal(&dataQCond);
		pthread_mutex_unlock(&dataQLock);
	}
}

void* threadParse(void* arg)
{
	params* p = (params*) arg;
	
	while(Running)
	{
		pthread_mutex_lock(&dataQLock);
		while(dataQ.empty())
			pthread_cond_wait(&dataQCond, &dataQLock);
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
			pos = dqo.data.find(term);
			while(pos != string::npos)
			{
				count += 1;
				current = dqo.data.substr(pos + 1);
				pos = current.find(term);
			}
			lresult.append(get_time());
			lresult.append(",");
			lresult.append(term);
			lresult.append(",");
			lresult.append(dqo.URL);
			lresult.append(",");
			lresult.append(to_string(count));
			lresult.append("\n");
	
		}

		// write in results
		pthread_mutex_lock(&resultsLock);
		results += lresult;
		nDone += 1;
		pthread_cond_signal(&resultsCond);
		pthread_mutex_unlock(&resultsLock);
	}

}

void* parseResults(void *arg)
{
	int iteration = 0;
	results = "";
	
	params *paramO = (params *) arg;
	while(Running)
	{
		iteration += 1;
		pthread_mutex_lock(&resultsLock);
		while(nDone < paramO->sitesV.size())
		{
			pthread_cond_wait(&resultsCond, &resultsLock);
		}
		// Results should now be in global results string
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

 
string our_curl(string website)
{
  CURL *curl_handle;
  CURLcode res;
  string data;
 
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
 
  /* get it! */ 
  res = curl_easy_perform(curl_handle);
  
  /* cleanup curl stuff */ 
  curl_easy_cleanup(curl_handle);
 
  /* check for errors */ 
  if(res != CURLE_OK) {
    cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << endl;
    free(chunk.memory);
    return string("");
  }
  else {
    /*
     * Now, our chunk.memory points to a memory block that is chunk.size
     * bytes big and contains the remote file.
     *
     * Do something nice with it!
     */ 
    data.assign(chunk.memory, chunk.size);
    free(chunk.memory);
    return data;
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
