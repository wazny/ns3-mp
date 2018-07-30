#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/random-variable-stream.h"
// #include "ns3/m-file.h"

#include <sstream>
#include <map>
#include <fstream>

#include <iostream>
#include <stdlib.h>
#include <string>
#include <stdio.h>

using namespace std;
using namespace ns3;


NS_LOG_COMPONENT_DEFINE ("DctcpTest");

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

std::string temp;

uint32_t packet_size;
uint32_t queue_size;
uint32_t threshold;
double offerload_1 = 0.3;
bool packet_level_ecmp = true;
bool flow_level_ecmp = false; 
//int asd[3] = {}
// uint32_t sssss=0;
// int  i=0;
// double m_time=0;
// double m_fct=0;
// uint32_t m_flowsize=30*100000;

// EmpiricalRandomVariable fsize,fsize1;


NodeContainer m_node;
NodeContainer m_core;
NodeContainer m_aggr;
NodeContainer m_host;
NodeContainer m_edge;

uint32_t nsender;
uint32_t nreceiver;
uint32_t m_count=0;


// The times
double global_start_time;
double global_stop_time;
double sink_start_time;
double sink_stop_time;
// double client_start_time;
// double client_stop_time;
// double client_interval_time;

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

//2018.07.28 Modified Version
uint32_t host_num;
uint32_t g_counter = 0;
std::ifstream g_trace_file_forward;
std::ifstream g_trace_file_backward;
uint64_t* g_flow_count;
uint64_t* g_recv_count;

bool turning_flag = false;

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
  // FlowIdTag flowIdTag;
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

//2018.07.28 Modified Version

void PacketSendEvent (Ptr<const Packet> p);
void PacketRecvEventForward (Ptr<const Packet> p, const Address &a);
void SetSendForward (int c);
void SetSendBackward (int c);
void InitApp (uint32_t k, std::string trace_f, std::string trace_b);
void CleanUp ();

void
BuildTopo (uint32_t k)
{
  NS_LOG_INFO ("Create nodes");
  const unsigned N = k>>1;
  const unsigned numST = 2*N;
  const unsigned numCore = N*N;
  const unsigned numAggr = numST * N;
  const unsigned numEdge = numST * N;
  const unsigned numHost = numEdge * N;
  const unsigned numTotal= numCore + numAggr + numEdge + numHost;

  // ObjectFactory m_channelFactory;
  // ObjectFactory m_deviceFactory;

  // m_deviceFactory.SetTypeId ("ns3::PointToPointNetDevice");
  // m_channelFactory.SetTypeId ("ns3::PointToPointChannel");

  NS_LOG_LOGIC ("Creating fat-tree nodes.");
  m_node.Create(numTotal);

  for(unsigned j=0;j<2*N;j++) 
  { // For every subtree
    for(unsigned i=j*2*N; i<=j*2*N+N-1; i++) 
    { // First N nodes
      m_edge.Add(m_node.Get(i));
    }
    for(unsigned i=j*2*N+N; i<=j*2*N+2*N-1; i++) 
    { // Last N nodes
      m_aggr.Add(m_node.Get(i));
    }
  }
  for(unsigned i=4*N*N; i<5*N*N; i++) 
  {
    m_core.Add(m_node.Get(i));
  }
  for(unsigned i=5*N*N; i<numTotal; i++)
  {
    m_host.Add(m_node.Get(i));
  }

  NS_LOG_LOGIC ("Install internet stack on all nodes.");
  InternetStackHelper internet;
  internet.Install (m_node);

  
 
  NS_LOG_LOGIC ("Creating connections and set-up addresses.");
  TrafficControlHelper tchPfifo;
  TrafficControlHelper tchMulti;
  TrafficControlHelper tchRed;
  TrafficControlHelper tchTcn;

  uint16_t handle =  tchPfifo.SetRootQueueDisc ("ns3::PfifoFastQueueDisc", "Limit", UintegerValue (queue_size));
  //tchPfifo.SetRootQueueDisc ("ns3::DropTailQueue");
  tchPfifo.AddInternalQueues (handle, 3, "ns3::DropTailQueue", "MaxPackets", UintegerValue (queue_size));


  tchRed.SetRootQueueDisc ("ns3::RedQueueDisc", "LinkBandwidth", StringValue (link_data_rate),
                                "LinkDelay", StringValue (link_delay));
  tchMulti.SetRootQueueDisc ("ns3::RedQueueDisc", "LinkBandwidth", StringValue (link_data_rate),
                              "LinkDelay", StringValue (link_delay));

  tchTcn.SetRootQueueDisc ("ns3::TcnQueueDisc", "LinkBandwidth", StringValue (link_data_rate),
                              "LinkDelay", StringValue (link_delay));
  NS_LOG_INFO ("Create channels");

  PointToPointHelper p2p;
  p2p.SetQueue ("ns3::DropTailQueue");

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer tmp;
  p2p.SetDeviceAttribute ("DataRate", StringValue (m_heRate));
  p2p.SetChannelAttribute ("Delay", StringValue (m_heDelay));
  for (unsigned j=0; j<numST; j++) 
  { // For each subtree
    for(unsigned i=0; i<N; i++) 
    { // For each edge
      for(unsigned m=0; m<N; m++) 
      { 
      // For each port of edge
        // Connect edge to host
        Ptr<Node> eNode = m_edge.Get(j*N+i);
        Ptr<Node> hNode = m_host.Get(j*N*N+i*N+m);
        NetDeviceContainer devs = p2p.Install (eNode, hNode);
        tchMulti.Install (devs);
        tmp = ipv4.Assign (devs);
        ipv4.NewNetwork ();
        serverInterfaces.Add (tmp.Get (1));

      }
    }
  }
  p2p.SetDeviceAttribute ("DataRate", StringValue (m_eaRate));
  p2p.SetChannelAttribute ("Delay", StringValue (m_eaDelay));
  for (unsigned j=0; j<numST; j++) 
  { // For each subtree
    for(unsigned i=0; i<N; i++) 
    { // For each edge
      for(unsigned m=0; m<N; m++) 
      { 
        // For each aggregation
        // Connect edge to aggregation
        Ptr<Node> aNode = m_aggr.Get(j*N+m);
        Ptr<Node> eNode = m_edge.Get(j*N+i);
        NetDeviceContainer devs = p2p.Install (aNode, eNode);
        queueDiscs = tchMulti.Install (devs);
        ipv4.Assign (devs);
        ipv4.NewNetwork ();
      } 
    }
  }
  p2p.SetDeviceAttribute ("DataRate", StringValue (m_acRate));
  p2p.SetChannelAttribute ("Delay", StringValue (m_acDelay));
  for(unsigned j=0; j<numST; j++) 
  { // For each subtree
    for(unsigned i=0; i<N; i++) 
    { // For each aggr
      for(unsigned m=0; m<N; m++) 
      { 
        // For each port of aggr
        // Connect aggregation to core
        Ptr<Node> cNode = m_core.Get(i*N+m);
        Ptr<Node> aNode = m_aggr.Get(j*N+i);
        NetDeviceContainer devs = p2p.Install (aNode, cNode);
        queueDiscs = tchMulti.Install (devs);
        ipv4.Assign (devs);
        ipv4.NewNetwork ();
      }
    }
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
}

void
BuildAppsTest (uint32_t k, std::string trace)
{
    ifstream flowf(trace);
    ApplicationContainer flows;
    ApplicationContainer sinkApps;
    // uint32_t total_host = k*k*k/4;
    uint16_t port = 10000;
    Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
    PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install (m_host);
    sinkApp.Start (Seconds (sink_start_time));
    sinkApp.Stop (Seconds (sink_stop_time));
    // sinkApp.Get (0)->TraceConnectWithoutContext ("Rx", MakeCallback (&RxTrace));


    

    // double interarrival = 0.0028;  // Interarrival mean time
    double tflow = 0.0;
    uint32_t tempsize = 0;
    // SeedManager::SetSeed(1);
    // Ptr<ExponentialRandomVariable> ev = CreateObject<ExponentialRandomVariable> ();
    //for (int i=0;tflow <= global_stop_time ;i++)
    //m_start22end.open("ns3-s-e-output.txt");
    int flow_count;
    flowf >> flow_count;
    for (int i=0;i<flow_count;i++)
    // while(!flowf.eof())
    {
      // getline(flowf, temp, '\n');

        int proi;
        flowf >> nsender >> nreceiver >> proi >> tempsize >> tflow >> global_stop_time;
        nsender -= 20;
        nreceiver -= 20;
        tempsize *= 1000;
      // if(temp[0]>='0' && temp[0]<='9' && temp.size() > 10)
        if (true)
      {
        m_count++;
        port++;
       
        Ptr<Node> receiver = m_host.Get(nreceiver);
        Ptr<NetDevice> ren = receiver->GetDevice(0);
        Ptr<Ipv4> ipv4 = receiver->GetObject<Ipv4> ();

        Ipv4InterfaceAddress r_ip = ipv4->GetAddress (1,0);
        Ipv4Address r_ipaddr = r_ip.GetLocal();

        // Initialize On/Off Application with addresss of the receiver
        OnOffHelper source ("ns3::TcpSocketFactory", Address (InetSocketAddress(r_ipaddr, port)));
        source.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
        source.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
        source.SetAttribute ("DataRate", StringValue ("10Gbps"));
        source.SetAttribute ("DataRate", DataRateValue (DataRate (link_data_rate)));
        source.SetAttribute ("PacketSize", UintegerValue (packet_size));
        InetSocketAddress tmp1 = InetSocketAddress (r_ipaddr, port);

        tmp1.SetTos((0B00000010|(0xff&proi<<5)));

        source.SetAttribute ("Remote", AddressValue(tmp1));
        
        // Set the amount of data to send in bytes according to the empirical distribution.  
        //int tempsize = fsize.GetInteger();
        

        //std::cout << "flow size is " << tempsize << std::endl;
        source.SetAttribute ("MaxBytes", UintegerValue (tempsize));
        source.SetAttribute("PacketSize",UintegerValue (packet_size));
        
        // Install sink 
        PacketSinkHelper sink("ns3::TcpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
        sinkApps.Add(sink.Install(m_host.Get(nreceiver)));
        
        // Install source to the sender
        NodeContainer onoff;
        onoff.Add(m_host.Get(nsender));
        flows.Add(source.Install (onoff));
        // Get the current flow
        Ptr<Application> the_flow = flows.Get(flows.GetN()-1);
        the_flow->SetStartTime(Seconds(tflow));
        flows.Stop(Seconds(global_stop_time));
        sinkApps.Start(Seconds(0));
        sinkApps.Stop(Seconds(global_stop_time));
        //m_start2end << nsender+20 << " " << nreceiver+20 << " " << 3 << " " << tempsize/1000 << " " << tflow << " " << global_stop_time+26 << endl;
        //m_start2end >> nsender >> " " >> nreceiver >> " " >> 3 >> " " >> tempsize >> " " >> tflow >> " " >> global_stop_time+1 >> endl;
        //cout << nsender+20 << " " << nreceiver+20 << " " << 3 << " " << tempsize/1000 << " " << tflow << " " << global_stop_time+1 << endl;
      }
      
    }
    std::cout << "m_count:" << m_count << std::endl;
    flowf.close();
    //m_start2end << "First line: flow #src dst priority packet# start_time end_time" << endl;

}

void
SetConfig ()
{

  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (false));

    //Dctcp with MultiQueue
    NS_LOG_INFO ("Set MultiQueue params");
    Config::SetDefault ("ns3::MultiQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
    Config::SetDefault ("ns3::MultiQueueDisc::MeanPktSize", UintegerValue (packet_size));
    Config::SetDefault ("ns3::MultiQueueDisc::MinTh", DoubleValue (threshold));
    Config::SetDefault ("ns3::MultiQueueDisc::MaxTh", DoubleValue (threshold));
    Config::SetDefault ("ns3::MultiQueueDisc::QueueLimit", UintegerValue (queue_size));

    NS_LOG_INFO ("Set RED params");
    Config::SetDefault ("ns3::RedQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
    Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (packet_size));
    Config::SetDefault ("ns3::RedQueueDisc::MinTh", DoubleValue (threshold));
    Config::SetDefault ("ns3::RedQueueDisc::MaxTh", DoubleValue (threshold));
    Config::SetDefault ("ns3::RedQueueDisc::QueueLimit", UintegerValue (queue_size));
    Config::SetDefault ("ns3::RedQueueDisc::UseEcn", BooleanValue (true));
    Config::SetDefault ("ns3::RedQueueDisc::UseMarkP", BooleanValue (true));

    Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (packet_size));
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));

    // Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));
    Config::SetDefault ("ns3::TcpSocketBase::UseEcn", BooleanValue (true));

    Config::SetDefault ("ns3::TcpL4Protocol::SocketBaseType", TypeIdValue(TypeId::LookupByName ("ns3::DctcpSocket")));
    Config::SetDefault ("ns3::DctcpSocket::DctcpWeight", DoubleValue (1.0 / 16));

    Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1000 - 42));
    Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));

    //Config::SetDefault ("ns3::Ipv4GlobalRouting::RandomEcmpRouting", BooleanValue(true));
    Config::SetDefault ("ns3::Ipv4GlobalRouting::RandomEcmpRouting", BooleanValue(packet_level_ecmp));
    Config::SetDefault ("ns3::Ipv4GlobalRouting::FlowEcmpRouting", BooleanValue(flow_level_ecmp));
  
}

int
main (int argc, char *argv[])
{
  // LogComponentEnable ("DctcpSocket", LOG_LEVEL_DEBUG);
  // fsize.CDF(10.0,0.5);
  // fsize.CDF(20000.0,1.0);

  // fsize1.CDF(40.0,0.8);
  // fsize1.CDF(80000.0,1.0); 
  // m_flowsize = fsize.GetInteger() * 1000;
  // std::cout<<"m_flowsize:   "<<m_flowsize;


  std::string pathOut = "."; // Current directory
  std::string trace = "trace/offerload_0.1.tr";
  bool writeForPlot = false;
  bool writePcap = false;
  bool flowMonitor = true;
  bool writeThroughput = true;

  bool printRedStats = true;
  bool ecnLimit = false;
  uint32_t initCwnd = 15;

  global_start_time = 0.0;
  global_stop_time = 100    ;
  sink_start_time = global_start_time;
  sink_stop_time = 100;//global_stop_time + 60.0;


  link_data_rate = "10Gbps";
  link_delay = "0us";
  packet_size = 958;
  queue_size = 1000;
  threshold = 20;

  m_heRate = "10Gbps";
  m_eaRate = "10Gbps";
  m_acRate = "10Gbps";
  m_heDelay = "1us";
  m_eaDelay = "1us";
  m_acDelay = "1us";

  uint32_t k = 4;

  // Will only save in the directory if enable opts below
  CommandLine cmd;

  cmd.AddValue ("trace", "Path of trace file", trace);
  cmd.AddValue ("initCwnd", "InitialCwnd of TCP socket", initCwnd);
  cmd.AddValue ("ecnLimit", "<0/1> to limit only sending once ecn in a rtt", ecnLimit);
  cmd.AddValue ("pathOut", "Path to save results from --writeForPlot/--writePcap/--writeFlowMonitor", pathOut);
  cmd.AddValue ("writeForPlot", "<0/1> to write results for plot (gnuplot)", writeForPlot);
  cmd.AddValue ("writePcap", "<0/1> to write results in pcapfile", writePcap);
  cmd.AddValue ("writeFlowMonitor", "<0/1> to enable Flow Monitor and write their results", flowMonitor);
  cmd.AddValue ("writeThroughput", "<0/1> to write throughtput results", writeThroughput);

  cmd.Parse (argc, argv);

  std::cout<<"trace: "<<trace<<std::endl
  <<"initCwnd: "<<initCwnd<<std::endl
  <<"ecnLimit: "<<ecnLimit<<std::endl;
  SetConfig ();
  Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (initCwnd));
  Config::SetDefault ("ns3::DctcpSocket::ECNLimit", BooleanValue (ecnLimit));
  BuildTopo (k);
  //BuildAppsTest (k,trace);
  //2018.07.28 Modified Version
  LogComponentEnable("DctcpTest", LOG_LEVEL_INFO);
  LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
  InitApp(k, "trace/f.tr", "trace/b.tr");

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

  Simulator::Stop (Seconds (sink_stop_time));
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
  // m_start2end.close();
  // m_fct.close();
  // m_start.close();

  return 0;
}

void PacketSendEvent (Ptr<const Packet> p)
{
  NS_LOG_FUNCTION (Simulator::Now());
  NS_LOG_INFO (Simulator::Now() << " sent");
}

void PacketRecvEventForward (Ptr<const Packet> p, const Address &a)
{
  NS_LOG_FUNCTION (Simulator::Now());
  FlowIdTag f;
  bool lastPacket = p->FindFirstMatchingByteTag (f);
  if (lastPacket)
  {
    //std::stringstream s;
    //p->PrintByteTags(s);
    //NS_LOG_INFO (s.str());
    g_recv_count[g_counter-1] += 1;
    NS_LOG_INFO (Simulator::Now() << " A Flow is Done. Num." << g_recv_count[g_counter - 1]);
  }
  // if (p->GetSize () != 0)
  // {
  //   g_recv_count[g_counter-1] +=1;
  //   NS_LOG_INFO (Simulator::Now () << " received " << p->GetSize());
  // }
  //NS_LOG_INFO (Simulator::Now() << g_recv_count[g_counter-1] );
  if (g_recv_count[g_counter-1] == g_flow_count[g_counter -1])
  {
    NS_LOG_INFO (Simulator::Now () << " A Layer is Done. Num." << g_counter);
    //then another layer
    if (g_counter < host_num - 1 && !turning_flag)
    {
      NS_LOG_INFO (Simulator::Now () << " Next Layer.");
      SetSendForward (g_counter++);
    }
    else
    {
      NS_LOG_INFO (Simulator::Now () << " Turning Point.");
      turning_flag = true;
      SetSendBackward (g_counter--);
    }

  }
}

void SetSendForward (int c)
{
  NS_LOG_FUNCTION (Simulator::Now());
  double tflow = 0.0;
  uint32_t tempsize = 0;
  int flow_count;
  // int packet_count = 0;
  ApplicationContainer flows;
  ApplicationContainer sinkApps;
  uint16_t port = 10000;
  // Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  // PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  // ApplicationContainer sinkApp = sinkHelper.Install (m_host);
  // sinkApp.Start (Seconds (sink_start_time));
  // sinkApp.Stop (Seconds (sink_stop_time));

  g_trace_file_forward >> flow_count;
  g_flow_count[c] = flow_count;
  NS_LOG_INFO (g_flow_count[c] << " flows to send.");
  for (int i=0;i<flow_count;i++)
  {
      int proi;
      g_trace_file_forward >> nsender >> nreceiver >> proi >> tempsize >> tflow >> global_stop_time;
      nsender -= 20;
      nreceiver -= 20;
      tempsize *= 1000;
      m_count++;
      port++;
      
      Ptr<Node> receiver = m_host.Get(nreceiver);
      Ptr<NetDevice> ren = receiver->GetDevice(0);
      Ptr<Ipv4> ipv4 = receiver->GetObject<Ipv4> ();

      Ipv4InterfaceAddress r_ip = ipv4->GetAddress (1,0);
      Ipv4Address r_ipaddr = r_ip.GetLocal();

      // Initialize On/Off Application with addresss of the receiver
      OnOffHelper source ("ns3::TcpSocketFactory", Address (InetSocketAddress(r_ipaddr, port)));
      source.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      source.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      source.SetAttribute ("DataRate", DataRateValue (DataRate (link_data_rate)));
      source.SetAttribute ("PacketSize", UintegerValue (packet_size));
      InetSocketAddress tmp1 = InetSocketAddress (r_ipaddr, port);

      tmp1.SetTos((0B00000010|(0xff&proi<<5)));

      source.SetAttribute ("Remote", AddressValue(tmp1));
      
      // Set the amount of data to send in bytes according to the empirical distribution.  
      //int tempsize = fsize.GetInteger();
      

      //std::cout << "flow size is " << tempsize << std::endl;
      source.SetAttribute ("MaxBytes", UintegerValue (tempsize));
      source.SetAttribute("PacketSize",UintegerValue (packet_size));

      // packet_count += (tempsize / packet_size + 1);
      
      // Install sink 
      PacketSinkHelper sink("ns3::TcpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
      sinkApps.Add(sink.Install(m_host.Get(nreceiver)));
      Ptr<Application> the_sink = sinkApps.Get(sinkApps.GetN()-1);
      the_sink->TraceConnectWithoutContext("Rx", MakeCallback (&PacketRecvEventForward));
      
      // Install source to the sender
      //NodeContainer onoff;
      //onoff.Add(m_host.Get(nsender));
      flows.Add(source.Install (m_host.Get(nsender)));
      // Get the current flow
      Ptr<Application> the_flow = flows.Get(flows.GetN()-1);
      the_flow->SetStartTime(Seconds(tflow));
      the_flow->TraceConnectWithoutContext("Tx", MakeCallback (&PacketSendEvent));

      flows.Stop(Seconds(global_stop_time));
      sinkApps.Start(Seconds(0));
      sinkApps.Stop(Seconds(global_stop_time));
  }
  // g_flow_count[c] = packet_count;
  // NS_LOG_INFO (Simulator::Now () << " " << g_flow_count[c] << " packets to send.");
}

void SetSendBackward (int c)
{
  NS_LOG_FUNCTION (Simulator::Now());
  NS_LOG_INFO ("Im hit!");
  //Simulator::Stop();
}

void InitApp (uint32_t k, std::string trace_f, std::string trace_b)
{
  NS_LOG_FUNCTION (Simulator::Now());
  g_trace_file_forward.open(trace_f);
  g_trace_file_backward.open(trace_b);
  host_num = k*k*k/4;
  g_flow_count = new uint64_t[host_num]();
  g_recv_count = new uint64_t[host_num]();
  SetSendForward(g_counter++);
  Simulator::ScheduleDestroy(&CleanUp);
}

void CleanUp ()
{
  NS_LOG_FUNCTION(Simulator::Now());
  g_trace_file_forward.close();
  g_trace_file_backward.close();
}