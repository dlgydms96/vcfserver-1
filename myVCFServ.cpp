#include "myVCFServ.h"

#define SERVER_IPADDR "127.0.0.1"
#define SERVER_PORTNO 9000
#define MAX_CLIENTS 1
#define MAX_BUFFER 1024
#define MAX_NAME 20

struct ECAT ecat_var;
struct OBD2 obd2_var;

int connect_start = 1;
int nclients = 0;

pthread_t recv_thread_id[MAX_CLIENTS];
VThread *send_thread[MAX_CLIENTS] = {NULL};
VThread *init_thread = NULL;
VThread *ecat_thread = NULL;
VThread *obd2_thread = NULL;

//////////////////////////////////////////////
// socket connection data structure (mutex not implemented!)
///////////////////////////////////////////////
struct client_struct
{
  int id;
  int sockfd;
  sockaddr_in sockaddr;
} clients[MAX_CLIENTS];

void init_clients(void)
{
  for (int k = 0; k < MAX_CLIENTS; k++)
  {
    clients[k].sockfd = -1;
  }
}

int find_empty_client(void)
{
  for (int k = 0; k < MAX_CLIENTS; k++)
  {
    if (clients[k].sockfd < 0)
      return k;
  }
  return -1;
}

int find_client_by_id(int id)
{
  for (int k = 0; k < MAX_CLIENTS; k++)
  {
    if (clients[k].sockfd < 0)
      continue;
    if (clients[k].id == id)
      return k;
  }
  return -1;
}

void add_client(int id, int sockfd, sockaddr_in sockaddr)
{
  int idx = find_empty_client();
  if (idx < 0)
  {
    printf("error: no empty entries\n");
    exit(1);
  }

  clients[idx].id = id;
  clients[idx].sockfd = sockfd;
  clients[idx].sockaddr = sockaddr;
  printf("client[%d] added (id=%d)\n", idx, id);
}

void delete_client(int id)
{
  int idx = find_client_by_id(id);
  if (idx < 0)
  {
    printf("error: unknown id\n");
    exit(1);
  }
  close(clients[idx].sockfd);
  clients[idx].sockfd = -1;
}

////////////////////////////////
//shutdown code//////
////////////////////
void exit_handler(VThread *t)
{
  cout << t->get_thread_name() << " terminated" << endl;
}
void countdown(int sec)
{
  for (int i = 0; i < sec; i++)
  {
    sleep(1);
    cout << "countdown to shutdown: " << sec - i << endl;
  }
}

void shutdown()
{ // shutdown server gracefully
  int retval = 0;
  int idx = 0;
  while (idx < MAX_CLIENTS)
  {
    if (recv_thread_id[idx] != 0)
    {
      pthread_cancel(recv_thread_id[idx]);
      pthread_join(recv_thread_id[idx], (void **)&retval);
      if (retval == PTHREAD_CANCELED)
        cout << "recv_thread canceled" << endl;
      else
        cout << "recv_thread cancellation failed" << endl;
      recv_thread_id[idx] = 0;
    }
    ++idx;
  }
  idx = 0;
  /*  ecat_var[15].value = 0;
  ecat_var[14].value = 0;
  connect_start = 1;
  if(ecat_var[necat_var - 1].value != 3) {
    ecat_off();
    ecat_down();
  }
  if(obd2_var[nobd2_var - 1].value != 3) {
    obd_off();
    obd_down();
  }
  ecat_thread->ExitThread();
  obd2_thread->ExitThread();
  delete ecat_thread; ecat_thread = NULL;
  delete obd2_thread; obd2_thread = NULL;
 */
  while (idx < MAX_CLIENTS)
  {
    if (clients[idx].sockfd >= 0)
    {
      delete_client(idx);
      send_thread[idx]->ExitThread();
      delete send_thread[idx];
      send_thread[idx] = NULL;
    }
    ++idx;
  }

  countdown(3);
}

///////////////////////////////////////////////
// system programming
///////////////////////////////////////////////

void signal_handler(int signo)
{
  if ((signo = !SIGINT) || (signo = !SIGTERM))
  {
    cout << "unexpected signal = " << signo << " '" << strerror(signo) << "'" << endl;
    exit(0);
  }

  shutdown();
  exit(0);
}

int startup_server(char *ipaddr, int portno)
{
  struct sockaddr_in sockaddr;
  int sockfd;
  int val = 1;

  // create an unnamed socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  // set a socket option to reuse the server address
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) != 0)
  {
    printf("error: setsockopt(): %s\n", strerror(errno));
    return -1;
  }

  // name the socket with the server address
  sockaddr.sin_family = AF_INET;
  sockaddr.sin_addr.s_addr = inet_addr(ipaddr);
  sockaddr.sin_port = htons(portno);
  if (bind(sockfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) != 0)
  {
    printf("error: bind(): %s\n", strerror(errno));
    return -1;
  }

  // set the maximum number of pending connection requests
  if (listen(sockfd, 10) != 0)
  {
    printf("error: listen(): %s\n", strerror(errno));
    return -1;
  }

  return sockfd;
}
///////////////////////////////////////////////
// recv_thread routine for protobuf
///////////////////////////////////////////////

void readHdr(google::protobuf::uint32 hdr[], char *buf)
{
  google::protobuf::io::ArrayInputStream ais(buf, 2);
  CodedInputStream coded_input(&ais);
  coded_input.ReadVarint32(&hdr[0]);
  coded_input.ReadVarint32(&hdr[1]);
  cout << "\tHDR: type (in int32) " << hdr[0] << endl;
  cout << "\tHDR: content (in int32) " << hdr[1] << endl;
}

Initial_msg *initial_ReadBody(int sockfd, google::protobuf::uint32 *hdr)
{
  int bytecount;
  char buffer[hdr[1] + 2];
  Initial_msg *init_msg = new Initial_msg;
  //Read the entire buffer including the hdr
  printf("*****start inital function*******\n");
  if ((bytecount = recv(sockfd, (void *)buffer, hdr[1] + 2, MSG_WAITALL)) == -1)
  {
    fprintf(stderr, "Error receiving data %d\n", errno);
  }
  cout << "\tBODY: bytecount = " << bytecount << endl;
  for (int i = 0; i < bytecount; i++)
  {
    printf("_%d", buffer[i]);
  }
  printf("\n");
  //Assign ArrayInputStream with enough memory
  google::protobuf::io::ArrayInputStream ais(buffer, hdr[1] + 2);
  CodedInputStream coded_input(&ais);
  //Read an unsigned integer with Varint encoding, truncating to 32 bits.
  coded_input.Skip(2);
  //After the message's length is read, PushLimit() is used to prevent the CodedInputStream
  //from reading beyond that length.Limits are used when parsing length-delimited
  //embedded messages
  google::protobuf::io::CodedInputStream::Limit msgLimit = coded_input.PushLimit(hdr[1]);
  //De-Serialize
  init_msg->ParseFromCodedStream(&coded_input);
  //Once the embedded message has been parsed, PopLimit() is called to undo the limit
  coded_input.PopLimit(msgLimit);
  //Print the message
  cout << "\tMessage is " << init_msg->DebugString() << endl;
  return init_msg;
}

Operational_msg *operational_ReadBody(int sockfd, google::protobuf::uint32 *hdr)
{
  int bytecount;
  char buffer[hdr[1] + 2];
  Operational_msg *oper_msg = new Operational_msg;
  //Read the entire buffer including the hdr
  printf("******start operational function**********\n");
  printf("\tsockfd = %d\n", sockfd);
  if ((bytecount = recv(sockfd, (void *)buffer, hdr[1] + 2, MSG_WAITALL)) == -1)
  {
    fprintf(stderr, "Error receiving data %d\n", errno);
  }
  cout << "\tBODY: bytecount = " << bytecount << endl;
  for (int i = 0; i < bytecount; i++)
  {
    printf("_%d", buffer[i]);
  }
  printf("\n");
  //Assign ArrayInputStream with enough memory
  google::protobuf::io::ArrayInputStream ais(buffer, hdr[1] + 2);
  CodedInputStream coded_input(&ais);
  //Read an unsigned integer with Varint encoding, truncating to 32 bits.
  coded_input.Skip(2);
  //After the message's length is read, PushLimit() is used to prevent the CodedInputStream
  //from reading beyond that length.Limits are used when parsing length-delimited
  //embedded messages
  google::protobuf::io::CodedInputStream::Limit msgLimit = coded_input.PushLimit(hdr[1]);
  //De-Serialize
  oper_msg->ParseFromCodedStream(&coded_input);
  //Once the embedded message has been parsed, PopLimit() is called to undo the limit
  coded_input.PopLimit(msgLimit);
  //Print the message
  cout << "\tMessage is \n"
       << oper_msg->DebugString() << endl;
  return oper_msg;
}

void *recv_thread(void *arg)
{
  int client_idx = *(int *)arg;
  google::protobuf::uint32 hdr[2];
  char buffer[2];
  int bytecount = 0;

  printf("******recv thread created!*******\n");
  memset(buffer, '\0', 2);
  printf("\tin recv : client_idx = %d  clients[*client_idx].sockfd = %d\n", client_idx, clients[client_idx].sockfd);
  while (1)
  {
    if ((bytecount = recv(clients[client_idx].sockfd, buffer, 2, MSG_PEEK)) == -1)
    {
      fprintf(stderr, "Error receiving data %d\n", errno);
    }
    else if (bytecount == 0)
      break;
    printf("#############Message received##################\n\t");
    for (int i = 0; i < bytecount; i++)
    {
      printf("_%d", buffer[i]);
    }
    printf("\n");

    readHdr(hdr, buffer);

    if (hdr[0] == 1)
    {
      Operational_msg *umsg;
      printf("#############It's Operational_msg################\n\t");
      printf("\tin recv in if: client_idx = %d cliets[*client_idx].sockfd= %d \n", client_idx, clients[client_idx].sockfd);
      umsg = operational_ReadBody(clients[client_idx].sockfd, hdr);
      switch (umsg->_to())
      {
      case ECAT:
        ecat_thread->PostOperationalMsg((const Operational_msg *)umsg);
        break;
      case OBD2 :
        obd2_thread->PostOperationalMsg((const Operational_msg *) umsg);
        break;
      }

      umsg->set__result(2);
      printf("\tbefor PostOperationalMsg\n client_idx = %d\n", client_idx);
      //        send_thread[client_idx]->PostOperationalMsg((const Operational_msg *) umsg);
    }

    else if (hdr[0] == 0)
    {
      Initial_msg *umsg;
      printf("#############It's Initial_msg################\n\t");
      printf("in recv in if: client_idx = %d cliets[*client_idx].sockfd= %d \n", client_idx, clients[client_idx].sockfd);
      umsg = initial_ReadBody(clients[client_idx].sockfd, hdr);
      //          init_thread->PostInitialMsg((const Initial_msg *) umsg);

      umsg->set__result(2);
      //        send_thread[client_idx]->PostInitialMsg((const Initial_msg *) umsg);
    }

    else
    {
      printf("unknown type\n");
    }
    printf("recv end\n");
  }
}
/////////////////////////////////////////////////////
// about HybridAutomata
////////////////////////////////////////////////////

HybridAutomata *HA_ecatmgr;
class ECAT_UP2ON : public Condition
{
public:
  bool check(HybridAutomata *HA)
  {
    if ((ecat_var.ecat_state.value == ECAT_ON) && (HA->curState == ECAT_UP))
      return true;
    else
      return false;
  }
};
class ECAT_ON2OFF : public Condition
{
public:
  bool check(HybridAutomata *HA)
  {
    if ((ecat_var.ecat_state.value == ECAT_OFF) && (HA->curState == ECAT_ON))
      return true;
    else
      return false;
  }
};
class ECAT_OFF2DOWN : public Condition
{
public:
  bool check(HybridAutomata *HA)
  {
    if ((ecat_var.ecat_state.value == ECAT_DOWN) && (HA->curState == ECAT_OFF))
      return true;
    else
      return false;
  }
};
class ECAT_OFF2ON : public Condition
{
public:
  bool check(HybridAutomata *HA)
  {
    if ((ecat_var.ecat_state.value == ECAT_ON) && (HA->curState == ECAT_OFF))
      return true;
    else
      return false;
  }
};
class ECAT_ON2DOWN : public Condition
{
public:
  bool check(HybridAutomata *HA)
  {
    if ((ecat_var.ecat_state.value == ECAT_DOWN) && (HA->curState == ECAT_ON))
      return true;
    else
      return false;
  }
};
class ECAT_DOWN2UP : public Condition
{
public:
  bool check(HybridAutomata *HA)
  {
    if ((ecat_var.ecat_state.value == ECAT_UP) && (HA->curState == ECAT_DOWN))
      return true;
    else
      return false;
  }
};
void HA_EcatMgrThread()
{
  cout << "in HA_EcatMgrThread func()" << endl;
  ECAT_UP2ON *ECAT_up2on = new ECAT_UP2ON();
  ECAT_ON2OFF *ECAT_on2off = new ECAT_ON2OFF();
  ECAT_OFF2DOWN *ECAT_off2down = new ECAT_OFF2DOWN();
  ECAT_OFF2ON *ECAT_off2on = new ECAT_OFF2ON();
  ECAT_ON2DOWN *ECAT_on2down = new ECAT_ON2DOWN();
  ECAT_DOWN2UP *ECAT_down2up = new ECAT_DOWN2UP();

  HA_ecatmgr = new HybridAutomata(ECAT_START, ECAT_FINISH);
  HA_ecatmgr->setState(ECAT_UP, ecat_up);
  HA_ecatmgr->setState(ECAT_ON, ecat_on);
  HA_ecatmgr->setState(ECAT_OFF, ecat_off);
  HA_ecatmgr->setState(ECAT_DOWN, ecat_down);
  HA_ecatmgr->setCondition(ECAT_START, NULL, ECAT_UP);
  HA_ecatmgr->setCondition(ECAT_UP, ECAT_up2on, ECAT_ON);
  HA_ecatmgr->setCondition(ECAT_ON, ECAT_on2off, ECAT_OFF);
  HA_ecatmgr->setCondition(ECAT_OFF, ECAT_off2down, ECAT_DOWN);
  HA_ecatmgr->setCondition(ECAT_DOWN, NULL, ECAT_FINISH);
  HA_ecatmgr->setCondition(ECAT_DOWN, ECAT_down2up, ECAT_UP);
  HA_ecatmgr->setCondition(ECAT_OFF, ECAT_off2on, ECAT_ON);
  HA_ecatmgr->setCondition(ECAT_ON, ECAT_on2down, ECAT_DOWN);
}
class OBD2_UP2ON : public Condition
{
public:
  bool check(HybridAutomata *HA)
  {
    if ((obd2_var.obd2_state.value == OBD2_ON) && (HA->curState == OBD2_UP))
    {
      cout << "OBD2_UP2ON" << endl;
      return true;
    }

    else
      return false;
  }
};
class OBD2_ON2OFF : public Condition
{
public:
  bool check(HybridAutomata *HA)
  {
    if ((obd2_var.obd2_state.value == OBD2_OFF) && (HA->curState == OBD2_ON))
    {
      cout << "OBD2_ON2OFF" << endl;
      return true;
    }
    else
      return false;
  }
};
class OBD2_OFF2ON : public Condition
{
public:
  bool check(HybridAutomata *HA)
  {
    if ((obd2_var.obd2_state.value == OBD2_ON) && (HA->curState == OBD2_OFF))
    {
      cout << "OBD2_OFF2ON" << endl;
      return true;
    }
    else
      return false;
  }
};
class OBD2_OFF2DOWN : public Condition
{
public:
  bool check(HybridAutomata *HA)
  {
    if ((obd2_var.obd2_state.value == OBD2_DOWN) && (HA->curState == OBD2_OFF))
    {
      cout << "OBD2_OFF2DOWN" << endl;
      return true;
    }
    else
      return false;
  }
};
HybridAutomata *HA_obd2mgr;
void HA_Obd2MgrThread()
{
  cout << "in HA_Obd2MgrThread func()" << endl;
  OBD2_UP2ON *obd2_up2on = new OBD2_UP2ON();
  OBD2_ON2OFF *obd2_on2off = new OBD2_ON2OFF();
  OBD2_OFF2ON *obd2_off2on = new OBD2_OFF2ON();
  OBD2_OFF2DOWN *obd2_off2down = new OBD2_OFF2DOWN();

  HA_obd2mgr = new HybridAutomata(OBD2_START, OBD2_FINISH);
  HA_obd2mgr->setState(OBD2_UP, obd2_up);
  HA_obd2mgr->setState(OBD2_ON, obd2_on);
  HA_obd2mgr->setState(OBD2_OFF, obd2_off);
  HA_obd2mgr->setState(OBD2_DOWN, obd2_down);
  HA_obd2mgr->setCondition(OBD2_START,NULL,OBD2_UP);
  HA_obd2mgr->setCondition(OBD2_UP,  obd2_up2on ,  OBD2_ON);
  HA_obd2mgr->setCondition(OBD2_ON,  obd2_on2off,  OBD2_OFF);
  HA_obd2mgr->setCondition(OBD2_OFF, obd2_off2on,  OBD2_ON);
  HA_obd2mgr->setCondition(OBD2_OFF, obd2_off2down,OBD2_DOWN);
  //HA_obd2mgr->setCondition(,,);
}

/////////////////////////////////////////////////////
// 3 manager_threads
////////////////////////////////////////////////////

void ecat_handler(VThread *t, ThreadMsg *msg)
{
  cout << "in ecat_handler start!" << endl;
  const Operational_msg *oper_msg = static_cast<const Operational_msg *>(msg->msg);
  if ((oper_msg->_varid()) == (ecat_var.ecat_state.varID))
  {
    cout << "oper_msg->_value() = " << oper_msg->_value() << endl;
    ecat_var.ecat_state.value = oper_msg->_value();
    HA_ecatmgr->operate();
  }
  else if((oper_msg->_varid()) == (ecat_var.motor_state.varID))
  {
    ecat_var.motor_state.value = oper_msg->_value();
    cout<< "ecat_var.motor_state.value " << ecat_var.motor_state.value << endl;
  }
}

void obd2_handler(VThread *t, ThreadMsg *msg)
{
    cout << "in obd2_handler start!" << endl;
    const Operational_msg* oper_msg = static_cast<const Operational_msg*>(msg->msg);
    if((oper_msg->_varid()) == (obd2_var.obd2_state.varID))
    {
      cout << "oper_msg->_value() = " << oper_msg->_value() << endl;
      obd2_var.obd2_state.value = oper_msg->_value();
      HA_obd2mgr->operate();
    }
}


/////////////////////////////////////////////////////
// send_thread routine for protobuf
////////////////////////////////////////////////////

void send_handler(VThread *t, ThreadMsg *msg)
{
  cout << "****Proto Send****" << endl;
  int msg_type = msg->msgType;
  int siz;
  char *pkt;
  switch (msg_type)
  {
  case 1:
  {
    const Operational_msg *oper_msg = static_cast<const Operational_msg *>(msg->msg);
    cout << "\tsize after serilizing is " << oper_msg->ByteSize() << endl;
    siz = oper_msg->ByteSize();
    pkt = new char[siz + 2];
    google::protobuf::io::ArrayOutputStream aos(pkt, siz + 2);
    CodedOutputStream *coded_output = new CodedOutputStream(&aos);
    coded_output->WriteVarint32(msg_type);
    coded_output->WriteVarint32(siz);
    cout << "where????" << endl;
    oper_msg->SerializeToCodedStream(coded_output);
    cout << "where?" << endl;
    cout << "\tin send_handler -> oper Message is \n"
         << oper_msg->DebugString() << endl;
    break;
  }
  case 0:
  {
    const Initial_msg *init_msg = static_cast<const Initial_msg *>(msg->msg);
    cout << "\tsize after serilizing is " << init_msg->ByteSize() << endl;
    siz = init_msg->ByteSize();
    pkt = new char[siz + 2];
    google::protobuf::io::ArrayOutputStream aos(pkt, siz + 2);
    CodedOutputStream *coded_output = new CodedOutputStream(&aos);
    coded_output->WriteVarint32(msg_type);
    coded_output->WriteVarint32(siz);
    init_msg->SerializeToCodedStream(coded_output);
    cout << "in send_handler -> init Message is \n"
         << init_msg->DebugString() << endl;
    break;
  }
  }

  cout << "\tin send_handler -> \nBODY: bytecount = " << siz << endl;
  for (int i = 0; i < siz + 2; ++i)
  {
    printf("_%d", pkt[i]);
  }
  printf("\n");

  for (int i = 0; i < nclients; ++i)
  {
    if (write(clients[i].sockfd, pkt, siz + 2) < 0)
    {
      cout << t->get_thread_name() << ": " << strerror(errno) << endl;
      kill(getpid(), SIGINT);
    }
    cout << "\tafter write in send_handler\n"
         << endl;
  }
}

///////////////////////////////////////////////
// main function
///////////////////////////////////////////////
int main(int argc, char *argv[])
{
  sigset_t newmask, oldmask, waitmask;
  int c, flag_help = 0;
  char ipaddr[20] = SERVER_IPADDR;
  int portno = SERVER_PORTNO;
  int halfsd, fullsd; // socket descriptors
  char buffer[4];
  struct sockaddr_in sockaddr;
  int idx;

  memset(buffer, '\0', 4);
  while ((c = getopt(argc, argv, "hi:p:")) != -1)
  {
    switch (c)
    {
    case 'h':
      flag_help = 1;
      break;
    case 'i':
      memcpy(ipaddr, optarg, strlen(optarg));
      break;
    case 'c':
      portno = atoi(optarg);
      break;
    default:
      printf("unknown option : %c\n", optopt);
      break;
    }
  }

  if (flag_help == 1)
  {
    printf("usage: %s [-h] [-i ipaddr] [-p portno] \n", argv[0]);
    exit(1);
  }
  printf("server address = %s:%d\n", ipaddr, portno);

  if (signal(SIGINT, signal_handler) == SIG_ERR)
  {
    printf("error: signal(): %s\n", strerror(errno));
    exit(1);
  }
  if (signal(SIGTERM, signal_handler) == SIG_ERR)
  {
    printf("error: signal(): %s\n", strerror(errno));
    exit(1);
  }

  sigemptyset(&waitmask);
  sigaddset(&waitmask, SIGUSR1);
  sigemptyset(&newmask);
  sigaddset(&newmask, SIGINT);
  sigaddset(&newmask, SIGTERM);

  if (sigprocmask(SIG_BLOCK, &newmask, &oldmask) < 0)
  {
    printf("error: sigprocmask(): %s\n", strerror(errno));
    exit(1);
  }

  init_clients();
  if ((halfsd = startup_server(ipaddr, portno)) < 0)
  {
    shutdown();
    exit(1);
  }

  while (1)
  {
    int len = sizeof(sockaddr);
    fullsd = accept(halfsd, (struct sockaddr *)&sockaddr, (socklen_t *)&len);

    if (fullsd < 0)
    {
      printf("error : accept() : %s\n", strerror(errno));
      shutdown();
      break;
    }

    printf("Connected\n");
    if (nclients == MAX_CLIENTS)
    {
      printf("error: max clients reached\n");
      close(fullsd);
      sleep(60); //wait for a thread to exit for 1minute
      continue;
    }

    add_client(nclients, fullsd, sockaddr);
    idx = find_client_by_id(nclients);

    printf("fullsd = %d\n", fullsd);
    if (pthread_create(&recv_thread_id[nclients], NULL, &recv_thread, (void *)&idx) < 0)
    {
      printf("error: pthread_create(): %s\n", strerror(errno));
      shutdown();
      break;
    }

    send_thread[nclients] = new VThread("send_thread", send_handler, exit_handler);
    send_thread[nclients]->CreateThread();

    if (connect_start)
    {
      // create the following threads only once --> move the following code outside the while loop
      /*	init_thread = new VThread("init_thread", init_handler, exit_handler);
        init_thread->CreateThread();
      */
      HA_Obd2MgrThread();
      obd2_thread = new VThread("obd2_thread", obd2_handler, exit_handler);
    	obd2_thread->CreateThread();
   
      HA_EcatMgrThread();
      HA_EcatCyclicThread();
      ecat_thread = new VThread("ecat_thread", ecat_handler, exit_handler);
      ecat_thread->CreateThread();
      connect_start = 0;
    }
    ++nclients;
    if (find_empty_client() == -1)
    {
      sigsuspend(&waitmask);
    }
    // remove the following line to enable multiple recv_thread's
    //do {sigsuspend(&waitmask);} while(glob_sockfd >= 0);
  }

  if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0)
  {
    printf("error: sigprocmask(): %s\n", strerror(errno));
    exit(1);
  }

  return 0;
}
