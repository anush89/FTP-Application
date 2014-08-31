#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netdb.h>
#include<errno.h>

#define HOSTNAME "net01.utdallas.edu"
#define PORT_UDP "3333" 
#define PORT_TCP "4444"
#define MAXBUFLEN 100

long long int size_of_file;

int main()
{
  size_of_file=0;
  printf("Client started. ");
  while(1)
  {
  printf("Please enter a command\n");
  char user_input[MAXBUFLEN];
  if(fgets(user_input,sizeof(user_input),stdin)!=NULL)
  {
    char *replace=strchr(user_input,'\n');
    if(replace!=NULL)
    {
      *replace='\0';
    }
  }

  if(strncmp(user_input,"lookup",6)==0)
  {
    //code for lookup function

    //socket creation for UDP 
    int sock_udp;
    struct addrinfo hints;
    struct addrinfo *serv_info;
    struct sockaddr_storage server_addr;
    socklen_t addr_len;

    memset(&hints,0,sizeof hints);
    hints.ai_family=AF_INET;
    hints.ai_socktype=SOCK_DGRAM;

    if(getaddrinfo(HOSTNAME,PORT_UDP,&hints,&serv_info)!=0)
    {
      printf("Error in getaddrinfo");
      exit(1);
    }

    sock_udp=socket(PF_INET,SOCK_DGRAM,0);
    if(sock_udp<0)
    {
      printf("Error creating sockets\n");
      exit(1);
    }

    //code for actual message transmission
    char *command;
    char *file_name;
    char buf[MAXBUFLEN];
    command=strtok(user_input," ");
    file_name=strtok(NULL," ");
    
    int numbytes;
    if((numbytes = sendto(sock_udp, file_name, strlen(file_name),0, serv_info->ai_addr, serv_info->ai_addrlen))==-1)
    {
      printf(strerror(errno));
      printf("Error sending UDP message");
      exit(1);
    }
    
    addr_len=sizeof server_addr;

    if((numbytes=recvfrom(sock_udp,buf,MAXBUFLEN,0,(struct sockaddr *)&server_addr,&addr_len))==-1)
    {
      printf("Error receiving UDP message");
      exit(1);
    }

    //process the msg
    buf[numbytes]='\0';

    if(strncmp(buf,"0",1)!=0)
    {
      size_of_file=strtol(buf,NULL,10);      
    }
    else
    {
      size_of_file=strtol("0",NULL,10);
    }
    
    printf("%lld\n",size_of_file);

    freeaddrinfo(serv_info);

    close(sock_udp);
  }//end of first if
  else if(strncmp(user_input,"download",8)==0)
  {
  char *user_command;
  char *fileName;
  char *no_of_chunks;
  int pieces;

  user_command=strtok(user_input," ");
  if((fileName=strtok(NULL," "))==NULL)
  {
    printf("download usage: download file_name no_of_chunks\n");
    continue;
  }
  if((no_of_chunks=strtok(NULL," "))==NULL)
  {    
    printf("download usage: download file_name no_of_chunks\n");
    continue;
  }
  pieces=strtol(no_of_chunks,NULL,10);

  if(size_of_file==0)
  {
    printf("Please lookup before downloading or the specified file does not exist\n");
    continue;
  }

  int size_of_chunk=size_of_file/pieces;

  //declare sockets
  int status,sock_tcp[MAXBUFLEN];
  struct addrinfo hints;
  struct addrinfo *serv_info;
  
  memset(&hints,0,sizeof hints);
  hints.ai_family=AF_INET;
  hints.ai_socktype=SOCK_STREAM;

  if(status=getaddrinfo(HOSTNAME,PORT_TCP,&hints,&serv_info)!=0)
  {
    perror("Error in getaddrinfo");
    exit(1);
  }

  int offset=0;
  char myOffset[MAXBUFLEN];
  char my_no_of_bytes[MAXBUFLEN];
  snprintf(my_no_of_bytes,MAXBUFLEN,"%d",size_of_chunk);

  int largest_sock;
  //fd_set temp_fds;
  fd_set orig_fds;

  //FD_ZERO(&temp_fds);
  FD_ZERO(&orig_fds);
  int i;
  
  for(i=0;i<pieces;i++)
  {
    if((sock_tcp[i]=socket(PF_INET,SOCK_STREAM,0))==-1)
    {
      printf("Error creating sockets for chunks\n");
      exit(1);
    }

    if(connect(sock_tcp[i],serv_info->ai_addr,serv_info->ai_addrlen)==-1)
    {
      printf(strerror(errno));      
      printf("\nError while connecting to server\n");
      exit(1);
    }
    
    int numBytes;
    char msg[MAXBUFLEN];
    
    strcpy(msg,fileName);
    strcat(msg," ");
    snprintf(myOffset,MAXBUFLEN,"%d",offset);
    strcat(msg,myOffset);
    offset=offset+size_of_chunk;
    strcat(msg," ");
    if(i==pieces-1)//last chunk
    {
      int temp=size_of_chunk+(size_of_file%pieces);
      snprintf(my_no_of_bytes,MAXBUFLEN,"%d",temp);
      strcat(msg,my_no_of_bytes);
    }
    else
    {
      strcat(msg,my_no_of_bytes);
    }
    printf("Chunk request %d is sent %s %s %s\n",(i+1),fileName,myOffset,my_no_of_bytes);
    if((numBytes=send(sock_tcp[i],msg,sizeof msg,0))==-1)
    {
      printf("msg error while sending\n");
      exit(1);
    }
    //for select function
    FD_SET(sock_tcp[i],&orig_fds);
    largest_sock=sock_tcp[i];
  }//end for

  offset=0;size_of_chunk=size_of_file/pieces;
  //receive the file
  char complete_file[size_of_file+1];
  memset(complete_file,0,(size_of_file+1));
  int flag=0;int my_flag=0;
  int count=0;
  
  int a[pieces];
  memset(a,0,pieces*sizeof(int));
  
  int p;
  for(p=0;p<pieces;p++)
    a[p]=1;

  offset=0;

  while(my_flag<pieces)
  {
    if((flag=select(largest_sock+1,&orig_fds,NULL,NULL,NULL))==-1)
    {
      printf("error in select()\n");
      exit(1);
    }

    int j;
    for(j=0;j<pieces;j++)
    {
      if(FD_ISSET(sock_tcp[j],&orig_fds))
      {
	FD_CLR(sock_tcp[j],&orig_fds);
	a[j]=0;
	my_flag+=1;
	int msgSize=0;
	int totalSizeRcvd=0;
	if(sock_tcp[j]==largest_sock)
	{
	  size_of_chunk=size_of_chunk+(size_of_file%pieces);
	}
	char temp[size_of_chunk];
        while(size_of_chunk>totalSizeRcvd)
	{
	  if((msgSize=recv(sock_tcp[j],temp+totalSizeRcvd,(size_of_chunk-totalSizeRcvd),0))==-1)
	  {
	    perror("TCP recv error\n");
	    exit(1);
	  }
	  totalSizeRcvd=totalSizeRcvd+msgSize;	  
        }//end while

        //write to complete_file[]
        int k;
        size_of_chunk=(size_of_file/pieces);
	offset=(size_of_chunk*j);
        for(k=0;k<totalSizeRcvd;k++)
        {
           complete_file[offset+k]=temp[k];
        }
	totalSizeRcvd=0;

      }//end if FD_ISSET
      else
      {
        if(a[j]!=0)
          FD_SET(sock_tcp[j],&orig_fds);
      }
    }//end for

  }//end outer while
  strcat(complete_file,"\0");

  //write to file
  FILE *my_fp;
  char tempFileName[MAXBUFLEN];
  memset(tempFileName,0,MAXBUFLEN);
  strcpy(tempFileName,fileName);
  size_t data_size;
  if(my_fp=fopen(tempFileName,"w+"))
  {
    data_size=fwrite(complete_file,1,size_of_file,my_fp);
    if(data_size!=size_of_file)
    {
      printf("fwrite writing less\n");
      exit(1);
    }
    fclose(my_fp);
    printf("File downloaded successfully\n");
  }
  else
  {
    printf("%s\n",strerror(errno));
    printf("error in file\n");
  }
  //need to freeaddrinfo in a for loop
  //need to close sockets after use
  for(i=0;i<pieces;i++)
  {
    close(sock_tcp[i]);
  }
  freeaddrinfo(serv_info);
  }//end second if
  else if(strncmp(user_input,"quit",4)==0)
  {
    printf("Client Exiting... Bye\n");
    break;    
  }
  else
  {
    printf("unknown command. Try again\n");
  }
  
  }//end while loop
  return 0;
}
