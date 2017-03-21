#include <string>
#include <vector>
#include <fstream>

using namespace std;
class params
{
	public:
	// Data members
	int period;
	int numFetch;
	int numParse;
	string searchFile;
	string siteFile;
	vector<string> sitesV;
	vector<string> searchesV;
	// Functions
	params(void)
	{
		period = 180;
		numFetch = 1;
		numParse = 1;
		searchFile = "Search.txt";
		siteFile = "Sites.txt";
	}
	bool parseSearchFile()
	{
		ifstream s(searchFile);
		string line;
		if(s.is_open())
		{
			while(getline(s, line))
			{
				if(line.length() > 0)
					searchesV.push_back(line);
			}
		}
		else
		{
			return false;
		}
		return true;
	}
	bool parseSiteFile()
	{
		ifstream s(siteFile);
		string line;
		if(s.is_open())
		{
			while(getline(s, line))
			{
				if(line.length() > 0)
					sitesV.push_back(line);
			}
		}
		else
		{
			return false;
		}
		return true;
	}
};


