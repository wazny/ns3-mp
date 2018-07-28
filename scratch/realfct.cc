#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/random-variable-stream.h"
#include "src/internet/model/m-file.h"
//#include "ns3/fat-tree-helper.h"

#include <sstream>
#include <map>
#include <fstream>

#include <iostream>
#include <stdlib.h>

using namespace std;

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DctcpTest");

uint32_t checkTimes;
double avgQueueSize;

// attributes
std::string link_data_rate;
std::string link_delay;
uint32_t packet_size;
uint32_t queue_size;
uint32_t threhold;
double offerload_1 = 0.2;
//int asd[3] = {}
// uint32_t sssss=0;
// int  i=0;
// double m_time=0;
// double m_fct=0;
// uint32_t m_flowsize=30*100000;

EmpiricalRandomVariable fsize,fsize1,fsize2;



// The times
double global_start_time;
double global_stop_time;
double sink_start_time;
double sink_stop_time;
double client_start_time;
double client_stop_time;
double client_interval_time;

// nodes
NodeContainer nodes;
NodeContainer clients;
NodeContainer switchs;
NodeContainer servers;

// server interfaces
Ipv4InterfaceContainer serverInterfaces;

// receive status
std::map<uint32_t, uint64_t> totalRx;   //fId->receivedBytes
std::map<uint32_t, uint64_t> lastRx;

// throughput result
std::map<uint32_t, std::vector<std::pair<double, double> > > throughputResult; //fId -> list<time, throughput>

QueueDiscContainer queueDiscs;

std::stringstream filePlotQueue;
std::stringstream filePlotQueueAvg;

void
CheckQueueSize (Ptr<QueueDisc> queue)
{
  uint32_t qSize = StaticCast<RedQueueDisc> (queue)->GetQueueSize ();

  avgQueueSize += qSize;
  checkTimes++;

  // check queue size every 1/100 of a second
  Simulator::Schedule (Seconds (0.01), &CheckQueueSize, queue);

  std::ofstream fPlotQueue (filePlotQueue.str ().c_str (), std::ios::out|std::ios::app);
  fPlotQueue << Simulator::Now ().GetSeconds () << " " << qSize << std::endl;
  fPlotQueue.close ();

  std::ofstream fPlotQueueAvg (filePlotQueueAvg.str ().c_str (), std::ios::out|std::ios::app);
  fPlotQueueAvg << Simulator::Now ().GetSeconds () << " " << avgQueueSize / checkTimes << std::endl;
  fPlotQueueAvg.close ();
}

void
TxTrace (uint32_t flowId, Ptr<const Packet> p)
{
  NS_LOG_FUNCTION (flowId << p);
  FlowIdTag flowIdTag;
  flowIdTag.SetFlowId (flowId);
  p->AddByteTag (flowIdTag);
}

void
RxTrace (Ptr<const Packet> packet, const Address &from)
{

  NS_LOG_FUNCTION (packet << from);
  FlowIdTag flowIdTag;
  bool retval = packet->FindFirstMatchingByteTag (flowIdTag);
  NS_ASSERT (retval);
  if (totalRx.find (flowIdTag.GetFlowId ()) != totalRx.end ())
    {
      totalRx[flowIdTag.GetFlowId ()] += packet->GetSize ();
      //cout << Simulator::Now().GetSeconds() << totalRx[flowIdTag.GetFlowId ()] << endl;
      // sssss += packet->GetSize();
      // //if(sssss > m_flowsize){
      //   i++;
        
      //   //cout << m_time << endl;


      //   if(sssss > m_flowsize){
      //       m_fct =  Simulator::Now().GetMicroSeconds() - m_time;
      //       m_time = Simulator::Now().GetMicroSeconds();
      //       sssss = 0;
      //       //cout << m_flowsize << endl;
      //       cout << Simulator::Now().GetMicroSeconds() << ":  " << m_flowsize << ":  " << "flow " << i << ":" << m_fct << endl; 
      //       m_flowsize = fsize.GetInteger() * 1000;
      //   }
        //m_flowsize += ((10000+rand())%30*100000);
        //cout << (10000+(rand()%30*100000)) << endl;
        
      //cout << Simulator::Now().GetSeconds() << ":  " <<sssss << endl;
    }
  else
    {
      totalRx[flowIdTag.GetFlowId ()] = packet->GetSize ();
      lastRx[flowIdTag.GetFlowId ()] = 0;
    }
}

void
CalculateThroughput (void)
{
  FlowIdTag flowIdTag;
  for (auto it = totalRx.begin (); it != totalRx.end (); ++it)
    {
      double cur = (it->second - lastRx[it->first]) * (double) 8/1e5; /* Convert Application RX Packets to MBits. */
      throughputResult[it->first].push_back (std::pair<double, double> (Simulator::Now ().GetSeconds (), cur));
      lastRx[it->first] = it->second;
      //cout << Simulator::Now().GetSeconds() << "it->second:" << it->second << "  it->first:" << it->first << "   lastRx[it->first]:" << lastRx[it->first] << "   cur:" << cur << endl;
      //cout << Simulator::Now().GetSeconds() << "   " << totalRx[it->first] << endl;
    }
  Simulator::Schedule (MilliSeconds (1), &CalculateThroughput);
  //cout << totalRx[flowIdTag.GetFlowId ()] << endl;
  
}

void
SetName (void)
{
  // add name to clients
  int i = 0;
  for(auto it = clients.Begin (); it != clients.End (); ++it)
    {
      std::stringstream ss;
      ss << "CL" << i++;
      Names::Add (ss.str (), *it);
    }
  i = 0;
  for(auto it = switchs.Begin (); it != switchs.End (); ++it)
    {
      std::stringstream ss;
      ss << "SW" << i++;
      Names::Add (ss.str (), *it);
    }
  i = 0;
  for(auto it = servers.Begin (); it != servers.End (); ++it)
    {
      std::stringstream ss;
      ss << "SE" << i++;
      Names::Add (ss.str (), *it);
    }
}
static void
RxDrop (Ptr<PcapFileWrapper> file, Ptr<const Packet> p)
{
  NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
  file->Write(Simulator::Now(), p);
}
void
BuildTopo (uint32_t clientNo, uint32_t serverNo, uint32_t TestType)
{
  NS_LOG_INFO ("Create nodes");
  clients.Create (clientNo);
  switchs.Create (2);
  servers.Create (serverNo);

  SetName ();

  NS_LOG_INFO ("Install internet stack on all nodes.");
  InternetStackHelper internet;
  internet.Install (clients);
  internet.Install (switchs);
  internet.Install (servers);


  TrafficControlHelper tchPfifo;
  TrafficControlHelper tchMulti;
  TrafficControlHelper tchRed;
  // use default limit for pfifo (1000)
  uint16_t handle = tchPfifo.SetRootQueueDisc ("ns3::PfifoFastQueueDisc", "Limit", UintegerValue (queue_size));
  tchPfifo.AddInternalQueues (handle, 3, "ns3::DropTailQueue", "MaxPackets", UintegerValue (queue_size));
  if(TestType == 1 )
  {

    tchRed.SetRootQueueDisc ("ns3::RedQueueDisc", "LinkBandwidth", StringValue (link_data_rate),
                             "LinkDelay", StringValue (link_delay));
  }
  else if(TestType == 2)
  {

    tchMulti.SetRootQueueDisc ("ns3::MultiQueueDisc", "LinkBandwidth", StringValue (link_data_rate),
                               "LinkDelay", StringValue (link_delay));
  }
  
  NS_LOG_INFO ("Create channels");
  PointToPointHelper p2p;

  p2p.SetQueue ("ns3::DropTailQueue");
  p2p.SetDeviceAttribute ("DataRate", StringValue (link_data_rate));
  p2p.SetChannelAttribute ("Delay", StringValue (link_delay));

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  for (auto it = clients.Begin (); it != clients.End (); ++it)
    {
      NetDeviceContainer devs = p2p.Install (NodeContainer (*it, switchs.Get (0)));
      tchPfifo.Install (devs);
      ipv4.Assign (devs);
      ipv4.NewNetwork ();
    }
  for (auto it = servers.Begin (); it != servers.End (); ++it)
    {
      NetDeviceContainer devs = p2p.Install (NodeContainer (switchs.Get (1), *it));
      tchPfifo.Install (devs);
      serverInterfaces.Add (ipv4.Assign (devs).Get (1));
      ipv4.NewNetwork ();
    }


PcapHelper pcapHelper;
Ptr<PcapFileWrapper> file = pcapHelper.CreateFile ("sixth.pcap", std::ios::out, PcapHelper::DLT_PPP);
clients.Get (0)->TraceConnectWithoutContext("PhyTxDrop", MakeBoundCallback (&RxDrop, file));

  {
    p2p.SetQueue ("ns3::DropTailQueue");
    p2p.SetDeviceAttribute ("DataRate", StringValue (link_data_rate));
    p2p.SetChannelAttribute ("Delay", StringValue (link_delay));
    NetDeviceContainer devs = p2p.Install (switchs);
    // only backbone link has RED queue disc
    if(TestType == 1)
    {
      queueDiscs = tchRed.Install (devs);
    }
    else if(TestType == 2)
    {
      queueDiscs = tchMulti.Install (devs);
    }
    ipv4.Assign (devs);
  }
  // Set up the routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
}

void
BuildAppsTest (uint32_t TestType)
{
    uint16_t port = 50000;
    Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
    PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install (servers.Get (0));
    sinkApp.Start (Seconds (sink_start_time));
    sinkApp.Stop (Seconds (sink_stop_time));
    sinkApp.Get (0)->TraceConnectWithoutContext ("Rx", MakeCallback (&RxTrace));

    double interarrival = 0.0028;  // Interarrival mean time
    double tflow = 0.0;
    uint32_t tempsize = 0;
    SeedManager::SetSeed(1);
    Ptr<ExponentialRandomVariable> ev = CreateObject<ExponentialRandomVariable> ();
  if(TestType == 1)
  {
    OnOffHelper clientHelper0 ("ns3::TcpSocketFactory", InetSocketAddress (serverInterfaces.GetAddress (0), port));
    clientHelper0.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper0.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper0.SetAttribute ("DataRate", DataRateValue (DataRate (link_data_rate)));
    clientHelper0.SetAttribute ("PacketSize", UintegerValue (packet_size));


    OnOffHelper clientHelper1 ("ns3::TcpSocketFactory", InetSocketAddress (serverInterfaces.GetAddress (0), port));
    clientHelper1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper1.SetAttribute ("DataRate", DataRateValue (DataRate (link_data_rate)));
    clientHelper1.SetAttribute ("PacketSize", UintegerValue (packet_size));


    OnOffHelper clientHelper2 ("ns3::TcpSocketFactory", InetSocketAddress (serverInterfaces.GetAddress (0), port));
    clientHelper2.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper2.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper2.SetAttribute ("DataRate", DataRateValue (DataRate (link_data_rate)));
    clientHelper2.SetAttribute ("PacketSize", UintegerValue (packet_size));


    OnOffHelper clientHelper3 ("ns3::TcpSocketFactory", InetSocketAddress (serverInterfaces.GetAddress (0), port));
    clientHelper3.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper3.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper3.SetAttribute ("DataRate", DataRateValue (DataRate (link_data_rate)));
    clientHelper3.SetAttribute ("PacketSize", UintegerValue (packet_size));


    for (int i=0; tflow <= global_stop_time ;i++)
    {
      //Dctcp && RedQueue with ECN
      tflow += ev->GetValue(interarrival,0.0)/offerload_1;
      tempsize=fsize1.GetInteger()*1000;
      interarrival = tempsize / 10000000000.0 ;
      // std::cout<<ev->GetValue(interarrival,0.0)<<"   time  &  size    "<<tempsize<<std::endl;
      // set different start/stop time for each app
      clientHelper0.SetAttribute ("MaxBytes", UintegerValue (tempsize));
      ApplicationContainer clientApps0 = clientHelper0.Install (clients.Get(0)); 
      Ptr<Application> the_flow0 = clientApps0.Get(clientApps0.GetN()-1);
      the_flow0->SetStartTime(Seconds(tflow));
      the_flow0->SetStopTime(Seconds(global_stop_time+1.0));
      the_flow0->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&TxTrace, 0));
    }
    tflow = 0.0;
    for (int i=0; tflow <= global_stop_time ;i++)
    {
      //Dctcp && RedQueue with ECN
      tflow += ev->GetValue(interarrival,0.0)/offerload_1;
      tempsize=fsize1.GetInteger()*1000;
      interarrival = tempsize / 10000000000.0 ;
      clientHelper1.SetAttribute ("MaxBytes", UintegerValue (tempsize));
      ApplicationContainer clientApps1 = clientHelper1.Install (clients.Get(1));
      Ptr<Application> the_flow1 = clientApps1.Get(clientApps1.GetN()-1);
      the_flow1->SetStartTime(Seconds(tflow));
      the_flow1->SetStopTime(Seconds(global_stop_time+1.0));
      the_flow1->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&TxTrace, 1));   
    }
    tflow = 0.0;     
    for (int i=0; tflow <= global_stop_time ;i++)
    {
      tflow += ev->GetValue(interarrival,0.0)/offerload_1;
      tempsize=fsize2.GetInteger()*1000;
      interarrival = tempsize / 10000000000.0 ;
      clientHelper2.SetAttribute ("MaxBytes", UintegerValue (tempsize));
      ApplicationContainer clientApps2 = clientHelper2.Install (clients.Get(2));
      Ptr<Application> the_flow2 = clientApps2.Get(clientApps2.GetN()-1);
      the_flow2->SetStartTime(Seconds(tflow));
      the_flow2->SetStopTime(Seconds(global_stop_time+1.0));
      the_flow2->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&TxTrace, 2));
    }
    tflow = 0.0;        
    for (int i=0; tflow <= global_stop_time ;i++)
    {
      tflow += ev->GetValue(interarrival,0.0)/offerload_1;
      tempsize=fsize2.GetInteger()*1000;
      interarrival = tempsize / 10000000000.0 ;
      clientHelper3.SetAttribute ("MaxBytes", UintegerValue (tempsize));
      ApplicationContainer clientApps3 = clientHelper3.Install (clients.Get(3));
      Ptr<Application> the_flow3 = clientApps3.Get(clientApps3.GetN()-1);
      the_flow3->SetStartTime(Seconds(tflow));
      the_flow3->SetStopTime(Seconds(global_stop_time+1.0));
      the_flow3->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&TxTrace, 3));
    }
    // OnOffHelper clientHelper ("ns3::TcpSocketFactory", InetSocketAddress (serverInterfaces.GetAddress (0), port));
    // clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    // clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    // clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate (link_data_rate)));
    // clientHelper.SetAttribute ("PacketSize", UintegerValue (packet_size));
    
    // for (int i=0 ; tflow <= global_stop_time ;i++)
    // {
    //   //Dctcp && RedQueue with ECN

    //   tflow += ev->GetValue(interarrival,0.0);
    //   tempsize=fsize.GetInteger()*1000;
    //   interarrival = tempsize / 10000000000.0 * offerload;
    //   // std::cout<<"interarrival:   "<<interarrival<<"   tflow:  "<<tflow<<std::endl;
    //   // std::cout<<ev->GetValue(interarrival,0.0)<<"   time  &  size    "<<tempsize<<std::endl;


    //   clientHelper.SetAttribute ("MaxBytes", UintegerValue (tempsize));

    //   ApplicationContainer clientApps = clientHelper.Install (clients);
    //   // set different start/stop time for each app
    //   uint32_t j = 1;
    //   for (auto it = clientApps.Begin (); it != clientApps.End (); ++it)
    //     {
    //       Ptr<Application> app = *it;
    //       app->SetStartTime (Seconds (tflow));
    //       app->SetStopTime (Seconds (client_stop_time));
    //       app->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&TxTrace, j++));
    //     }
    // }
  }
  else if(TestType == 2)
  {
    //Dctcp && MultiQueue
    OnOffHelper clientHelper0 ("ns3::TcpSocketFactory", InetSocketAddress (serverInterfaces.GetAddress (0), port));
    clientHelper0.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper0.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper0.SetAttribute ("DataRate", DataRateValue (DataRate (link_data_rate)));
    clientHelper0.SetAttribute ("PacketSize", UintegerValue (packet_size));
    InetSocketAddress tmp0 = InetSocketAddress (serverInterfaces.GetAddress (0), port);
    tmp0.SetTos(0B00000010); 
    clientHelper0.SetAttribute ("Remote", AddressValue(tmp0));

    OnOffHelper clientHelper1 ("ns3::TcpSocketFactory", InetSocketAddress (serverInterfaces.GetAddress (0), port));
    clientHelper1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper1.SetAttribute ("DataRate", DataRateValue (DataRate (link_data_rate)));
    clientHelper1.SetAttribute ("PacketSize", UintegerValue (packet_size));
    InetSocketAddress tmp1 = InetSocketAddress (serverInterfaces.GetAddress (0), port);
    tmp1.SetTos(0B00100010); 
    clientHelper1.SetAttribute ("Remote", AddressValue(tmp1));

    OnOffHelper clientHelper2 ("ns3::TcpSocketFactory", InetSocketAddress (serverInterfaces.GetAddress (0), port));
    clientHelper2.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper2.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper2.SetAttribute ("DataRate", DataRateValue (DataRate (link_data_rate)));
    clientHelper2.SetAttribute ("PacketSize", UintegerValue (packet_size));
    InetSocketAddress tmp2 = InetSocketAddress (serverInterfaces.GetAddress (0), port);
    tmp2.SetTos(0B01000010); 
    clientHelper2.SetAttribute ("Remote", AddressValue(tmp2));

    OnOffHelper clientHelper3 ("ns3::TcpSocketFactory", InetSocketAddress (serverInterfaces.GetAddress (0), port));
    clientHelper3.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper3.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper3.SetAttribute ("DataRate", DataRateValue (DataRate (link_data_rate)));
    clientHelper3.SetAttribute ("PacketSize", UintegerValue (packet_size));
    InetSocketAddress tmp3 = InetSocketAddress (serverInterfaces.GetAddress (0), port);
    tmp3.SetTos(0B01100010); 
    clientHelper3.SetAttribute ("Remote", AddressValue(tmp3));



    for (int i=0; tflow <= global_stop_time ;i++)
    {
      tflow += ev->GetValue(interarrival,0.0)/offerload_1;
      tempsize=fsize1.GetInteger()*1000;
      interarrival = tempsize / 10000000000.0 ;
      // std::cout<<ev->GetValue(interarrival,0.0)<<"   time  &  size    "<<tempsize<<std::endl;
      // set different start/stop time for each app
      clientHelper0.SetAttribute ("MaxBytes", UintegerValue (tempsize));
      ApplicationContainer clientApps0 = clientHelper0.Install (clients.Get(0)); 
      Ptr<Application> the_flow0 = clientApps0.Get(clientApps0.GetN()-1);
      the_flow0->SetStartTime(Seconds(tflow));
      the_flow0->SetStopTime(Seconds(global_stop_time+1.0));
      the_flow0->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&TxTrace, 0));
    }
    tflow = 0.0;
    for (int i=0; tflow <= global_stop_time ;i++)
    {
      tflow += ev->GetValue(interarrival,0.0)/offerload_1;
      tempsize=fsize1.GetInteger()*1000;
      interarrival = tempsize / 10000000000.0 ;
      clientHelper1.SetAttribute ("MaxBytes", UintegerValue (tempsize));
      ApplicationContainer clientApps1 = clientHelper1.Install (clients.Get(1));
      Ptr<Application> the_flow1 = clientApps1.Get(clientApps1.GetN()-1);
      the_flow1->SetStartTime(Seconds(tflow));
      the_flow1->SetStopTime(Seconds(global_stop_time+1.0));
      the_flow1->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&TxTrace, 1));        
    }
    tflow = 0.0;
    for (int i=0; tflow <= global_stop_time ;i++)
    {
      tflow += ev->GetValue(interarrival,0.0)/offerload_1;
      tempsize=fsize2.GetInteger()*1000;
      interarrival = tempsize / 10000000000.0 ;
      clientHelper2.SetAttribute ("MaxBytes", UintegerValue (tempsize));
      ApplicationContainer clientApps2 = clientHelper2.Install (clients.Get(2));
      Ptr<Application> the_flow2 = clientApps2.Get(clientApps2.GetN()-1);
      the_flow2->SetStartTime(Seconds(tflow));
      the_flow2->SetStopTime(Seconds(global_stop_time+1.0));
      the_flow2->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&TxTrace, 2));        
    }
    tflow = 0.0;
    for (int i=0; tflow <= global_stop_time ;i++)
    {
      tflow += ev->GetValue(interarrival,0.0)/offerload_1;
      tempsize=fsize2.GetInteger()*1000;
      interarrival = tempsize / 10000000000.0 ;
      clientHelper3.SetAttribute ("MaxBytes", UintegerValue (tempsize));
      ApplicationContainer clientApps3 = clientHelper3.Install (clients.Get(3));
      Ptr<Application> the_flow3 = clientApps3.Get(clientApps3.GetN()-1);
      the_flow3->SetStartTime(Seconds(tflow));
      the_flow3->SetStopTime(Seconds(global_stop_time+1.0));
      the_flow3->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&TxTrace, 3));
      }



  }
  else if(TestType ==3)
  {
    //DCQCN

  }


  // Connection one
  // Clients are in left side
  /*
   * Create the OnOff applications to send TCP to the server
   * onoffhelper is a client that send data to TCP destination
   */



  // OnOffHelper clientHelper1 ("ns3::TcpSocketFactory", InetSocketAddress (serverInterfaces.GetAddress (0), port));
  // clientHelper1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  // clientHelper1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  // clientHelper1.SetAttribute ("DataRate", DataRateValue (DataRate (link_data_rate)));
  // clientHelper1.SetAttribute ("PacketSize", UintegerValue (packet_size));

  // ApplicationContainer clientApps1 = clientHelper1.Install (clients);

  // // set different start/stop time for each app
  // uint32_t j = 1;
  // for (auto it = clientApps1.Begin (); it != clientApps1.End (); ++it)
  //   {
  //     Ptr<Application> app = *it;
  //     app->SetStartTime (Seconds (clientStartTime));
  //     app->SetStopTime (Seconds (clientStopTime));
  //     app->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&TxTrace, j++));
  //     clientStartTime += client_interval_time;
  //     clientStopTime -=  client_interval_time;
  //   }

  // OnOffHelper clientHelper2 ("ns3::TcpSocketFactory", InetSocketAddress (serverInterfaces.GetAddress (0), port));
  // clientHelper2.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  // clientHelper2.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  // clientHelper2.SetAttribute ("DataRate", DataRateValue (DataRate (link_data_rate)));
  // clientHelper2.SetAttribute ("PacketSize", UintegerValue (packet_size));
  // clientHelper2.SetAttribute ("MaxBytes", UintegerValue (10000));

  // ApplicationContainer clientApps2 = clientHelper2.Install (clients);
  // clientStartTime = client_start_time+0.1;
  // clientStopTime = client_stop_time;
  // // set different start/stop time for each app
  // uint32_t z = 1;
  // for (auto it = clientApps2.Begin (); it != clientApps2.End (); ++it)
  //   {
  //     Ptr<Application> app = *it;
  //     app->SetStartTime (Seconds (clientStartTime));
  //     app->SetStopTime (Seconds (clientStopTime));
  //     app->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&TxTrace, z++));
  //     clientStartTime += client_interval_time;
  //     clientStopTime -=  client_interval_time;
  //   }
}

void
SetConfig (uint32_t TestType)
{

  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (false));
  // RED params
  if(TestType == 1)
  {
    //Dctcp && RedQueue with ECN
    NS_LOG_INFO ("Set RED params");
    Config::SetDefault ("ns3::RedQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
    Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (packet_size));
    // Config::SetDefault ("ns3::RedQueueDisc::Gentle", BooleanValue (false));
    Config::SetDefault ("ns3::RedQueueDisc::QW", DoubleValue (1.0));
    Config::SetDefault ("ns3::RedQueueDisc::UseMarkP", BooleanValue (true));
    Config::SetDefault ("ns3::RedQueueDisc::MarkP", DoubleValue (2.0));
    Config::SetDefault ("ns3::RedQueueDisc::MinTh", DoubleValue (threhold));
    Config::SetDefault ("ns3::RedQueueDisc::MaxTh", DoubleValue (threhold));
    Config::SetDefault ("ns3::RedQueueDisc::QueueLimit", UintegerValue (queue_size));

    // TCP params
    Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (packet_size));
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
    // Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));
    Config::SetDefault ("ns3::TcpSocketBase::UseEcn", BooleanValue (true));
    Config::SetDefault ("ns3::RedQueueDisc::UseEcn", BooleanValue (true));

    Config::SetDefault ("ns3::TcpL4Protocol::SocketBaseType", TypeIdValue(TypeId::LookupByName ("ns3::DctcpSocket")));
    Config::SetDefault ("ns3::DctcpSocket::DctcpWeight", DoubleValue (1.0 / 16));

    Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1000 - 42));
    Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));
  }
  else if(TestType == 2)
  {
    //Dctcp with MultiQueue
    NS_LOG_INFO ("Set MultiQueue params");
    Config::SetDefault ("ns3::MultiQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
    Config::SetDefault ("ns3::MultiQueueDisc::MeanPktSize", UintegerValue (packet_size));
    Config::SetDefault ("ns3::MultiQueueDisc::MinTh", DoubleValue (threhold));
    Config::SetDefault ("ns3::MultiQueueDisc::MaxTh", DoubleValue (threhold));
    Config::SetDefault ("ns3::MultiQueueDisc::QueueLimit", UintegerValue (queue_size));
    Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (packet_size));
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
    // Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));
    Config::SetDefault ("ns3::TcpSocketBase::UseEcn", BooleanValue (true));

    Config::SetDefault ("ns3::TcpL4Protocol::SocketBaseType", TypeIdValue(TypeId::LookupByName ("ns3::DctcpSocket")));
    Config::SetDefault ("ns3::DctcpSocket::DctcpWeight", DoubleValue (1.0 / 16));

    Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1000 - 42));
    Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));

  }
  else if(TestType == 3)
  {
    //DCQCN
  }
  
}

int
main (int argc, char *argv[])
{
  //std::cout << "DDDDD" << std::endl;
  m_start2end.open("ns3-output.txt");
  m_fct.open("ns3-fct-output.txt");
  // LogComponentEnable ("DctcpSocket", LOG_LEVEL_DEBUG);
  fsize.CDF(10.0,0.5);
  fsize.CDF(20000.0,1.0);

  fsize1.CDF(40.0,0.2);
  fsize1.CDF(80000.0,1.0);

  fsize2.CDF(40.0,0.8);
  fsize2.CDF(80000.0,1.0);


  // m_flowsize = fsize.GetInteger() * 1000;
  // std::cout<<"m_flowsize:   "<<m_flowsize;
  uint32_t TestType; // 1:Dctcp&&RedQueue  2:Dctcp+MultiQueue 3:DCQCN
  TestType = 2;  

  std::string pathOut;
  bool writeForPlot = false;
  bool writePcap = false;
  bool flowMonitor = false;
  bool writeThroughput = false;

  bool printRedStats = true;

  global_start_time = 0.0;
  global_stop_time = 0.5;
  sink_start_time = global_start_time;
  sink_stop_time = global_stop_time + 3.0;
  client_start_time = sink_start_time + 0.1;


  client_stop_time = global_stop_time;
  client_interval_time = 0.00005;

  link_data_rate = "10Gbps";
  link_delay = "50us";
  packet_size = 958;
  queue_size = 250;
  threhold = 20;

  // Will only save in the directory if enable opts below
  pathOut = "."; // Current directory
  CommandLine cmd;

  cmd.AddValue ("pathOut", "Path to save results from --writeForPlot/--writePcap/--writeFlowMonitor", pathOut);
  cmd.AddValue ("writeForPlot", "<0/1> to write results for plot (gnuplot)", writeForPlot);
  cmd.AddValue ("writePcap", "<0/1> to write results in pcapfile", writePcap);
  cmd.AddValue ("writeFlowMonitor", "<0/1> to enable Flow Monitor and write their results", flowMonitor);
  cmd.AddValue ("writeThroughput", "<0/1> to write throughtput results", writeThroughput);

  cmd.Parse (argc, argv);

  SetConfig (TestType);
  BuildTopo (4, 1, TestType);
  BuildAppsTest (TestType);

  if (writePcap)
    {
      PointToPointHelper ptp;
      std::stringstream stmp;
      stmp << pathOut << "/dctcp";
      ptp.EnablePcapAll (stmp.str ());
    }

  Ptr<FlowMonitor> flowmon;
  if (flowMonitor)
    {
      FlowMonitorHelper flowmonHelper;
      flowmon = flowmonHelper.InstallAll ();
    }

  if (writeForPlot)
    {
      filePlotQueue << pathOut << "/" << "red-queue.plotme";
      filePlotQueueAvg << pathOut << "/" << "red-queue_avg.plotme";

      remove (filePlotQueue.str ().c_str ());
      remove (filePlotQueueAvg.str ().c_str ());
      Ptr<QueueDisc> queue = queueDiscs.Get (0);
      Simulator::ScheduleNow (&CheckQueueSize, queue);
    }

  if (writeThroughput)
    {
      Simulator::Schedule (Seconds (sink_start_time), &CalculateThroughput);
    }

  Simulator::Stop (Seconds (sink_stop_time+4.0));
  Simulator::Run ();

  if (flowMonitor)
    {
      std::stringstream stmp;
      stmp << pathOut << "/red.flowmon";

      flowmon->SerializeToXmlFile (stmp.str (), false, false);
    }

  if (printRedStats)
    {
      RedQueueDisc::Stats st = StaticCast<RedQueueDisc> (queueDiscs.Get (0))->GetStats ();
      std::cout << "*** RED stats from Node 2 queue ***" << std::endl;
      std::cout << "\t " << st.unforcedDrop << " drops due prob mark" << std::endl;
      std::cout << "\t " << st.unforcedMark << " marks due prob mark" << std::endl;
      std::cout << "\t " << st.forcedDrop << " drops due hard mark" << std::endl;
      std::cout << "\t " << st.qLimDrop << " drops due queue full" << std::endl;

      st = StaticCast<RedQueueDisc> (queueDiscs.Get (1))->GetStats ();
      std::cout << "*** RED stats from Node 3 queue ***" << std::endl;
      std::cout << "\t " << st.unforcedDrop << " drops due prob mark" << std::endl;
      std::cout << "\t " << st.unforcedMark << " marks due prob mark" << std::endl;
      std::cout << "\t " << st.forcedDrop << " drops due hard mark" << std::endl;
      std::cout << "\t " << st.qLimDrop << " drops due queue full" << std::endl;
    }

  if (writeThroughput)
    {
      for (auto& resultList : throughputResult)
        {
          std::stringstream ss;
          ss << pathOut << "/throughput-" << resultList.first << ".txt";
          std::ofstream out (ss.str ());
          for (auto& entry : resultList.second)
            {
              out << entry.first << "," << entry.second << std::endl;
            }
        }
    }
  Simulator::Destroy ();
  m_start2end.close();
  m_fct.close();

  return 0;
}
