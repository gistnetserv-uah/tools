#include <string.h>
#include <omnetpp.h>
#include <algorithm>
#include <vector>
#include <math.h>
#include <cstdio>
#include <cstdlib>
#include "AFrame_m.h"
using namespace omnetpp;

//Para crear los logs, op1 -> EV muy sucio, op2 -> std :: fopen();
//global var.

/*-----------------------------------------mod_1---------------------------------------------------*/
FILE* logs = std::fopen("logs.txt", "w+");
long long int AMACs_Counter = 0 ;
char aux_str[20];
simtime_t conv;
long long int No_AMACNodes = 0 ;
int num_nodes = 0, aux_nodes = 0;
/*-----------------------------------------/mod_1---------------------------------------------------*/

class node : public cSimpleModule
{
  private:
    int numberOfPorts;
    int numberOfLearnedAMACs=0; //Keep track of AMACs learned so far
    bool isARoot;
    int N=0; //Max number of AMACs - When a switch reaches this number (if set to nonzero), it starts discarding new frames and does not learn anymore.
    int L=0; //L: Preffix variation length - learn the N (if set to nonzero) first that vary in the prefix of length L
    int P=0; //Learn up to P (when set to >0) AMACs per port irrespective of the value of N and L
    static const int depth=16;
    static const int breadth=8;
    int portnumToBreadthRatio=0;
    struct AMAC
    {
        int level;
        int octets[depth];
    };
    /*struct AMACList
    {
        std::vector<AMAC> amacList;
    };*/
    std::vector<AMAC>** portAMACListArray;//Store list of AMACs for each port
  protected:
    // The following redefined virtual function holds the algorithm.
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    virtual void broadcastAFrameAsRoot(int aRoot, int arrivalPort);
    virtual void broadcastAFrame(AMAC aMAC, int arrivalPort, bool addAMAC=true);
    virtual void processAFrame(int arrivalPort, AFrame* aFrame);
    virtual bool dismantleAMAC(AMAC aMAC, int arrivalPort);
    virtual void dismantleAndBroadcastAllAMAC(int failedPort);
    virtual bool isLoopFreeAMAC(AMAC aMAC);
    virtual int getNextHopUpstream(AMAC aMAC);
    virtual int getNextHopDownstream(AMAC aMAC, int arrivalPort);
    virtual int getFirstAMAC(AFrame* aFrame);
    virtual void printAMAC(AMAC aMAC);
    virtual void printAllAMAC();
    virtual void printAMAC(AFrame* aFrame);
};
// The module class needs to be registered with OMNeT++
Define_Module(node);
void node::initialize()
{
    isARoot=par("isARoot");
    N=par("N");
    L=par("L");
    P=par("P");
    numberOfPorts=gateSize("port$o");
    portAMACListArray=new std::vector<AMAC>*[numberOfPorts];
    portnumToBreadthRatio=floor(numberOfPorts/breadth);
    if (portnumToBreadthRatio==0)
        portnumToBreadthRatio=1;
    EV<<"Initialize: num ports "<<numberOfPorts<<"\n";
    std::cout<<"Initializing... switch: "<<getName()<<" number of ports: "<<numberOfPorts<<std::endl;

    /*-----------------------------------------mod_2---------------------------------------------------*/
    SimTime();
   // std::fprintf(logs,"Initializing... switch: %s\tnumber of ports: %d \n",getName(),numberOfPorts);
    /*-----------------------------------------mod_2---------------------------------------------------*/
    for(int i=0;i<numberOfPorts;i++)
    {
        portAMACListArray[i]=nullptr;
    }
    
    num_nodes++;
    /*if(strcmp(getName(),"s9")==0)//test S2C message; will be moved from here
    {            
        AFrame* aFrame1=new AFrame("S2C");
        aFrame1->setLevel(5);
        aFrame1->setAMAC(0,0);
        aFrame1->setAMAC(1,1);
        aFrame1->setAMAC(2,1);
        aFrame1->setAMAC(3,1);
        aFrame1->setAMAC(4,2);
        scheduleAt(10, aFrame1);
    }*/
}
void node::handleMessage(cMessage *msg)
{    
    std::cout<<"Message arrived. Node:  "<<getName()<<" Message Type: "<<msg->getName()<<std::endl;
    /*-----------------------------------------mod_3---------------------------------------------------*/

    //std::fprintf(logs,"--- Message arrived. Node: %s\t Message Type: %s ---\n",getName(),msg->getName());
    /*-----------------------------------------/mod_3---------------------------------------------------*/


    if(strcmp(msg->getName(),"CoI")==0)//Directly connected to an SDN Controller
    {
        int arrivalPort=msg->getArrivalGate()->getIndex();
        AFrame* aFrame=(AFrame*)msg;
        isARoot=true;
        int receivedAMAC=aFrame->getAMAC(0);
        AMAC aMAC;
        aMAC.level=1;
        aMAC.octets[0]=receivedAMAC;
        strcpy(aux_str, getName());
        std::cout<<"AMAC is "<<receivedAMAC<<" from SDN Controller for port "<<arrivalPort<<"\n";

        /*-----------------------------------------mod_4---------------------------------------------------*/
        //std::fprintf(logs,"AMAC is %d  from SDN Controller for port %d \n",receivedAMAC,arrivalPort);
        /*-----------------------------------------/mod_4---------------------------------------------------*/

        if(portAMACListArray[arrivalPort]==nullptr)
        {
            portAMACListArray[arrivalPort]=new std::vector<AMAC>();
        }
        portAMACListArray[arrivalPort]->push_back(aMAC);
        broadcastAFrameAsRoot(receivedAMAC, arrivalPort);
        delete msg;
    }
    else if(strcmp(msg->getName(),"AMAC")==0)
    {
        AMACs_Counter++;
        int arrivalPort=msg->getArrivalGate()->getIndex();
        AFrame* aFrame=(AFrame*)msg;
        std::cout<<"AMAC received for port "<<arrivalPort<<"\n";
        /*-----------------------------------------mod_5---------------------------------------------------*/
        //std::fprintf(logs,"AMAC received for port %d\n",arrivalPort);
        /*-----------------------------------------/mod_5---------------------------------------------------*/
       // printAMAC(aFrame);
        processAFrame(arrivalPort, aFrame);
    }
    else if(strcmp(msg->getName(),"S2C")==0)
    {     
        AFrame* aFrame=(AFrame*)msg;
        int receivedLevel=aFrame->getLevel();
        int nextHop;
        if(receivedLevel==-1)//From pktGen; have to populate aFrame
        {
            //aFrame=new AFrame();
            //delete msg;
            std::cout<<"From pktGen"<<std::endl;
            nextHop=getFirstAMAC(aFrame);
        }
        else
        {
            AMAC aMAC1;
            aMAC1.level=receivedLevel;
            for(int i=0;i<receivedLevel;i++)//Copy octets
            {
                aMAC1.octets[i]=aFrame->getAMAC(i);
            }
            nextHop = getNextHopUpstream(aMAC1);
        }
        //std::cout<<"S2C message arrived at "<<getName()<<": sending AFrame with AMAC ";
        //printAMAC(aMAC1);
        //std::cout<<" to port "<<nextHop<<std::endl;
        if(nextHop>=0)
            send(aFrame, "port$o", nextHop);
    }
    else if(strcmp(msg->getName(),"C2S")==0)
    {     
        int arrivalPort=msg->getArrivalGate()->getIndex();
        AFrame* aFrame=(AFrame*)msg;
        AMAC aMAC1;
        int receivedLevel=aFrame->getLevel();
        aMAC1.level=receivedLevel;
        for(int i=0;i<receivedLevel;i++)//Copy octets
        {
            aMAC1.octets[i]=aFrame->getAMAC(i);
        }
        int nextHop = getNextHopDownstream(aMAC1, arrivalPort);
        //std::cout<<"S2C message arrived at "<<getName()<<": sending AFrame with AMAC ";
        //printAMAC(aMAC1);
        //std::cout<<" to port "<<nextHop<<std::endl;
        if(nextHop==arrivalPort)
            std::cout<<"I am the destination"<<std::endl;
        else
            send(aFrame, "port$o", nextHop);
    }
    else if(strcmp(msg->getName(),"LinkFailure")==0)
    {
        int arrivalPort=msg->getArrivalGate()->getIndex();
        dismantleAndBroadcastAllAMAC(arrivalPort);
        delete msg;
    }
}

int node::getFirstAMAC(AFrame* aFrame)//Returns upstream hop and populates aMAC
{
    int nextHopUpstream=-1;
    std::cout<<"getFirstAMAC.";

    std::cout<<std::endl;
    for(int i=0;i<numberOfPorts;i++)//check AMAClist of each port to see if received AMAC was originated from this upstream port
    {
        if(portAMACListArray[i]!=nullptr)
        {
            nextHopUpstream=i;
            std::vector<AMAC> *portAMACList=portAMACListArray[i];
            std::cout<<"port "<<i<<" AMAC List size"<<portAMACList->size()<<std::endl;
            for(int j=0;j<portAMACList->size();j++)//check all AMAC associated with this port
            {
                AMAC portAMAC=portAMACList->at(j);
                aFrame->setLevel(portAMAC.level);
                for(int k=0;k<portAMAC.level;k++)//Copy octets
                {
                    aFrame->setAMAC(k,portAMAC.octets[k]);
                }
                std::cout<<"Selected AMAC ";
                //printAMAC(portAMAC);
                std::cout<<std::endl;
                break;
            }
            break;
        }
    }
    std::cout<<"Next hop upstream "<<nextHopUpstream<<std::endl;
    return nextHopUpstream;
}

bool node::isLoopFreeAMAC(AMAC aMAC)
{
    bool amacIsLoopFree=true;
    for(int i=0;i<numberOfPorts;i++)//check AMAClist of each port to see if received AMAC was originated from this switch
    {
        if(portAMACListArray[i]!=nullptr)
        {
            std::vector<AMAC> *portAMACList=portAMACListArray[i];
            for(int j=0;j<portAMACList->size();j++)//check all AMAC associated with this port
            {
                AMAC portAMAC=portAMACList->at(j);
                if(portAMAC.level<=aMAC.level)
                {
                    bool prefixMatch=true;
                    int maxSearchLevel=(L==0?portAMAC.level:L);
                    for(int k=0;k<maxSearchLevel;k++)
                    {
                        if(portAMAC.octets[k]!=aMAC.octets[k])
                        {
                            prefixMatch=false;
                            break;
                        }
                    }
                    if(prefixMatch)
                    {
                        amacIsLoopFree=false;
                        std::cout<<"AMAC ";
                        //printAMAC(aMAC);
                        std::cout<<"not loop free. Originated from ";
                        //printAMAC(portAMAC);
                        std::cout<<std::endl;
                    }
                }
            }
        }
    }
    return amacIsLoopFree;
}

void node::printAMAC(AMAC aMAC)
{
    std::cout<<"(AMAC) Level "<<aMAC.level<<": ";
    for(int i=0;i<aMAC.level;i++)
    {
        std::cout<<aMAC.octets[i]<<".";
    }
    /*-----------------------------------------mod_6---------------------------------------------------*/
   //std::fprintf(logs,"(AMAC) Level %d: ",aMAC.level);
    std::fprintf(logs,"\t ");
   for(int i=0;i<aMAC.level;i++)
    {
        std::fprintf(logs,"%d.",aMAC.octets[i]);
    }
    std::fprintf(logs,"\n");
    /*-----------------------------------------/mod_6---------------------------------------------------*/

    //std::cout<<std::endl;
}

void node::printAMAC(AFrame *aFrame)
{
    std::cout<<"(AFRAME) Level "<<aFrame->getLevel()<<": ";
    for(int i=0;i<aFrame->getLevel();i++)
    {
        std::cout<<aFrame->getAMAC(i)<<".";
    }
    /*-----------------------------------------mod_7---------------------------------------------------*/
    //std::fprintf(logs,"(AFRAME) Level %d:",aFrame->getLevel());
    for(int i=0;i<aFrame->getLevel();i++)
    {
       std::fprintf(logs,"%d.",aFrame->getAMAC(i));
    }
    std::fprintf(logs,"\n");
     /*-----------------------------------------/mod_7---------------------------------------------------*/
    //std::cout<<std::endl;
}

void node::printAllAMAC()
{
    ////
    int z = 0 ;
    for(int i=0;i<numberOfPorts;i++)//iterate through all ports
            {
                if(portAMACListArray[i]==nullptr)
                {z++;}
            }

    if(z == numberOfPorts){

        std::fprintf(logs,"\tempty\n");
        No_AMACNodes++;

    }else{

    ///
        for(int i=0;i<numberOfPorts;i++)//iterate through all ports
        {
            if(portAMACListArray[i]!=nullptr)
            {
                /*-----------------------------------------mod_8---------------------------------------------------*/
                std::fprintf(logs,"\n\n\t-Port %d\n",i);
                /*-----------------------------------------/mod_8---------------------------------------------------*/

                std::cout<<"\tPort "<<i<<std::endl;
                std::vector<AMAC> *portAMACList=portAMACListArray[i];
                for(int j=0;j<portAMACList->size();j++)//check all AMAC associated with this port
                {
                    std::cout<<"\t\tAMAC "<<j<<": ";
                    /*-----------------------------------------mod_9---------------------------------------------------*/
                    //std::fprintf(logs,"\t\tAMAC %d:",j);
                    /*-----------------------------------------/mod_9---------------------------------------------------*/
                    AMAC portAMAC=portAMACList->at(j);
                    printAMAC(portAMAC);
                    std::cout<<std::endl;
                }
            }
        }

    }
}

int node::getNextHopUpstream(AMAC aMAC)
{
    int nextHopUpstream=-1;
    std::cout<<"getNextHopUpstream. Got AMAC ";
    printAMAC(aMAC);
    std::cout<<std::endl;
    for(int i=0;i<numberOfPorts;i++)//check AMAClist of each port to see if received AMAC was originated from this upstream port
    {
        if(portAMACListArray[i]!=nullptr)
        {
            std::vector<AMAC> *portAMACList=portAMACListArray[i];
            for(int j=0;j<portAMACList->size();j++)//check all AMAC associated with this port
            {
                AMAC portAMAC=portAMACList->at(j);
                //std::cout<<"Matching with port AMAC ";
                //printAMAC(portAMAC);
                //std::cout<<std::endl;
                if(portAMAC.level<=aMAC.level)//both level will be equal only in case of self message, otherwise port level will be less than message level
                {
                    bool prefixMatch=true;                    
                    for(int k=0;k<portAMAC.level;k++)
                    {
                        if(portAMAC.octets[k]!=aMAC.octets[k])
                        {
                            prefixMatch=false;
                            break;
                        }
                    }
                    if(prefixMatch)
                    {
                        nextHopUpstream=i;
                        std::cout<<"Next upstream hop for AMAC ";
                        //printAMAC(aMAC);
                        std::cout<<"is "<<nextHopUpstream<<std::endl;                        
                    }
                }
            }
        }
    }
    return nextHopUpstream;
}

int node::getNextHopDownstream(AMAC aMAC, int arrivalPort)
{
    int nextHopDownstream=-1;
    std::cout<<"getNextHopDownstream. Got AMAC ";
    printAMAC(aMAC);
    std::cout<<std::endl;
    for(int i=arrivalPort;i<=arrivalPort;i++)//check only at arrival port
    {
        if(portAMACListArray[i]!=nullptr)
        {
            std::vector<AMAC> *portAMACList=portAMACListArray[i];
            for(int j=0;j<portAMACList->size();j++)//check all AMAC associated with this port
            {
                AMAC portAMAC=portAMACList->at(j);
                //std::cout<<"Matching with port AMAC ";
                //printAMAC(portAMAC);
                //std::cout<<std::endl;
                if(portAMAC.level<=aMAC.level)//both level will be equal and AMAC will match only at destination switch
                {
                    bool prefixMatch=true;                    
                    for(int k=0;k<portAMAC.level;k++)
                    {
                        if(portAMAC.octets[k]!=aMAC.octets[k])
                        {
                            prefixMatch=false;
                            break;
                        }
                    }
                    if(prefixMatch)
                    {
                        if(portAMAC.level==aMAC.level)
                            nextHopDownstream=arrivalPort;//This switch is the destination
                        else
                            nextHopDownstream=aMAC.octets[portAMAC.level];
                        std::cout<<"Next downstream hop for AMAC ";
                        //printAMAC(aMAC);
                        std::cout<<"is "<<nextHopDownstream<<std::endl;                        
                    }
                }
            }
        }
    }
    return nextHopDownstream;
}

bool node::dismantleAMAC(AMAC aMAC, int arrivalPort)
{
    bool foundAndDismantled=false;
    std::cout<<"Dismantle AMAC. Got AMAC ";
    printAMAC(aMAC);
    std::cout<<std::endl;
    for(int i=arrivalPort;i<=arrivalPort;i++)//check only at arrival port
    {
        if(portAMACListArray[i]!=nullptr)
        {
            std::vector<AMAC> *portAMACList=portAMACListArray[i];
            for(int j=0;j<portAMACList->size();j++)//check all AMAC associated with this port
            {
                AMAC portAMAC=portAMACList->at(j);
                //std::cout<<"Matching with port AMAC ";
                //printAMAC(portAMAC);
                //std::cout<<std::endl;
                if(portAMAC.level==aMAC.level)//both levels must be equal for the AMACs to be equal
                {
                    bool amacMatch=true;
                    for(int k=0;k<portAMAC.level;k++)
                    {
                        if(portAMAC.octets[k]!=aMAC.octets[k])
                        {
                            amacMatch=false;
                            break;//stop comparing octets
                        }
                    }
                    if(amacMatch)
                    {
                        portAMACList->erase(portAMACList->begin()+j);
                        if(portAMACList->size()==0)
                        {
                            delete portAMACList;
                            portAMACListArray[i]=nullptr;
                        }
                        std::cout<<"Matched and Dismantled."<<std::endl;
                        foundAndDismantled=true;
                        break;//stop going through AMAC list
                    }
                }
            }
        }
    }
    return foundAndDismantled;
}

void node::dismantleAndBroadcastAllAMAC(int failedPort)
{
    std::cout<<"Dismantle and broadcast AMAC for port ";
    std::cout<<failedPort<<std::endl;
    for(int i=failedPort;i<=failedPort;i++)//check only at arrival port
    {
        if(portAMACListArray[i]!=nullptr)
        {
            std::vector<AMAC> *portAMACList=portAMACListArray[i];
            for(int j=0;j<portAMACList->size();j++)//check all AMAC associated with this port
            {
                AMAC portAMAC=portAMACList->at(j);
                broadcastAFrame(portAMAC, failedPort, false);//Instruct dismantle towards leaf nodes
            }
            delete portAMACList;
            portAMACListArray[i]=nullptr;
        }
    }
}

void node::processAFrame(int arrivalPort, AFrame* aFrame)
{
	AMAC aMAC;
	int receivedLevel=aFrame->getLevel();
	aMAC.level=receivedLevel;
	for(int i=0;i<receivedLevel;i++)//Copy octets
	{
		aMAC.octets[i]=aFrame->getAMAC(i);
	}
	bool amacIsLoopFree=isLoopFreeAMAC(aMAC);//Check if AMAC is loop free
	if(aFrame->getAddAMAC()==false)//Dismantle AMAC
	{
	    if(dismantleAMAC(aMAC, arrivalPort))
	        broadcastAFrame(aMAC, arrivalPort, false);//Broadcast AMAC dismantle message
	}
	else if(amacIsLoopFree)//Add AMAC if loop free
	{
	    if(portAMACListArray[arrivalPort]==nullptr)
        {
            portAMACListArray[arrivalPort]=new std::vector<AMAC>();
        }

	    int numberOfAMACsLearnedForThisPort=portAMACListArray[arrivalPort]->size();

        if(numberOfAMACsLearnedForThisPort<P || (P==0&&(N<=0 || numberOfLearnedAMACs<N)))//Skip if we have already learned N AMACs
        {
            portAMACListArray[arrivalPort]->push_back(aMAC);//add AMAC to port
            numberOfLearnedAMACs++;
            if(aMAC.level<depth)
                broadcastAFrame(aMAC, arrivalPort);//Send out to all ports except the receiving port
        }
        else
        {
            std::cout<<"Not learning due to max number of AMAC ("<<N<<") already learned."<<std::endl;
        }
	}
	else
	{
	    std::cout<<"Discarding loop AMAC"<<std::endl;
	}
	delete aFrame;
}

void node::broadcastAFrameAsRoot(int aRoot, int arrivalPort)
{
    int loopMaxLimit=numberOfPorts<breadth?numberOfPorts:breadth;
    for(int i=0;i<loopMaxLimit;i++)
    {
        int targetPort=i*portnumToBreadthRatio;
        if(targetPort!=arrivalPort)
        {
            AFrame* aFrame=new AFrame("AMAC");
            aFrame->setLevel(2);
            aFrame->setAMAC(0,aRoot);
            aFrame->setAMAC(1,targetPort);
            send(aFrame, "port$o", targetPort);
        }
    }
}

void node::broadcastAFrame(AMAC aMAC, int arrivalPort, bool addAMAC)
{
    //std::cout<<"Number of ports: "<<numberOfPorts<<std::endl;
    int loopMaxLimit=numberOfPorts<breadth?numberOfPorts:breadth;
    for(int i=0; i<loopMaxLimit;i++)
    {
        int targetPort=i*portnumToBreadthRatio;
        if(targetPort!=arrivalPort)
        {
            AFrame* aFrame=new AFrame("AMAC");
            aFrame->setLevel(aMAC.level+1);//Increment level by 1
            aFrame->setAddAMAC(addAMAC);
            for(int j=0;j<aMAC.level;j++)
            {
                aFrame->setAMAC(j,aMAC.octets[j]);//Copy received AMAC
            }
            aFrame->setAMAC(aMAC.level,targetPort);//Append this port identifier to received AMAC before sending out
            send(aFrame, "port$o", targetPort);
        }
    }
}

void node::finish()
{
    /*------------*/
    char aux_command[200];
    double aux;
    aux_nodes ++;
    /*------------*/

    std::cout<<"Printing AMAC list for node "<<getName()<<std::endl;

    /*-----------------------------------------mod_10---------------------------------------------------*/
    std::fprintf(logs,"\nPrinting AMAC list for node %s :",getName());
    /*-----------------------------------------/mod_10---------------------------------------------------*/

    printAllAMAC();

    /*-----------------------------------------mod_11---------------------------------------------------*/
     if(aux_nodes == num_nodes){

        aux = simTime().dbl() * 1000;
        std::fprintf(logs,"\n\n-- Num.Nodes: %d | Num.Nodes WithoutAMAC: %d | Total AMACs: %d | Conv.Time: %f ms | N: %d | P: %d | L: %d | Node connected to SDNcontroller: %s --",num_nodes,No_AMACNodes,AMACs_Counter,aux,N,P,L,aux_str);
        std::fclose(logs);
        strcpy(aux_command,"rename logs.txt rt-barabasi-7-6_1(");
        strcat(aux_command,aux_str);
        strcat(aux_command,").txt");

        system(aux_command);
     }
    /*-----------------------------------------/mod_11---------------------------------------------------*/

    for(int i=0;i<numberOfPorts;i++)//free allocated memory for AMAC list from each port
    {
        if(portAMACListArray[i]!=nullptr)
        {
            delete portAMACListArray[i];
            portAMACListArray[i]=nullptr;
        }
    }

    delete portAMACListArray;
}
