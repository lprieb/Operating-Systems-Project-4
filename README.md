# Operating-Systems-Project-4
Producer/Consumer design using Locks and Condition Variables to monitor a set of webpages

# Members
Luis Prieb (lprieb), Emily Park (epark3)

# Description

This program uses a series of threads to  monitor a list of sites and check whether each site contains any of a list of search terms. The program takes a single parameter, which is a configuration file. The configuration takes a parameter per line in the format PARAMETER=VALUE. If a parameter is not provided, default values are assumed. Even if no parameter is changed, an empty config file should be provided. Below are all the accepted parameter names with their meanings and default values:

Parameter	Description							Default
PERIOD_FETCH	The time (in seconds) between fetches of the various sites	180
NUM_FETCH	Number of fetch threads (0 to 8)				1
NUM_PARSE	Number of parsing threads (0 to 8)				1
SEARCH_FILE	File containing the search strings				Search.txt
SITE_FILE	File containing the sites to query				Sites.txt


Internally the program works as follows. There are 4 types of threads: a timer thread, fetch threads, parser threads, and a result printer thread. The timer thread puts the sites in a queue that the fetch threads pop from to parse a site. It will do this every period of the time provided in the config file. The fetch threads use the libcurl library to get data from the current website in question and put the data into another queue that will be processed later. The parser threads read from the data queue and check for all instances of the search terms. The results are put into one final queue for results. The number of parser threads and fetch threads should be specified in the configuration file. The last thread waits for all the processing to finish using a global variable that all threads count up in. When all threads are done, a global result variable is written into a file. 

# Installing

A make file is provided for simple installion. Simply run

$ make

# Running

To run the program simply run the program with the config file. For example, if the config file is config.txt and the executable is called project4, run the program as follows

$ ./project4 config.txt

To close the program, send the SIGINT signal using Ctrl+C.

# Output

Each time a result is ready, the result is written into a file called #.csv, where # is the current iteration of the program. Each line in the file will be formatted as follows:

Time/Date,SearchTerm,Site,WordCount


# Possible Errors

If a period that is too short for the process to finish is given, undefined behavior occurs.


