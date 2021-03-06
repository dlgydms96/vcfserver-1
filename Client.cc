#include <unistd.h>
#include "umsg.pb.h"
#include <iostream>
#include <cerrno>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

using namespace google::protobuf::io;
using namespace vcf;
using namespace std;
void myFlush()
{
    while(getchar() != '\n');
}
void readHdr(google::protobuf::uint32 hdr[], char *buf)
{
    google::protobuf::io::ArrayInputStream ais(buf, 2);
    CodedInputStream coded_input(&ais);
    coded_input.ReadVarint32(&hdr[0]);
    coded_input.ReadVarint32(&hdr[1]);
    cout << "HDR: type (in int32) " << hdr[0] << endl;
    cout << "HDR: content (in int32) " << hdr[1] << endl;
}

Initial_msg *initial_ReadBody(int sockfd, google::protobuf::uint32 *hdr)
{
    int bytecount;
    char buffer[hdr[1] + 2];
    Initial_msg *init_msg = new Initial_msg;
    //Read the entire buffer including the hdr
    printf("start inital function\n");
    if ((bytecount = recv(sockfd, (void *)buffer, hdr[1] + 2, MSG_WAITALL)) == -1)
    {
        fprintf(stderr, "Error receiving data %d\n", errno);
    }
    cout << "BODY: bytecount = " << bytecount << endl;
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
    cout << "Message returned\n " << init_msg->DebugString() << endl;
    return init_msg;
}

Operational_msg *operational_ReadBody(int sockfd, google::protobuf::uint32 *hdr)
{
    int bytecount;
    char buffer[hdr[1] + 2];
    Operational_msg *oper_msg = new Operational_msg;
    //Read the entire buffer including the hdr
    printf("start operational function\n");
    printf("sockfd = %d\n", sockfd);
    if ((bytecount = recv(sockfd, (void *)buffer, hdr[1] + 2, MSG_WAITALL)) == -1)
    {
        fprintf(stderr, "Error receiving data %d\n", errno);
    }
    cout << "BODY: bytecount = " << bytecount << endl;
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
    cout << "Message returned \n"
         << oper_msg->DebugString() << endl;
    return oper_msg;
}
void recv(int hsock)
{
    google::protobuf::uint32 hdr[2];
    char buf[2];
    int bytecount;

    printf("\n\n recv start!\n");
    memset(buf, '\0', 2);

    if ((bytecount = recv(hsock, buf, 2, MSG_PEEK)) == -1)
    {
        fprintf(stderr, "Error receiving data %d\n", errno);
    }

    for (int i = 0; i < bytecount; i++)
    {
        printf("_%d", buf[i]);
    }
    printf("\n");

    readHdr(hdr, buf);

    if (hdr[0] == 1)
    {
        Operational_msg *umsg = new Operational_msg;
        umsg = operational_ReadBody(hsock, hdr);
        /*          switch(umsg->_to())
          {
              case ECAT:
                    ecat_thread->PostOperationalMsg((const Operational_msg *) umsg);
        	    break;
              case OBD2 : 
                    obd2_thread->PostOperationalMsg((const Operational_msg *) umsg);
                    break;
          }
*/
    }

    else if (hdr[0] == 0)
    {
        Initial_msg *umsg;
        umsg = initial_ReadBody(hsock, hdr);
        //          init_thread->PostInitialMsg((const Initial_msg *) umsg);
    }

    else
    {
        printf("unknown type\n");
    }
}
int main(int argv, char **argc)
{

    /* Coded output stram */

    vcf::Initial_msg request_msg;
    vcf::Initial_msg initial_cmd_msg;
    vcf::Initial_msg initial_prm_msg;
    vcf::Operational_msg oper_msg;

    request_msg.set__seqno(1);
    request_msg.set__version(0);
    request_msg.set__result(0);
    request_msg.set__msg("request_msg");
    request_msg.set__cycle(10);

    initial_cmd_msg.set__seqno(2);
    initial_cmd_msg.set__version(1);
    initial_cmd_msg.set__result(1);
    initial_cmd_msg.set__msg("initial_cmd_msg");
    vcf::Initial_msg::Cmd *cmd_value = initial_cmd_msg.add_cmd();
    cmd_value->set__from(0);
    cmd_value->set__name("current velocity");
    cmd_value->set__varid(10);
    cmd_value->set__value(30);

    initial_prm_msg.set__seqno(3);
    initial_prm_msg.set__version(1);
    initial_prm_msg.set__result(1);
    initial_prm_msg.set__msg("initial_prm_msg");
    vcf::Initial_msg::Cmd *prm_value1 = initial_prm_msg.add_cmd();
    prm_value1->set__from(0);
    prm_value1->set__name("target velocity");
    prm_value1->set__varid(11);
    prm_value1->set__value(0);
    vcf::Initial_msg::Prm *prm_value2 = initial_prm_msg.add_prm();
    prm_value2->set__vartype(1);
    prm_value2->set__min(0);
    prm_value2->set__max(100);

   

    int host_port = 9000;
    char *host_name = "127.0.0.1";

    struct sockaddr_in my_addr;
    char buffer[1024];
    int bytecount;
    int buffer_len = 0;
    int hsock;
    int *p_int;
    int err;
    
    hsock = socket(AF_INET, SOCK_STREAM, 0);
    if (hsock == -1)
    {

        printf("Error initializing socket %d\n", errno);
    }
    p_int = (int *)malloc(sizeof(int));
    *p_int = 1;
    if ((setsockopt(hsock, SOL_SOCKET, SO_REUSEADDR, (char *)p_int, sizeof(int)) == -1) ||
        (setsockopt(hsock, SOL_SOCKET, SO_KEEPALIVE, (char *)p_int, sizeof(int)) == -1))
    {
        printf("Error setting options %d\n", errno);
        free(p_int);
    }
    free(p_int);
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(host_port);
    memset(&(my_addr.sin_zero), 0, 8);
    my_addr.sin_addr.s_addr = inet_addr(host_name);
    if (connect(hsock, (struct sockaddr *)&my_addr, sizeof(my_addr)) == -1)
    {
        if ((err = errno) != EINPROGRESS)
        {

            fprintf(stderr, "Error connecting socket %d\n", errno);
        }
    }
    char ch;
    int siz;
    char *pkt;
    int num=0;
    while (1)
    {
        myFlush();
        ch = getchar();
        switch (ch)
        {
        case '1':
        {
            oper_msg.set__seqno(++num);
            oper_msg.set__to(1);
            oper_msg.set__varid(27);
            oper_msg.set__value(1);
            oper_msg.set__msg("ecatup");
            break;
        }
        case '2':
        {
            oper_msg.set__seqno(++num);
            oper_msg.set__to(1);
            oper_msg.set__varid(27);
            oper_msg.set__value(2);
            oper_msg.set__msg("ecaton");
            break;
        }
        case '3':
        {
            oper_msg.set__seqno(++num);
            oper_msg.set__to(1);
            oper_msg.set__varid(27);
            oper_msg.set__value(3);
            oper_msg.set__msg("ecatoff");
            break;
        }
        case '4':
        {
            oper_msg.set__seqno(++num);
            oper_msg.set__to(1);
            oper_msg.set__varid(27);
            oper_msg.set__value(4);
            oper_msg.set__msg("ecatdown");
            break;
        }
        case '5':
        {
            oper_msg.set__seqno(++num);
            oper_msg.set__to(2);
            oper_msg.set__varid(110);
            oper_msg.set__value(1);
            oper_msg.set__msg("obdup");
            break;
        }
        case '6':
        {
            oper_msg.set__seqno(++num);
            oper_msg.set__to(2);
            oper_msg.set__varid(110);
            oper_msg.set__value(2);
            oper_msg.set__msg("obdon");
            break;
        }
        case '7':
        {
            oper_msg.set__seqno(++num);
            oper_msg.set__to(2);
            oper_msg.set__varid(110);
            oper_msg.set__value(3);
            oper_msg.set__msg("obdoff");
            break;
        }
        case '8':
        {
            oper_msg.set__seqno(++num);
            oper_msg.set__to(2);
            oper_msg.set__varid(110);
            oper_msg.set__value(4);
            oper_msg.set__msg("obddown");
            break;
        }
        case 's':
        {
            oper_msg.set__seqno(++num);
            oper_msg.set__to(1);
            oper_msg.set__varid(28);
            oper_msg.set__value(5);
            oper_msg.set__msg("motor auto");
            break;
        }
        case 'r':
        {
            oper_msg.set__seqno(++num);
            oper_msg.set__to(1);
            oper_msg.set__varid(28);
            oper_msg.set__value(7);
            oper_msg.set__msg("motor reset");
            break;
        }
        case 'e':
        {
            oper_msg.set__seqno(++num);
            oper_msg.set__to(1);
            oper_msg.set__varid(28);
            oper_msg.set__value(9);
            oper_msg.set__msg("motor emergency_stop");
            break;
        }
        case 'p':
        {
            oper_msg.set__seqno(++num);
            oper_msg.set__to(1);
            oper_msg.set__varid(28);
            oper_msg.set__value(8);
            oper_msg.set__msg("motor regular_stop");
            break;
        }
        case 't':
        {
            char vel;
            int tvel;
            oper_msg.set__seqno(++num);
            oper_msg.set__to(1);
            oper_msg.set__varid(7);
            tvel = getchar();
            tvel = (int)vel;
            oper_msg.set__value(tvel);
            oper_msg.set__msg("target velocity");
            break;
        }
        default :
        continue;

        } //end of switch
        oper_msg.set__from(0);
        oper_msg.set__result(1);

        cout << "size after serilizing is " << oper_msg.ByteSize() << endl;
        siz = oper_msg.ByteSize();
        pkt = new char[siz + 2];
        google::protobuf::io::ArrayOutputStream aos(pkt, siz + 2);
        CodedOutputStream *coded_output = new CodedOutputStream(&aos);
        coded_output->WriteVarint32(1);
        coded_output->WriteVarint32(siz);
        oper_msg.SerializeToCodedStream(coded_output);
        if ((bytecount = send(hsock, (void *)pkt, siz + 2, 0)) == -1)
        {
            fprintf(stderr, "Error sending data %d\n", errno);
        }
        printf("Sent bytes %d\n", bytecount);
        for (int i = 0; i < bytecount; i++)
        {
            printf("_%d", pkt[i]);
        }
        delete pkt;
        usleep(1);
    }
FINISH:
    close(hsock);
}
