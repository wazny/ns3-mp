#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/random-variable-stream.h"
#include "src/internet/model/m-file.h"
#include <iomanip>
#include <sstream>
#include <map>
#include <fstream>

#include <iostream>
#include <stdlib.h>
#include <string>

using namespace std;

using namespace ns3;

uint32_t checkTimes;
double avgQueueSize;

// attributes
std::string link_data_rate;
std::string link_delay;
std::string  m_heRate;
std::string  m_eaRate;
std::string  m_acRate;
std::string  m_heDelay;
std::string  m_eaDelay;
std::string  m_acDelay;

uint32_t packet_size;
uint32_t queue_size;
uint32_t threhold;
double offerload_1 = 0.3;
double offerload_2 = 0.9;
//int asd[3] = {}
// uint32_t sssss=0;
// int  i=0;
// double m_time=0;
// double m_fct=0;
// uint32_t m_flowsize=30*100000;

EmpiricalRandomVariable fsize,fsize1,fsize2;


NodeContainer m_node;
NodeContainer m_core;
NodeContainer m_aggr;
NodeContainer m_host;
NodeContainer m_edge;

uint32_t nsender;
uint32_t nreceiver;



// The times
double global_start_time;
double global_stop_time;
double sink_start_time;
double sink_stop_time;
double client_start_time;
double client_stop_time;
double client_interval_time;

// server interfaces
Ipv4InterfaceContainer serverInterfaces;

// receive status
std::map<uint32_t, uint64_t> totalRx;   //fId->receivedBytes
std::map<uint32_t, uint64_t> lastRx;

// throughput result
std::map<uint32_t, std::vector<std::pair<double, double> > > throughputResult; //fId -> list<time, throughput>


void
BuildAppsTest (uint32_t k)
{
  uint32_t total_host = k*k*k/4;
  for (float offerload = 0.1; offerload<1.1;offerload+=0.1){
    std::stringstream ss, trace;
    char tracefilename[256];
    // ss<< "new_trace/offerload_"<<offerload<<".tr";
    // ss>>tracefilename;
    sprintf(tracefilename, "trace_0.2/offerload_%.1f.tr", offerload);
    ofstream tracefile(tracefilename);
    double interarrival = 0.0;  // Interarrival mean time
    uint32_t pg = 0 ; 
    uint32_t m_count=0;
    uint32_t tempsize = 0;
    SeedManager::SetSeed(2);
    Ptr<ExponentialRandomVariable> ev = CreateObject<ExponentialRandomVariable> ();
    for(uint32_t j=0 ; j<total_host; j++)
    {
      double tflow = 0.0;
      for (int i=0;tflow <= global_stop_time ;i++)
      {

        tflow += ev->GetValue(interarrival,0.0)/(offerload*0.1);
        tempsize=fsize1.GetInteger();

        nreceiver = rand() % total_host;

        if(tempsize == 100)
        {
          // tmp1.SetTos(0B01000010);
          pg=2; 
        }
        else
        {
           pg=3;
        }
        interarrival = tempsize / 10000000.0 ;
        
        // Randomly select a sender
        nsender = j;

        while (nsender == nreceiver)
        {
          nreceiver = rand() % total_host;
        } // make sure that client and server are different

        trace << nsender+20 << " " << nreceiver+20 << " " << pg << " " << tempsize << " " << setprecision(8) << tflow << " " << global_stop_time << endl;
        m_count++;
      }
    }
    tracefile<<m_count<<std::endl;
    tracefile<<trace.rdbuf();
    tracefile.close();
    std::cout << "m_count:" << m_count << std::endl;
    // m_start2end << "First line: flow #src dst priority packet# start_time end_time" << endl;
  }


}

int
main (int argc, char *argv[])
{
  fsize.CDF(10.0,0.5);
  fsize.CDF(20000.0,1.0);

  fsize1.CDF(100.0,0.8);
  fsize1.CDF(10000.0,0.8);
  fsize1.CDF(10000.0,1.0);

  fsize2.CDF(100.0,0.8);
  fsize2.CDF(10000.0,1.0);

  global_start_time = 0.0;
  global_stop_time = 3;

  uint32_t k = 4;

  BuildAppsTest (k);

  return 0;
}
