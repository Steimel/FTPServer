/*
  Tommy Steimel
  my_ftpd.cpp
*/

#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <dirent.h>
#include <fcntl.h>
#include <cstring>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

using namespace std;

int getSocketFD(int p, bool toBind, string address);
int getSocketFD(char* s, bool toBind, string address);
string Trim(string in);

int main(int argc, char** argv)
{
  //Check for valid command-line input
  if(argc != 2)
    {
      cout << "Error: wrong number of arguments." << endl;
      return -1;
    }

  int sock = getSocketFD(argv[1], true, "");
  if(sock == -1)
    {
      // failed
      return -1;
    }

  int dataFD = -1;

  // Accept a peer
  while(true)
    {
      struct sockaddr_storage their_addr;
      socklen_t addr_size = sizeof their_addr;

      int newFD = accept(sock,(struct sockaddr*)&their_addr, &addr_size);

      bool quit = false;
      bool ImageMode = false;
      
      // close old data file desc if it exists
      if(dataFD > 0)
	{
	  close(dataFD);
	}
      dataFD = -1;

      string toSend;

      toSend = "220 Service ready for new user.\n";
      if(send(newFD, toSend.c_str(), toSend.length(), 0) == -1)
	{
	  cout << "Error sending." << endl;
	  close(newFD);
	  continue;
	}
      
      while(!quit)
	{	  
	  char buf[4096];
	  int r = recv(newFD, buf, 4096, 0);

	  if(r == 0)
	    {
	      // Connection has been closed
	      quit = true;
	      close(newFD);
	      cout << "User closed connection." << endl;
	      continue;
	    }
	  else if(r == -1)
	    {
	      //Error
	      cout << "Error: recv()" << endl;
	      close(newFD);
	      quit = true;
	      continue;
	    }
	  
	  string msg = "";
	  for(int x = 0; x < r; x++)
	    {
	      msg.push_back(buf[x]);
	    }

	  cout << "Received: " << msg << endl;

	  if(msg.substr(0,4) == "QUIT")
	    {
	      // Quit
	      toSend = "221 Service closing control connection.\n";
	      send(newFD, toSend.c_str(), toSend.size(), 0);
	      quit = true;
	      close(newFD);
	      cout << "User quit." << endl;
	    }
	  else if(msg.substr(0,4) == "USER")
	    {
	      // Accept all users because I'm secure like that
	      cout << "Logged in." << endl;
	      toSend = "230 User logged in, proceed.\n";
	      send(newFD, toSend.c_str(), toSend.size(), 0);
	    }
	  else if(msg.substr(0,6) == "TYPE I")
	    {
	      // Change type to Image
	      ImageMode = true;
	      toSend = "200 Command okay.\n";
	      send(newFD, toSend.c_str(), toSend.size(), 0);
	    }
	  else if(msg.substr(0,4) == "STOR")
	    {
	      // First, make sure you're in image mode
	      if(!ImageMode)
		{
		  // Not yet in image mode
		  cout << "Attempted to STOR before entering image mode." << endl;
		  toSend = "451 Requested action aborted: local error in processing\n";
		  send(newFD, toSend.c_str(), toSend.size(), 0);
		  continue;
		}
	      
	      // Now, make sure PORT has been called
	      if(dataFD < 0)
		{
		  // Port hasn't been set
		  cout << "Attempted to STOR before porting." << endl;
		  toSend = "451 Requested action aborted: local error in processing\n";
		  send(newFD, toSend.c_str(), toSend.size(), 0);
		  continue;
		}

	      // Create file
	      mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	      
	      int destFile = creat(Trim(msg.substr(5)).c_str(), mode);
	      if(destFile < 0)
		{
		  // File creation failed
		  cout << "Error creating file: " << msg.substr(5) << endl;
		  toSend = "450 Requested file aciton not taken.\n";
		  send(newFD, toSend.c_str(), toSend.size(), 0);
		  continue;
		}

	      // Tell the client that reading is about to start
	      toSend = "125 Data connection already open; transfer starting.\n";
	      send(newFD, toSend.c_str(), toSend.size(), 0);
	      
	      // Read and write
	      while((r = recv(dataFD, buf, 4096, 0)) > 0)
		{
		  // write to file
		  write(destFile, buf, r);
		  // clear buffer
		  buf[0] = '\0';
		}
	      close(destFile);

	      // done
	      toSend = "250 Requested file action okay, completed.\n";
	      send(newFD, toSend.c_str(), toSend.size(), 0);
	      
	    }
	  else if(msg.substr(0,4) == "RETR")
	    {
	      // Retrieve
	      // First, make sure you're in image mode
	      if(!ImageMode)
		{
		  // Not yet in image mode
		  cout << "Attempted to RETR before entering image mode." << endl;
		  toSend = "451 Requested action aborted: local error in processing\n";
		  send(newFD, toSend.c_str(), toSend.size(), 0);
		  continue;
		}
	      
	      // Now, make sure PORT has been called
	      if(dataFD < 0)
		{
		  // Port hasn't been set
		  cout << "Attempted to RETR before porting." << endl;
		  toSend = "451 Requested action aborted: local error in processing\n";
		  send(newFD, toSend.c_str(), toSend.size(), 0);
		  continue;
		}
	      
	      // get file
	      string fileName = msg.substr(5);
	      // Take out those spaces
	      fileName = Trim(fileName);
	      int sourceFile = open(fileName.c_str(), O_RDONLY);
	      if(sourceFile < 0)
		{
		  // File not found
		  cout << "Error opening source file: |" << msg.substr(5) << "|" <<  endl;
		  toSend = "550 Requested action not taken.\n";
		  send(newFD, toSend.c_str(), toSend.size(), 0);
		  continue;
		}
	      
	      //tell the client that data transfer is about to happen
	      toSend = "125 Data connection already open; transfer starting.\n";
	      send(newFD, toSend.c_str(), toSend.size(), 0);
	      
	      // Read and write
	      while((r = read(sourceFile, buf, 4096)) > 0)
		{
		  // write to data port
		  send(dataFD, buf, r, 0);
		  // clear buffer
		  buf[0] = '\0';
		}
	      close(sourceFile);
	      close(dataFD);
	      dataFD = -1;

	      // done
	      toSend = "250 Requested file action okay, completed.\n";
	      send(newFD, toSend.c_str(), toSend.size(), 0);
	      
	    }
	  else if(msg.substr(0,4) == "PORT")
	    {
	      // open data port
	      int c2 = msg.rfind(",");
	      int c1 = msg.rfind(",",c2-1);
	      
	      // Calculate port number
	      int portNumber = atoi(msg.substr(c1+1,c2-c1).c_str()) * 256;
	      portNumber += atoi(msg.substr(c2+1).c_str());
	      
	      // Get address
	      string address = msg.substr(5,c1-5);
	      for(int i = 0; i < address.size(); i++)
		{
		  if(address.substr(i,1) == ",")
		    {
		      address.replace(i,1,".");
		    }
		}

	      // if data port already open, close it
	      if(dataFD > 0)
		{
		  close(dataFD);
		}
	      
	      // Get the file descriptor
	      cout << "Attempting to open port: " << portNumber << endl;
	      cout << "From address: " << address << endl;
	      dataFD = getSocketFD(portNumber, false, address);
	      
	      if(dataFD < 0)
		{
		  // Port open failure
		  cout << "Port open failed." << endl;
		  toSend = "425 Can't open data connection.\n";
		  send(newFD, toSend.c_str(), toSend.size(), 0);
		  continue;
		}
	      
	      // Port open success
	      cout << "Port opened successfully!" << endl;
	      toSend = "200 Command okay.\n";
	      send(newFD, toSend.c_str(), toSend.size(), 0);
	    }
	  else if(msg.substr(0,6) == "STRU R")
	    {
	      // Don't allow for a switch to Record
	      cout << "Attempted to change to RECORD." << endl;
	      toSend = "504 Command not implemented for that parameter\n";
	      send(newFD, toSend.c_str(), toSend.size(), 0);
	    }
	  else if(msg.substr(0,4) == "MODE")
	    {
	      // Don't allow modes other than stream
	      cout << "Attempted to change modes." << endl;
	      toSend = "504 Command not implemented for that parameter\n";
	      send(newFD, toSend.c_str(), toSend.size(), 0);
	    }
	  else if(msg.substr(0,4) == "LIST")
	    {
	      // First, make sure you're in image mode
	      if(!ImageMode)
		{
		  // Not yet in image mode
		  cout << "Attempted to LIST before entering image mode." << endl;
		  toSend = "451 Requested action aborted: local error in processing\n";
		  send(newFD, toSend.c_str(), toSend.size(), 0);
		  continue;
		}
	      
	      // Now, make sure PORT has been called
	      if(dataFD < 0)
		{
		  // Port hasn't been set
		  cout << "Attempted to LIST before porting." << endl;
		  toSend = "451 Requested action aborted: local error in processing\n";
		  send(newFD, toSend.c_str(), toSend.size(), 0);
		  continue;
		}

	      // Open directory
	      DIR* dir;
	      string files = "";
	      if(Trim(msg) == "LIST")
		{
		  cout << "LIST no param" << endl;
		  dir = opendir(".");
		}
	      else
		{
		  cout << "LIST with param" << endl;
		  dir = opendir(Trim(msg.substr(5)).c_str());
		}
	      
	      if(dir == NULL)
		{
		  //Failed to open directory
		  cout << "Failed to open directory" << endl;
		  toSend = "550 Requested action not taken.\n";
		  send(newFD, toSend.c_str(), toSend.size(), 0);
		  continue;
		}

	      struct dirent* entry;
	      while((entry = readdir(dir)) != NULL)
		{
		  files = files + string(entry->d_name) + "\n";
		}
	      closedir(dir);
	      
	      //tell the client that data transfer is about to happen
	      toSend = "125 Data connection already open; transfer starting.\n";
	      send(newFD, toSend.c_str(), toSend.size(), 0);
	      
	      // send
	      send(dataFD, files.c_str(), files.length(), 0);
	      close(dataFD);
	      dataFD = -1;

	      // done
	      toSend = "250 Requested file action okay, completed.\n";
	      send(newFD, toSend.c_str(), toSend.size(), 0);
	      
	    }
	  else if(msg.substr(0,4) == "NOOP")
	    {
	      // No operation
	      cout << "No operation." << endl;
	      toSend = "200 Command okay.\n";
	      send(newFD, toSend.c_str(), toSend.size(), 0);
	    }
	  else
	    {
	      // Unimplemented command
	      toSend = "502 Command not implemented.\n";
	      send(newFD, toSend.c_str(), toSend.size(), 0);
	    }
	}
    }
}







// Gets the fd for the socket. Returns -1 on failure
// toBind - whether you bind or connect
// address - address to connect to (not used for bind)
int getSocketFD(char* s, bool toBind, string address)
{
  int status;
  struct addrinfo hints;
  struct addrinfo *servinfo;
  struct addrinfo *bound;
  int sock;
  int yes = 1;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  //Get address info
  if(toBind)
    {
      if((status = getaddrinfo(NULL, s, &hints, &servinfo)) != 0)
	{
	  cout << "Error: getaddrinfo failed." << endl;
	  return -1;
	}
    }
  else
    {
      if((status = getaddrinfo(address.c_str(), s, &hints, &servinfo)) != 0)
	{
	  cout << "Error: getaddrinfo failed." << endl;
	  return -1;
	}
    }

  //Try to bind/connect

  for(bound = servinfo; bound != NULL; bound = bound->ai_next)
    {
      sock = socket(bound->ai_family, bound->ai_socktype, bound->ai_protocol);
      if(sock == -1) continue;
      
      if(setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes, sizeof(int)) == -1)
	{
	  cout << "Error: setsockopt failed." << endl;
	  close(sock);
	  return -1;
	}
      if(toBind)
	{
	  // try to bind
	  if(bind(sock,bound->ai_addr,bound->ai_addrlen) == -1)
	    {
	      close(sock);
	      continue;
	    }
	}
      else
	{
	  //try to connect
	  if(connect(sock,bound->ai_addr,bound->ai_addrlen) == -1)
	    {
	      close(sock);
	      continue;
	    }
	}
      
      // If you get this far, you've successfully bound
      break;
    }

  //If you get here with a NULL bound, then you were unsuccessful
  if(bound == NULL)
    {
      cout << "Error: failed to bind to anything." << endl;
      close(sock);
      return -1;
    }

  //Free up the space since the addrinfo is no longer needed
  freeaddrinfo(servinfo);

  //Try to listen if you're binding
  if(toBind)
    {
      if(listen(sock, 3) == -1)
	{
	  cout << "Error: failed to listen." << endl;
	  close(sock);
	  return -1;
	}
    }

  // return the fd
  return sock;
}

int getSocketFD(int p, bool toBind, string address)
{
  char port[6];
  sprintf(port,"%d",p);
  port[6] = '\0';
  return getSocketFD(port, toBind, address);
}

// Used to trim spaces and line feeds
string Trim(string in)
{
  bool done = false;
  for(int i = in.size()-1; i >= 0 && !done; i--)
    {
      if(in.substr(i) == " " || in.substr(i) == "\r\n" || in.substr(i) == "\n" || in.substr(i) == "\r")
	{
	  in = in.substr(0,i);
	}
      else
	{
	  done = true;
	}
    }
  return in;
}
