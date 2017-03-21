#include <iostream>
#include <string>
#include "params.h"
#include <fstream>
#include <cctype>
#include <vector>
#include <cerrno>
#include <cstring>

using namespace std;

// Prototypes
void parse_config(string fileName, params &paramO);
bool test_digit(string word);


int main(int argc, char *argv[])
{
	string configFN;// Configuration File name
	params paramO; // Params objects
	if(argc == 2)
	{
		configFN = argv[1];
	}
	else
	{
		cout << "Usage: " << argv[0] << " configuration_file_name" << endl;
	}
	
	parse_config(configFN, paramO);
	
	// Print read sites
	cout << "Printing Sites" << endl;
	for(string s: paramO.sitesV)
	{
		cout << s << endl;
	}
	// Print read search terms
	cout << "Printing Search terms" << endl;
	for(string s: paramO.searchesV)
	{
		cout << s << endl;
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

