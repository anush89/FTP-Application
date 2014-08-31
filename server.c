#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<string.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<pthread.h>
#include<sys/stat.h>
#include<memory.h>
#include<pthread.h>
#include<errno.h>
#include<signal.h>

#define MAXBUFLEN 100 //Defining some upper limit

static int read_till;
pthread_mutex_t lock;

//UDP lookup function
void *listen_UDP(void *UDP_port_no)
{
  //dont forget to remove UDP code from the main
  
  //addrinfo
  struct addrinfo hints, *server_addr;
  int rv;

  char udp_hostname[128];
  int udp_status;
  
  udp_status=gethostname(udp_hostname,sizeof(udp_hostname));
  
  if(udp_status<0)
  {
    printf("hostname error");
    exit(1);
  }

  memset(&hints,0,sizeof hints);
  hints.ai_family=AF_INET;
  hints.ai_socktype=SOCK_DGRAM;
  if((rv=getaddrinfo(udp_hostname,(char *)UDP_port_no,&hints,&server_addr))!=0)
  {
    printf("Error in UDP getaddrinfo\n");
    exit(1);
  }
  struct sockaddr_in *my_ptr=(struct sockaddr_in *)server_addr->ai_addr;
  char ipstr[INET_ADDRSTRLEN];
  inet_ntop(server_addr->ai_family,&(my_ptr->sin_addr),ipstr, sizeof ipstr);

  int UDP_s;
  UDP_s=socket(server_addr->ai_family,server_addr->ai_socktype,0);
  if(UDP_s<0)
  {
    printf("Error creating UDP socket\n");
    exit(1);
  }

  // add bind
  if(bind(UDP_s, server_addr->ai_addr, server_addr->ai_addrlen)==-1)
  {
    printf(strerror(errno));
    printf("Error while binding UDP\n");
    exit(1);
  }

  int UDP_flag=1;
  while(UDP_flag)
  {
  
  //recvfrom client file_name

  int num_bytes;
  char buf[MAXBUFLEN];
  struct sockaddr_storage client_addr;
  socklen_t addr_len;
  addr_len=sizeof client_addr;
  memset(buf,0,MAXBUFLEN);
  if((num_bytes=recvfrom(UDP_s,buf, MAXBUFLEN-1,0,(struct sockaddr *)&client_addr,&addr_len))<0)
  {
    printf(strerror(errno));
    printf("UDP recvfrom error\n");
    exit(1);
  }


  //listen for UDP
  num_bytes=0;

  //check for file
  FILE *fp;
  char file_size[MAXBUFLEN];
  strcpy(file_size,"\0");
  char temp[MAXBUFLEN]="./data/";

  if(fp=fopen(strcat(temp,buf),"r"))
  {
    struct stat st;
    stat(temp,&st);
    snprintf(file_size,sizeof file_size, "%ld", st.st_size);

    printf("UDP look-up query received for the file \"%s\".Returning query response %ld\n",buf,st.st_size);

    if((num_bytes=sendto(UDP_s,file_size,sizeof file_size, 0,(struct sockaddr *)&client_addr,addr_len))<0)
    {
      printf("UDP send to error\n");
      exit(1);
    }
    fclose(fp);
  }
  else
  { 
    printf("UDP look-up query received for the file \"%s\".Returning query response %d\n",buf,0);
    if((num_bytes=sendto(UDP_s,"0",sizeof "0", 0,(struct sockaddr *)&client_addr,addr_len))<0)
    {
      printf("UDP send to error\n");
      exit(1);
    }
  }
  }//end while loop
  
  freeaddrinfo(server_addr);
  close(UDP_s);
  return(0);
}

//----------------------------------------------------------------------------------------------------------------------

//tcp worker thread function
void *tcp_worker_thread(void *ptr)
{
  pthread_mutex_lock(&lock);
  intptr_t my_tid=(intptr_t)ptr;
  int bytes_recv;
  char data_read[MAXBUFLEN];
  bytes_recv=recv(my_tid,data_read,sizeof data_read,0);
  if(bytes_recv<0)
  {
    printf("Error reading data from client\n");
    exit(1);
  }
  
  char *f_name;
  char *offset;
  char *num_of_bytes;
  int off,bytes_to_read;
  f_name=strtok(data_read," ");
  offset=strtok(NULL," ");
  num_of_bytes=strtok(NULL," ");
  off=strtol(offset,NULL,10);
  bytes_to_read=strtol(num_of_bytes,NULL,10);

  printf("chunk request of %d bytes in \"%s\" at offset %d\n",bytes_to_read,f_name,off);

  //send all requested data.

  FILE *fp2;
  char *data_read_file;
  size_t result;
  char temp2[MAXBUFLEN];
  strcpy(temp2,"./data/");
  strcat(temp2,f_name);
  if(fp2=fopen(temp2,"r"))
  {
    data_read_file=(char *)malloc(sizeof(char)*(bytes_to_read+1));
    fseek(fp2,off,SEEK_SET);
    result=fread(data_read_file,1,bytes_to_read,fp2);
    if(result!=bytes_to_read)
    {  
      printf("file not completely read\n");
      exit(1);
    }
    
    fclose(fp2);

    //send read data
    int total=0;
    int bytesleft=bytes_to_read;
    int n;
    
    char size_buf[MAXBUFLEN];
    int bytes_sent;

    while(total<bytes_to_read)
    {
      n=send(my_tid,data_read_file+total,bytesleft,0);
      if(n==-1)
      {
        printf("Error while sending data using sockets\n");
	exit(1);
      }
      total=total+n;
      bytesleft=bytesleft-n;
    }
    
    //free memory and close file
    free(data_read_file);
  }
  else
  {
    printf("Error, file not found\n");
  }
 
  //close socket here
  close(my_tid);
  pthread_mutex_unlock(&lock);
  return NULL;
}

//-----------------------------------------------------------------------------------------------------------------------
void cleanExit()
{
  exit(0);
}
//------------------------------------------------------------------------------------------------------------------------

int main(int argc,char *argv[])
{
  unsigned int TCP_port;
  if(argc!=3)
  {
    printf("Usage: server TCP_port_no UDP_port_no\n");
    exit(1);
  } 
  else
  {
    if(sscanf(argv[2],"%u",&TCP_port)==0)
    {
      printf("Not a valid argument for TCP port num input");
      exit(1);
    }
  }

  //spawn a thread for listening UDP
  pthread_t UDP_thread;
  pthread_attr_t attr2;
  pthread_attr_init(&attr2);
  pthread_attr_setdetachstate(&attr2,PTHREAD_CREATE_DETACHED);
  if(pthread_create(&UDP_thread,&attr2, listen_UDP,argv[1]))
  {
    printf("Error creating UDP thread\n");
    exit(1);
  }
  pthread_attr_destroy(&attr2);
  
  char my_hostname[128];
  int hn_status;
  
  hn_status=gethostname(my_hostname,sizeof(my_hostname));
  
  if(hn_status<0)
  {
    printf("hostname error");
    exit(1);
  }

  struct hostent *hent;
  char str[INET_ADDRSTRLEN];
  
  hent=gethostbyname(my_hostname);
  
  if(hent==NULL)
  {
    printf("Error while getting IP address\n");
    exit(1);
  }

  printf("Server (%s) is running on UDP port %s TCP port %s\n",inet_ntop(AF_INET,(struct in_addr*)hent->h_addr,str,INET_ADDRSTRLEN),argv[1],argv[2]);

  //Declare sockets
  int TCP_s;
  TCP_s=socket(PF_INET,SOCK_STREAM,0);
  if(TCP_s<0)
  {
    printf("Error creating socket main()\n");
    exit(1);
  }
  
  //Binding sockets
  struct sockaddr_in my_addr;
  struct sockaddr_in my_addr_UDP;
  int bind_status;  
  my_addr.sin_family=AF_INET;
  my_addr.sin_port=htons(TCP_port);
  my_addr.sin_addr.s_addr=INADDR_ANY;
  memset(my_addr.sin_zero,'\0',sizeof(my_addr.sin_zero));
  bind_status=bind(TCP_s,(struct sockaddr *)&my_addr,sizeof(my_addr));

  if(bind_status<0)
  {
    perror("Error while binding:");
    exit(1);
  }

  //listen for connection
  int listen_status;
  listen_status=listen(TCP_s,10);
  if(listen_status==-1)
  {
    printf("Error listening for TCP connections\n");
    exit(1);
  }  
  
  struct sockaddr_storage client_addr_tcp;
  socklen_t sin_size;
  sin_size=sizeof client_addr_tcp;
  //main accept()
  int i=0; 
  pthread_t thread[1000];
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
  while(i<1000)
  {
    intptr_t new_s;
    new_s=accept(TCP_s,(struct sockaddr *)&client_addr_tcp, &sin_size);
    if(new_s==-1)
    {
      printf("Error in accept\n");
      exit(1);
    }
    if(i==0)
      printf("TCP connection received\n");

    signal(SIGTERM,cleanExit);
    signal(SIGINT,cleanExit);

    if(pthread_create(&(thread[i]),&attr,tcp_worker_thread,(void *)new_s))
    {
      printf("Error creating tcp worker threads\n");
      exit(1);
    }
    i++;
  }
  printf("Server limit reached... exiting\n");
  return 0;
}
