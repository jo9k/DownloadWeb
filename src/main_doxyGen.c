/*
 * main.c
 *
 *  Created on: Feb 11, 2016
 *      Author: mode
 */


/*!
 ============================================================================
 Name        : DownloadWeb.cpp
 Author      : Ernest Chochołowski 362761 ; Moritz Luszek 313556
 Version     : 0.08
 Copyright   : --
 Description : Sehr gut programm!
 ============================================================================
 */

/* According to POSIX.1-2001, POSIX.1-2008 */
#include <sys/select.h>

/* According to earlier standards */
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include<arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

//defines
#define PORT 80			//The port on which to listen for incoming data
#define SOCKINQUEUE 5	//Max number of socks in queue
#define URLSIZE 256		//Max length of URL
#define HOSTSIZE 50		//For finding method of query
#define BUFLEN 63000 	//Max length of buffer -> experimentally 128K, seems legitimate, after some checking ; For google we gonna need ~twice as much 131072
#define STDIN 0
#define TIMESIZE 20
#define DEPTH 2
#define MAX_REQ 50

/// struct for webadresses
/** saves the text representation of the URL, the hostname and the link within the webpage. Also the text representation of the IP can be saved */
typedef struct webaddr {
	char hostname[HOSTSIZE];/**< hostname extracted from URL */
	char url[URLSIZE]; /**< URL */
	char IP[50];/**< IP of the webpage */
	char page[URLSIZE-HOSTSIZE];/**< Link of the webpage at the host */
	char filename [URLSIZE-HOSTSIZE];/**< Resource name */
} WebAddr;

/// struct for list
/** used to list character strings of new requests */
typedef struct simple_list{
	struct simple_list* next; /**< pointer to the next element of the list*/
	char * string; /**< character string*/
} list;

void die(char *s /**< [in] error message */, int i/**< [in] error number */);				//custom error function
int processSite(char *name/**< [in] name of the webpage */, FILE *log/**< [in,out] pointer to logfile */, int depth/**< [in] recursion depth*/,char * current_folder/**< [in] folder to write-in*/);
int processQuery (char *query /**< [in] string containing the query in html format */, FILE *log /**< [in,out] pointer to logfile */);
char* getTimeBuffer(char *timebuff);
int preprocess_website(WebAddr *website/**< [in,out] pointer to WebAddr struct */, FILE *log/**< [in,out] pointer to logfile */);
int requestWebsite (WebAddr *website /**< [in,out] pointer to WebAddr struct */, FILE *log /**< [in,out] pointer to logfile */, char *buffer /**< [in,out] pointer to received content */,unsigned int *buflen  /**< [out] pointer to received data size */);
int editAndSave (WebAddr *website /**< [in,out] pointer to WebAddr struct */, FILE *log /**< [in,out] pointer to logfile */, char *content /**< [in,out] pointer to formatted data */, list *new_requests /**< [out] pointer to list of new http queries */);


/// main function
/** calls the functions necessary for the function of the program, also all the steps for logging are implemented here. Select is used to efficiently wait for user input */
int main()
{
	//master set
	fd_set master_set; /*!< master fd_set for restoring the flag status */
	int ready;
	FD_ZERO(&master_set);
	FD_SET (STDIN, &master_set);

	//tmp_set
	fd_set tmp_set; /*!< temp fd_set for checking the flag status by select() */
	FD_ZERO(&tmp_set);

	//two more variables for work with stdin
	char stdbuff[URLSIZE] = {0};	//We don't expect user to enter query longer than expected URL sizes
	int lenbuf;

	///preparing pointers and variables for logging
	FILE *logfile; /*!< File pointer */
	char *logname;
	char *logdir;
	char timebuff[TIMESIZE];

	logname = (char*) malloc (sizeof(char)*32);
	///generate a filename
	strcpy(logname,"SESSION ");
	strcat(logname,getTimeBuffer(timebuff));
	strcat(logname,".txt");
	logdir = (char*) malloc (sizeof(logname)*strlen(logname)+sizeof(char)*7);
	strcpy(logdir, "./logs/");
	//if directory doesn't exist, create directory
	struct stat st = {0};
	if (stat(logdir, &st) == -1)
	{
		mkdir(logdir, 0700);
	}
	///concatenate path&filename
	strcat(logdir,logname);
	logfile=fopen(logdir,"w");
	if(logfile==NULL)
	{
		printf("Unable to create %s file.\n", logname);
		die("error creating log file", 5);
	}
	fprintf(logfile , "%s %s\n", getTimeBuffer(timebuff), "New session started");

	//choose initial folder to download the webiste in
	char * websiteDir = (char*) malloc (sizeof(char)*URLSIZE);
	strcpy(websiteDir,"./");



	printf("Welcome in DownloadWeb application.\nType \"help.\" for help instructions\n\n");

	while(1)
	{
		ready=0;
		fflush (stdout);

		/// restore temp_set
		tmp_set = master_set;
		/// register stdin for select
		ready=select(FD_SETSIZE, &tmp_set, NULL, NULL, NULL);
		if (ready == -1)
		{
			fprintf(logfile , "%s %s\n", getTimeBuffer(timebuff), "Error while selecting!");
			die("select()",1);

		}
		/// any number of file descriptors is ready to be read
		if (FD_ISSET(STDIN, &tmp_set) && ready)// listen has read access-> accept new connection
		{
			/// Read data from stdin using fgets.
			fgets(stdbuff, sizeof(stdbuff), stdin);

			/// Remove trailing newline character from the input buffer if needed.
			lenbuf = strlen(stdbuff) - 1;
			if (stdbuff[lenbuf] == '\n')
				stdbuff[lenbuf] = '\0';
			///starting application shutdown procedure
			if (!strcmp(stdbuff, "q."))
			{
				printf("Terminating...\n");
				fprintf(logfile , "%s %s %s\n", getTimeBuffer(timebuff), "User input:",stdbuff);
				fprintf(logfile , "%s %s %s\n", getTimeBuffer(timebuff), "System response:","Close application.");
				return EXIT_SUCCESS;
			}
			///processing URL query
			else if (!strncmp (stdbuff,"http://",7))//processing URL query
			{
				printf("Starting processing given website, please wait...\n%s\n", stdbuff);
				fprintf(logfile , "%s %s %s\n", getTimeBuffer(timebuff), "User input:",stdbuff);
				fprintf(logfile , "%s %s %s\n", getTimeBuffer(timebuff), "System response:","WWW website");
				if (processSite(stdbuff, logfile, DEPTH,websiteDir)==1)
				{
					printf("Unable to properly process website\n");
				}
				else
				{
					printf("Website got downloaded and is avalaible for offline use.\n");
				}
			}
			///processing google query
			else if (!strncmp (stdbuff,"GOOGLE: ",8))
			{
				printf("Started processing your google query, please wait...\n");
				fprintf(logfile , "%s %s %s\n", getTimeBuffer(timebuff), "User input:",stdbuff);
				fprintf(logfile , "%s %s %s\n", getTimeBuffer(timebuff), "System response:","GOOGLE QUERY");
				if (processQuery(stdbuff, logfile)==1)
				{
					printf("Unable to properly process website\n");
				}
				else
				{
					printf("Query got processed and is avalaible for offline use.\n");
				}
			}
			///Writes help manual in console
			else if (!strcmp(stdbuff, "help."))
			{
				fprintf(logfile , "%s %s %s\n", getTimeBuffer(timebuff), "User input:",stdbuff);
				fprintf(logfile , "%s %s %s\n", getTimeBuffer(timebuff), "System response:","Help request");
				printf("To download website one needs to write it's name in format:\n");
				printf("http://www.<<nameofwebsite>>.<<domain>>/<<perhaps_more_address>>\n");
				printf("And press ENTER\n(Be careful! Wrong format may cause application to not work properly!\n\n");
				printf("To download google query one needs to write it in format:\n");
				printf("GOOGLE: <<ALL_YOUR_QUERY_HERE>>\n");
				printf("And press ENTER\n(Be careful! Wrong format may cause application to not work properly!\n\n");
				printf("To leave, write \"q.\" and press ENTER\n\n");
			}
			else
			{
				fprintf(logfile , "%s %s %s\n", getTimeBuffer(timebuff), "User input:",stdbuff);
				fprintf(logfile , "%s %s %s\n", getTimeBuffer(timebuff), "System response:","Meaningless input");
				printf("meaningless input\n");
			}
		}
	}
	return EXIT_SUCCESS;
}

/// custom error function
/** outputs the error string s and exits program with exit number i
            \param s char string with error Message
            \param i error number
 */
void die (char *s, int i)
{
	perror(s);
	exit(i);
}

/// reads the time base, creates a timestamp and writes the result into char string timebuff
/** timestamp in Format YMD HMS
            \param timebuf pointer to string to write timestamp to
            \return pointer to string which contains the timestamp
 */
char* getTimeBuffer(char *timebuff)
{
	struct tm *sTm;
	time_t now = time (0);
	sTm = gmtime (&now);
	strftime (timebuff, TIMESIZE, "%Y-%m-%d %H:%M:%S", sTm);
	return timebuff;
}





/// converts the hostname to a IP address, prepares webpage address for processing in the program
/** extracts the hostname and the relative address on the server from the given URL, query the DNS to obtain the IP addresses linked to that webpage and save the result in a WebAddr struct,
          \param website WebAddr pointer which provides website address, the IP address will be added
            \param log pointer to logfile
            \return 0 on success and 1 on error
 */
int preprocess_website (WebAddr *website, FILE *log)
{
	char timebuff[TIMESIZE];
	/// check argument *website
	if ( !(website->hostname != NULL && website->url != NULL ))
	{
		fprintf(log, "%s %s\n", getTimeBuffer(timebuff), "invalid argument for partitionAddress()");
		return EXIT_FAILURE;
	}

	/// check buffer overflow
	if ( !(sizeof(website->hostname)) )
	{
		fprintf(log, "%s %s\n", getTimeBuffer(timebuff), "buffer overflow inside partitionAddress()");
		return EXIT_FAILURE;
	}

	/** start processing, search for the first / after the web address, this positio tells where the hostname ends and where the relative page address starts
	 */
	int i = strcspn (website->url+7,"/");

	if ((i+7)>48)
	{
		fprintf(log, "%s %s\n", getTimeBuffer(timebuff), "Hostname too long");
		return EXIT_FAILURE;
	}
	if ((i+7)>204)
	{
		fprintf(log, "%s %s\n", getTimeBuffer(timebuff), "page too long");
		return EXIT_FAILURE;
	}

	fprintf(log, "%s %s\n", getTimeBuffer(timebuff), "copying hostname and webpage.... successful");
	char *pch = strrchr (website->url,'/');
	strcpy(website->filename, pch+1);
	strncpy(website->hostname,website->url,i+7); //! strings not zero terminated!
	int l = pch-(website->url+i+7);
	strncpy(website->page,website->url+i+7,l+1);

	printf("\nname: %s, hostname:%s, page:%s\n", website->url, website->hostname, website->page);


	/// get the IP address for the website
	fprintf(log, "%s %s\n", getTimeBuffer(timebuff), "obtain IP address for website");
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_in *h;
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;


	//excluding http:// for getaddrinfo to work
	if ( (rv = getaddrinfo( website->hostname+7 , "http" , &hints , &servinfo)) != 0)
	{
		fprintf(log, "%s getaddrinfo error: %s\n", getTimeBuffer(timebuff), gai_strerror(rv));
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	/// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next)
	{
		h = (struct sockaddr_in *) p->ai_addr;
		strcpy(website->IP , inet_ntoa( h->sin_addr ) );
	}
	freeaddrinfo(servinfo); // all done with this structure

	fprintf(log, "%s %s\n", getTimeBuffer(timebuff), "success DNS query");
	return EXIT_SUCCESS;
}





/// uses google.de to search for the given string
/** the given string is the argument of a google search on the german google servers. The resulting webpage is downloaded.
            \param query input string for the google Query
            \param log pointer to logfile
            \return 0 on success*/
int processQuery(char *query, FILE *log)
{
	///logging  system if no global log file exists.
	FILE *logfile;
	char timebuff[TIMESIZE];

	if (log == NULL)
	{
		char *logname;
		char *logdir;
		logname = (char*) malloc (sizeof(char)*36);
		logdir = (char*) malloc (sizeof(logname)*strlen(logname)+sizeof(char)*7);
		strcpy (logname, "GOOGLEQUERY ");
		strcat(logname,getTimeBuffer(timebuff));
		strcat(logname,".txt");
		strcpy(logdir, "./logs/");
		strcat(logdir,logname);
		///if directory doesn't exist, create directory
		struct stat st = {0};
		if (stat(logdir, &st) == -1)
		{
			mkdir(logdir, 0700);
		}
		logfile=fopen(logdir,"w");
		if(logfile==NULL)
		{
			printf("Unable to create %s file.", logname);
			die("error creating log file", 5);
		}
	}
	else
	{
		logfile = log;
	}

	fprintf(logfile , "%s %s \"%s\"\n", getTimeBuffer(timebuff), "Started processing query", query);
	///processing query
	char *pch;
	char *tmp;
	query+=8;
	fprintf (logfile,"%s %s \"%s\" %s\n", getTimeBuffer(timebuff), "Splitting string", query, "into tokens.");

	tmp = (char*) malloc (sizeof(char)*(strlen(query)+30));
	strcpy (tmp, "http://www.google.de/search?q=");
	pch = strtok (query, " ,.-\"");
	while (1)
	{
		strcat(tmp,pch);
		fprintf (logfile,"%s %s \"%s\" %s\n", getTimeBuffer(timebuff), "Got token", pch, "from string.");
		pch = strtok (NULL, " ,.-\"");
		if(pch != NULL)
			strcat(tmp,"+");
		else
			break;

	}
	fprintf (logfile,"%s %s \"%s\"\n", getTimeBuffer(timebuff), "Finished processing query into string:", tmp);
	processSite(tmp, logfile, DEPTH,"./");
	query-=8;
	fprintf(logfile , "%s %s \"%s\"\n", getTimeBuffer(timebuff), "Finished processing query", query);
	return EXIT_SUCCESS;
}




/// download the webpage
/** assemble the http command for downloading the webpage given in WebAddr struct. Send the request to the server and download the webpage
            \param website website to be downloaded
            \param log pointer to logfile
            \param buffer char string for the website DATA
	\param buflen pointer to unsigned int returns size of received data
            \return 0 on success 1 on failure
 */

int requestWebsite (WebAddr *website, FILE *logfile, char *buffer, unsigned int *buflen)
{
	char timebuff[TIMESIZE];
	struct sockaddr_in si_server;
	socklen_t sock_length = sizeof(si_server);
	char data[BUFLEN];
	int sock_id;
	int rcv_length=-1;
	*buflen = 0;
	fprintf(logfile, "%s %s\n", getTimeBuffer(timebuff), "Initialisation of requestWebsite() inside of processSite()");
	char *request;
	request = (char*) malloc (sizeof(website->hostname)*strlen(website->hostname)+sizeof(website->page)*strlen(website->page)+sizeof(char)*75);
	///HTTP REQUEST
	strcpy(request, "GET ");
	strcat(request, website->page);
	strcat(request, website->filename);
	strcat(request, " HTTP/1.1\r\nHost: ");
	strcat(request, website->hostname+7);
	strcat(request, "\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux i686; rv:44.0) Gecko/20100101 Firefox/44.0\r\n");
	strcat(request, "\r\n");
	fprintf(logfile, "%s\n%s\n%s\n", getTimeBuffer(timebuff), "SENT REQUEST:", request);
	// Set the criteria
	memset((char*)&si_server, 0, sizeof(si_server)); // Set the structure to 0
	si_server.sin_family = AF_INET;
	si_server.sin_port = htons(PORT);

	sock_id = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_id == -1)
		fprintf(logfile, "%s %s\n", getTimeBuffer(timebuff), "Socket() inside requestWebsite() :: FAILURE");
	else
		fprintf(logfile, "%s %s\n", getTimeBuffer(timebuff), "Socket() inside requestWebsite() :: SUCCESS");

	if (inet_aton(website->IP, &si_server.sin_addr) == 0)
		fprintf(logfile, "%s %s\n", getTimeBuffer(timebuff), "inet_aton() inside requestWebsite() :: FAILURE");
	else
		fprintf(logfile, "%s %s\n", getTimeBuffer(timebuff), "inet_aton() inside requestWebsite() :: SUCCESS");

	fprintf(logfile, "%s %s\n", getTimeBuffer(timebuff), "Initialisation of connect() inside requestWebsite()");

	if (connect( sock_id, (const struct sockaddr *) &si_server, sock_length) == 0)
		fprintf(logfile, "%s %s\n", getTimeBuffer(timebuff), "connect() inside requestWebsite() :: SUCCESS");
	else
	{
		fprintf(logfile, "%s %s\n", getTimeBuffer(timebuff), "connect() inside requestWebsite() :: FAILURE");
		return EXIT_FAILURE;
	}
	if (send(sock_id, request, strlen(request), 0) < 0)
	{
		fprintf(logfile, "%s %s\n", getTimeBuffer(timebuff), "send() inside requestWebsite() :: FAILURE");
		return EXIT_FAILURE;
	}
	else
	{
		fprintf(logfile, "%s %s\n", getTimeBuffer(timebuff), "send() inside requestWebsite() :: SUCCESS");
	}

	while(1)
	{
		rcv_length = recv(sock_id, data, sizeof(data), 0);
		if (rcv_length > 0)
		{
			fprintf(logfile, "%s %s\n", getTimeBuffer(timebuff), "recv() inside requestWebsite() :: SUCCESS");
			if (*buflen <= BUFLEN-1)
			{
				fprintf(logfile, "%s %s\n", getTimeBuffer(timebuff), "var rcv_length inside requestWebsite() :: OK");
				memcpy(buffer+(*buflen), data,rcv_length);
				*buflen += rcv_length;
			}
			else
			{
				fprintf(logfile, "%s %s\n", getTimeBuffer(timebuff), "var rcv_length inside requestWebsite() :: DATA TOO LONG");
				fprintf(logfile, "%s %s %d\n", getTimeBuffer(timebuff), "var rcv_length inside requestWebsite() ::", rcv_length);
				close(sock_id);
				return EXIT_FAILURE;
			}

		}
		if(rcv_length==0)
		{
			fprintf(logfile, "%s %s %d\n", getTimeBuffer(timebuff), "recv() inside requestWebsite() :: SUCCESS, databytes read:",*buflen);
			break;
		}
		if(rcv_length == -1)
		{
			fprintf(logfile, "%s %s\n", getTimeBuffer(timebuff), "recv() inside requestWebsite() :: FAILURE");
			close(sock_id);
			return EXIT_FAILURE;
		}
	}
	char* beginning_of_data =strstr(buffer,"\r\n\r\n");
	int offset = beginning_of_data-buffer+4; //+4 fehlt
	if(beginning_of_data !=NULL)
	{
		(*buflen) = (*buflen)- offset;
		memmove(buffer,buffer +offset,*buflen);
		//buffer = beginning_of_data+4; //the escape character sequence has to be excluded

	}
	close(sock_id);
	return EXIT_SUCCESS;
}
/// edits contents of webpage and tries to retrieve links to other sites/resources to be downloaded
/** list with new requests is filled with request found on the site. max number of requests is MAX_REQ
            \param name anme for the logging file
            \param log pointer to logfile
            \param depth depth of the recursion
            \return 0 on success 1 on failure
 */
int editAndSave (WebAddr *website, FILE *logfile, char *content, list *new_requests)
{
	char timebuff[TIMESIZE];
	fprintf(logfile, "%s %s\n", getTimeBuffer(timebuff), "Initialisation of editAndSave() inside of processSite()");

	new_requests->string = (char*) malloc(URLSIZE);
	/// buffer beginning of list
	list * initial_pointer = new_requests;
	char * pch =content,*positionbuf;
	char *tmp;
	int i;
	for(i=0;i<MAX_REQ;)
	{
		positionbuf = strstr(pch,"src=\"");
		if(positionbuf==NULL)
			break; //no further occurrence found
		pch = strstr(positionbuf+5,"\"");
		tmp = (char*) malloc (pch-(positionbuf+5)+1);//length of src=" is 5, -1 to exclude "
		memset(tmp,0,pch-(positionbuf+5)+1);
		strncpy(tmp,positionbuf+5, pch-(positionbuf+5));

		///check if we have an absolute link, if so the path in the !saved! file has to be changed!
		if(strstr(tmp,"http://"))
			strcpy(new_requests->string, tmp);
		///relative link => add website address but the path in the !saved! file can stay the same (Browser will look in the same folder for this files)
		else
		{
			strcpy(new_requests->string, website->hostname);
			strcat(new_requests->string, website->page); // ist ein / nach der page address vorhanden? müsste!!
			strcat(new_requests->string, tmp);
		}
		new_requests->next =(list *) malloc(sizeof( list));
		new_requests = new_requests->next;
		new_requests->string = (char*) malloc(URLSIZE);
	}
	///reset position of search pointer
	pch=content;
	for(;i<MAX_REQ;)
	{
		positionbuf = strstr(pch,"href=\"");
		if(positionbuf==NULL)
			break; //no further occurrence found
		pch = strstr(positionbuf+6,"\"");
		tmp = (char*) malloc (pch-(positionbuf+6)+1);//length of src=" is 5, -1 to exclude "
		memset(tmp,0,pch-(positionbuf+5)+1);
		strncpy(tmp,positionbuf+6, pch-(positionbuf+6));

		///check if we have an absolute link, if so the path in the !saved! file has to be changed!
		if(strstr(tmp,"http://"))
			strcpy(new_requests->string, tmp);
		///relative link => add website address but the path in the !saved! file can stay the same (Browser will look in the same folder for this files)
		else
		{
			strcpy(new_requests->string, website->hostname);
			strcat(new_requests->string, website->page); // ist ein / nach der page address vorhanden? müsste!!
			strcat(new_requests->string, tmp);
		}
		new_requests->next =(list *) malloc(sizeof( list));
		new_requests = new_requests->next;
		new_requests->string = (char*) malloc(URLSIZE);
	}
	new_requests = initial_pointer;
	fprintf(logfile , "%s %s\n", getTimeBuffer(timebuff), "All links of the website have been saved");
	return EXIT_SUCCESS;
}





/// download contents of webpage recursively and start the logging process
/** a textfile under the name name is created for logging, a WebAddr struct is initialized and the webpage contents are downloaded recursively
            \param name anme for the logging file
            \param log pointer to logfile
            \param depth depth of the recursion
	    \param current_folder is pointer to char string containing directory for file save
            \return 0 on success 1 on failure
 */
int processSite(char *name, FILE *log, int depth, char *current_folder)
{
	char timebuff[TIMESIZE];
	if(depth < 1)
	{
		fprintf(log , "%s %s\n", getTimeBuffer(timebuff), "Maximum depth reached, website not processed");
		return EXIT_FAILURE;
	}
	fprintf(log , "%s %s\t%s, %s %d\n", getTimeBuffer(timebuff), "Started processing website", name, "DEPTH ::", depth);

	///processing website address
	WebAddr website;
	memset(website.hostname,0,HOSTSIZE-1);
	memset(website.IP,0,49);
	memset(website.page,0,URLSIZE-HOSTSIZE-1);
	memset(website.filename,0,URLSIZE-HOSTSIZE-1);
	memset(website.url,0,URLSIZE-1);
	strcpy(website.url, name);
	preprocess_website(&website, log);
	fprintf(log , "%s %s %s %s\n", getTimeBuffer(timebuff), website.url, "hostname is", website.hostname);
	fprintf(log , "%s %s %s %s\n", getTimeBuffer(timebuff), website.hostname, "resolved to", website.IP);
	fprintf(log , "%s %s %s\n", getTimeBuffer(timebuff), "Demanded page is", website.page);


	///download the website

	char content[BUFLEN];
	unsigned int received_data_size;
	memset(content, 0, BUFLEN-1);
	if(requestWebsite (&website, log, content,&received_data_size)==0)
		fprintf(log, "%s %s\n", getTimeBuffer(timebuff), "requestWebsite() inside processSite() :: SUCCESS");
	else
	{
		fprintf(log, "%s %s\n", getTimeBuffer(timebuff), "requestWebsite() inside processSite() :: FAILURE");
		return EXIT_FAILURE;
	}

	/// extract all the links from the website and change to relative addressing
	list *new_requests =malloc(sizeof(list));
	if(editAndSave (&website, log, content, new_requests)==0)
		fprintf(log, "%s %s\n", getTimeBuffer(timebuff), "editAndSave() inside processSite() :: SUCCESS");
	else
	{
		fprintf(log, "%s %s\n", getTimeBuffer(timebuff), "editAndSave() inside processSite() :: FAILURE");
		return EXIT_FAILURE;
	}

	/// create folders for all the websites and write content (ie the downloaded file from website) to folder
	int websvr_header_length =0;
	char *websiteDir = current_folder;
	///if it's html site, then create it another folder, else leave dir unchanged
	if (strstr(website.filename,".htm"))
	{
		websvr_header_length = strstr(content,"<!doctype HTML")-content;
		strcat(websiteDir, website.filename);
		strcat(websiteDir,"/");
		struct stat st = {0};
		if (stat(websiteDir, &st) == -1)
		{
			mkdir(websiteDir, 0700);
		}
	}
///	else
///	{
///		websvr_header_length = (strstr(content,"Path=/")+6)-content;
///	}
	FILE *file;
	char * filename = (char*) malloc(strlen(website.filename)+strlen(websiteDir)+50);

	strcpy(filename,websiteDir);
	///if main site, ie. no file name for saving
	if(strlen(website.filename)==0)
		/// filename cannot consist of "/" so http:// ommited
		strcat(filename, (website.hostname+7) );
	else
		strcat(filename, website.filename );
	///try to open file for saving data
	file=fopen(filename,"wb");
	if(file==NULL)
	{
		printf("Unable to create %s file.", filename);
		die("error creating  file", 5);
	}
	free(filename);
	///save the file
	fwrite (content+websvr_header_length , received_data_size-websvr_header_length, 1, file);
	fclose (file);
	//	   }


	/// recursively call processSite() for all the found links src = full addresses or relative ones
	for (; new_requests->next!=NULL ; new_requests = new_requests->next)
	{
		fprintf (log,"%s %s \"%s\" %s\n", getTimeBuffer(timebuff), "Got URI", new_requests->string, "from char array _new_requests");
		if(processSite(new_requests->string, log, depth-1,websiteDir))
			fprintf (log,"%s %s \"%s\"\n", getTimeBuffer(timebuff), "Couldn't proccess resource", new_requests->string);
	}
	fprintf (log,"%s %s\n", getTimeBuffer(timebuff), "No more URIs left.");
	fprintf(log , "%s %s %s\n", getTimeBuffer(timebuff), "Finished processing website", website.url);

	return EXIT_SUCCESS;

}






