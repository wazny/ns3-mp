#include "ns3/log.h"
#include "ns3/enum.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/simulator.h"
#include "ns3/abort.h"
#include "tcn-queue-disc.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/socket.h"
#include "ns3/data-rate.h"
#include "ns3/ipv4-queue-disc-item.h"
#include "ns3/ipv4-header.h"
#include "m-file.h"
#include "tcn-tag.h"
/*#include "ns3/node.h"                     
  #define NS_LOG_APPEND_CONTEXT \
  if (m_node) { std::clog << " [node " << m_node->GetId () << "] "; }
*/
namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcnQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (TcnQueueDisc);

TypeId TcnQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcnQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName("Internet")
    .AddConstructor<TcnQueueDisc> ()
    .AddAttribute ("Mode",
                   "Determines unit for QueueLimit",
                   EnumValue (Queue::QUEUE_MODE_PACKETS),
                   MakeEnumAccessor (&TcnQueueDisc::SetMode),
                   MakeEnumChecker (Queue::QUEUE_MODE_BYTES, "QUEUE_MODE_BYTES",
                                    Queue::QUEUE_MODE_PACKETS, "QUEUE_MODE_PACKETS"))
    .AddAttribute ("MeanPktSize",
                   "Average of packet size",
                   UintegerValue (500),
                   MakeUintegerAccessor (&TcnQueueDisc::m_meanPktSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("IdlePktSize",
                   "Average packet size used during idle times. Used when m_cautions = 3",
                   UintegerValue (0),
                   MakeUintegerAccessor (&TcnQueueDisc::m_idlePktSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Wait",
                   "True for waiting between dropped packets",
                   BooleanValue (true),
                   MakeBooleanAccessor (&TcnQueueDisc::m_isWait),
                   MakeBooleanChecker ())
    .AddAttribute ("Gentle",
                   "True to increases dropping probability slowly when average queue exceeds maxthresh",
                   BooleanValue (true),
                   MakeBooleanAccessor (&TcnQueueDisc::m_isGentle),
                   MakeBooleanChecker ())
    .AddAttribute ("ARED",
                   "True to enable ARED",
                   BooleanValue (false),
                   MakeBooleanAccessor (&TcnQueueDisc::m_isARED),
                   MakeBooleanChecker ())
    .AddAttribute ("AdaptMaxP",
                   "True to adapt m_curMaxP",
                   BooleanValue (false),
                   MakeBooleanAccessor (&TcnQueueDisc::m_isAdaptMaxP),
                   MakeBooleanChecker ())
    .AddAttribute ("MinTh",
                   "Minimum average length threshold in packets/bytes",
                   DoubleValue (5),
                   MakeDoubleAccessor (&TcnQueueDisc::m_minTh),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("MaxTh",
                   "Maximum average length threshold in packets/bytes",
                   DoubleValue (15),
                   MakeDoubleAccessor (&TcnQueueDisc::m_maxTh),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("QueueLimit",
                   "Queue limit in bytes/packets",
                   UintegerValue (1000),
                   MakeUintegerAccessor (&TcnQueueDisc::SetQueueLimit),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("LInterm",
                   "The maximum probability of dropping a packet",
                   DoubleValue (50),
                   MakeDoubleAccessor (&TcnQueueDisc::m_lInterm),
                   MakeDoubleChecker <double> ())
    .AddAttribute ("TargetDelay",
                   "Target average queuing delay in ARED",
                   TimeValue (Seconds (0.005)),
                   MakeTimeAccessor (&TcnQueueDisc::m_targetDelay),
                   MakeTimeChecker ())
    .AddAttribute ("Interval",
                   "Time interval to update m_curMaxP",
                   TimeValue (Seconds (0.00001)),
                   MakeTimeAccessor (&TcnQueueDisc::m_interval),
                   MakeTimeChecker ())
    .AddAttribute ("Alpha",
                   "Increment parameter for m_curMaxP in ARED",
                   DoubleValue (0.01),
                   MakeDoubleAccessor (&TcnQueueDisc::SetAredAlpha),
                   MakeDoubleChecker <double> (0, 1))
    .AddAttribute ("Beta",
                   "Decrement parameter for m_curMaxP in ARED",
                   DoubleValue (0.9),
                   MakeDoubleAccessor (&TcnQueueDisc::SetAredBeta),
                   MakeDoubleChecker <double> (0, 1))
    .AddAttribute ("LastSet",
                   "Store the last time m_curMaxP was updated",
                   TimeValue (Seconds (0.0)),
                   MakeTimeAccessor (&TcnQueueDisc::m_lastSet),
                   MakeTimeChecker ())
    .AddAttribute ("Rtt",
                   "Round Trip Time to be considered while automatically setting m_bottom",
                   TimeValue (Seconds (0.01)),
                   MakeTimeAccessor (&TcnQueueDisc::m_rtt),
                   MakeTimeChecker ())
    .AddAttribute ("Ns1Compat",
                   "NS-1 compatibility",
                   BooleanValue (false),
                   MakeBooleanAccessor (&TcnQueueDisc::m_isNs1Compat),
                   MakeBooleanChecker ())
    .AddAttribute ("LinkBandwidth",
                   "The RED link bandwidth",
                   DataRateValue (DataRate ("1.5Mbps")),
                   MakeDataRateAccessor (&TcnQueueDisc::m_linkBandwidth),
                   MakeDataRateChecker ())
    .AddAttribute ("LinkDelay",
                   "The RED link delay",
                   TimeValue (MilliSeconds (20)),
                   MakeTimeAccessor (&TcnQueueDisc::m_linkDelay),
                   MakeTimeChecker ())
    .AddAttribute ("UseEcn",
                   "Checks if queue-disc is ECN Capable",
                   BooleanValue (false),
                   MakeBooleanAccessor (&TcnQueueDisc::m_useEcn),
                   MakeBooleanChecker ())
    .AddAttribute ("UseMarkP",
                   "Check if markP is enabled",
                   BooleanValue (false),
                   MakeBooleanAccessor (&TcnQueueDisc::m_useMarkP),
                   MakeBooleanChecker ())
    .AddAttribute ("RoundSendCount",
                   "Every Queue send packet count arrive the number then update the  m_OutQueueRate",
                   UintegerValue (10),
                   MakeUintegerAccessor (&TcnQueueDisc::m_RoundSendCount),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MinDelay",
                   "min  queuing delay",
                   TimeValue (MicroSeconds(20.0)),
                   MakeTimeAccessor (&TcnQueueDisc::m_MinDelay),
                   MakeTimeChecker ())
    .AddAttribute ("MaxDelay",
                   " max  queuing delay",
                   TimeValue (MicroSeconds (20.0)),
                   MakeTimeAccessor (&TcnQueueDisc::m_MaxDelay),
                   MakeTimeChecker  ())

  ;

  return tid;
}

TcnQueueDisc::TcnQueueDisc () :
  QueueDisc ()
{
  NS_LOG_FUNCTION (this);
  m_uv = CreateObject<UniformRandomVariable> ();
}

TcnQueueDisc::~TcnQueueDisc ()
{
  NS_LOG_FUNCTION (this);
}

void
TcnQueueDisc::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_uv = 0;
  QueueDisc::DoDispose ();
}

void
TcnQueueDisc::SetMode (Queue::QueueMode mode)
{
  NS_LOG_FUNCTION (this << mode);
  m_mode = mode;
}

Queue::QueueMode
TcnQueueDisc::GetMode (void)
{
  NS_LOG_FUNCTION (this);
  return m_mode;
}

void
TcnQueueDisc::SetAredAlpha (double alpha)
{
  NS_LOG_FUNCTION (this << alpha);
  m_alpha = alpha;

  if (m_alpha > 0.01)
    {
      NS_LOG_WARN ("Alpha value is above the recommended bound!");
    }
}

double
TcnQueueDisc::GetAredAlpha (void)
{
  NS_LOG_FUNCTION (this);
  return m_alpha;
}

void
TcnQueueDisc::SetAredBeta (double beta)
{
  NS_LOG_FUNCTION (this << beta);
  m_beta = beta;

  if (m_beta < 0.83)
    {
      NS_LOG_WARN ("Beta value is below the recommended bound!");
    }
}

double
TcnQueueDisc::GetAredBeta (void)
{
  NS_LOG_FUNCTION (this);
  return m_beta;
}

void
TcnQueueDisc::SetQueueLimit (uint32_t lim)
{
  NS_LOG_FUNCTION (this << lim);
  m_queueLimit = lim;
}

void
TcnQueueDisc::SetTh (double minTh, double maxTh)
{
  NS_LOG_FUNCTION (this << minTh << maxTh);
  NS_ASSERT (minTh <= maxTh);
  m_minTh = minTh;
  m_maxTh = maxTh;
}

TcnQueueDisc::Stats
TcnQueueDisc::GetStats ()
{
  NS_LOG_FUNCTION (this);
  return m_stats;
}

int64_t
TcnQueueDisc::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_uv->SetStream (stream);
  return 1;
}

bool
TcnQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{

    NS_LOG_FUNCTION (this << item);

  uint8_t tos = StaticCast <Ipv4QueueDiscItem> (item) ->GetHeader().GetTos();
  uint32_t band = (tos & 0xff)>> 5;

  NS_LOG_LOGIC("ToS "<<(tos & 0xff));
  NS_LOG_LOGIC("band "<< band);

  if (GetInternalQueue (band)->GetNPackets () > m_queueLimit )
    {
      m_drop<<Simulator::Now()<<"\t"<<StaticCast <Ipv4QueueDiscItem> (item) ->GetHeader()<<std::endl;
      NS_LOG_LOGIC ("Queue disc limit exceeded -- dropping packet");
      Drop (item);
      return false;
    }

  TcnTag tcn;
  tcn.SetDeadline(Simulator::Now());
  item ->GetPacket()->AddPacketTag(tcn);
  // item ->GetPacket()->RemovePacketTag(tcn);
  // Time tc = tcn.GetDeadline();
  //std::cout<<"DoEnqueue"<<Simulator::Now()<<std::endl;

  // item->GetPacket()->Print(std::cout);
  // Ipv4Header ipv4header;
  // item ->GetPacket()->PeekHeader(ipv4header);
  // ipv4header.Print(std::cout);

  bool retval = GetInternalQueue (band)->Enqueue (item);

  // If Queue::Enqueue fails, QueueDisc::Drop is called by the internal queue
  // because QueueDisc::AddInternalQueue sets the drop callback

  NS_LOG_LOGIC ("DoEnqueue Number packets band " << band << ": " << GetInternalQueue (band)->GetNPackets ());

  return retval;
}


/*
 * Note: if the link bandwidth changes in the course of the
 * simulation, the bandwidth-dependent RED parameters do not change.
 * This should be fixed, but it would require some extra parameters,
 * and didn't seem worth the trouble...
 */
void
TcnQueueDisc::InitializeParams (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("Initializing TcnQueue params.");

  m_cautious = 0;
  m_ptc = m_linkBandwidth.GetBitRate () / (8.0 * m_meanPktSize);

  if (m_minTh == 0 && m_maxTh == 0)
    {
      m_minTh = 5.0;

      // set m_minTh to max(m_minTh, targetqueue/2.0) [Ref: http://www.icir.org/floyd/papers/adaptiveRed.pdf]
      double targetqueue = m_targetDelay.GetSeconds() * m_ptc;

      if (m_minTh < targetqueue / 2.0 )
        {
          m_minTh = targetqueue / 2.0;
        }
      if (GetMode () == Queue::QUEUE_MODE_BYTES)
        {
          m_minTh = m_minTh * m_meanPktSize;
        }

      // set m_maxTh to three times m_minTh [Ref: http://www.icir.org/floyd/papers/adaptiveRed.pdf]
      m_maxTh = 3 * m_minTh;
    }

  m_stats.forcedDrop = 0;
  m_stats.unforcedDrop = 0;
  m_stats.qLimDrop = 0;
  m_stats.unforcedMark = 0;

  NS_ASSERT (m_minTh <= m_maxTh);
  for (int i=0;i<4;i++){
    m_markP[i] = 0;
    m_OutQueueRate[i] = DataRate("10Gbps");
    CurrentBytes[i] = 0;
  }
  m_qAvg = 0.0;
  m_countBytes = 0;
  m_old = 0;
  m_idle = 1;
  r = 0;

  double th_diff = (m_maxTh - m_minTh);
  if (th_diff == 0)
    {
      th_diff = 1.0;
    }

  m_idleTime = NanoSeconds (0);
}

// Compute the average queue size
DataRate
TcnQueueDisc::Estimator ( DataRate OutQueueRate, uint32_t i)
{
  NS_LOG_FUNCTION (this);
  Time now = Simulator::Now();
  if ( ((now >= m_lasttime[i] + m_interval)&&m_count[i]>0) || m_count[i] > m_RoundSendCount)
  {
  //  Time tmp=m_lasttime[i] + m_interval;
  //  std::cout<<"m_lasttime[i]    "<<m_lasttime[i]<<"m_interval   "<<m_interval<<"   "<<tmp<<std::endl;
    uint64_t nBits;
    if(i==0)
    {
       nBits = OutQueueRate.GetBitRate() * (1 - w) + m_count[i]*52*8/(now.GetSeconds()-m_lasttime[i].GetSeconds()) * w;
    }
    else
    {
       nBits = OutQueueRate.GetBitRate() * (1 - w) + m_count[i]*1010*8/(now.GetSeconds()-m_lasttime[i].GetSeconds()) * w;
    }
    // uint64_t nBits = OutQueueRate.GetBitRate() * (1 - w) + m_count[i]*1010*8/(now.GetSeconds()-m_lasttime[i].GetSeconds()) * w;
    DataRate QueueRate = DataRate(nBits);
  //  std::cout<<OutQueueRate.GetBitRate()<<"          "<< m_count[i]*1000*8/(now.GetSeconds()-m_lasttime[i].GetSeconds())<<std::endl;
    // NS_LOG_LOGIC("Count "<< m_count[i]);
    // NS_LOG_LOGIC("Round "<< m_RoundSendCount );
    // NS_LOG_LOGIC("OutQueueRate " << OutQueueRate);
    // NS_LOG_LOGIC("Current " << DataRate(m_count[i]*1000*8/(now.GetSeconds()-m_lastSet.GetSeconds())));
    // NS_LOG_LOGIC("QueueRate " << QueueRate);
    m_lastSet = now;
    m_lasttime[i] = now;
    m_count[i] = 0;
    return QueueRate;
  }
  else
  {
    m_count[i] ++;
    NS_LOG_LOGIC("Count "<< m_count[i]);
    NS_LOG_LOGIC("OutQueueRate " << OutQueueRate);
    return OutQueueRate;
  }

}

Ptr<QueueDiscItem>
TcnQueueDisc::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);
  Ptr<QueueDiscItem> item;


  // TcnTag tcn;
  // // // tcn.SetDeadline(Simulator::Now());
  // item ->GetPacket()->RemovePacketTag(tcn);
  // // // item ->GetPacket()->RemovePacketTag(tcn);
  //  Time tc = tcn.GetDeadline();
  // // std::cout<<"DoDequeue"<<tc.GetMicroSeconds()<<std::endl;
  for (uint32_t i = 0; i < GetNInternalQueues (); i++)
  {
    if ((item = StaticCast<QueueDiscItem> (GetInternalQueue (r)->Dequeue ())) != 0)
    {
        TcnTag timestamp;
        // // tcn.SetDeadline(Simulator::Now());
        item ->GetPacket()->RemovePacketTag(timestamp);
        // // item ->GetPacket()->RemovePacketTag(tcn);
        Time tc = timestamp.GetDeadline();
        //std::cout<<"(Simulator::Now()-tc)-m_MaxDelay "<< (Simulator::Now()-tc)-m_MaxDelay<<std::endl;
        if((Simulator::Now()-tc-m_MaxDelay)>0)
        {
          item->Mark ();
          //std::cout<<"Queue "<< r <<" Marked ECN!"<<std::endl;
        }
        //std::cout<<"tc-Simulator::Now()   "<<Simulator::Now()-tc<<std::endl;

      // NS_LOG_LOGIC ("Popped from band " << r << ": " << item);
      // NS_LOG_LOGIC ("DoDequeue Number packets band " << r << ": " << GetInternalQueue (r)->GetNPackets ());
      DataRate tmp = Estimator ( m_OutQueueRate[r], r);
      m_OutQueueRate[r] = tmp;
      // std::cout<<Simulator::Now()<<"           OutQueueRate["<< r <<"]         " << m_OutQueueRate[r]<<std::endl;
      if(m_markP[r] > 0 && (item ->GetPacketSize()) > 52 )
      {
        
        m_markP[r] -- ;
        m_stats.unforcedMark++;
        // item->Print (std::cout);
        // std::cout<<std::endl;
        NS_LOG_INFO ("Queue "<< r <<" Marked ECN!");
      }
      DWRR_R[r] --;
      if(DWRR_R[r] == 0)
      {
        DWRR_R[r]=DWRR[r];
        r ++;
        if(r == 4)
        {
          r = 0;
        }
      }
      return item;
    }

    DWRR_R[r]=DWRR[r];
    r ++;
    if(r == 4)
    {
      r = 0;
    }
  }
  NS_LOG_LOGIC ("Queue empty");
  return item;
}

Ptr<const QueueDiscItem>
TcnQueueDisc::DoPeek (void) const
{
NS_LOG_FUNCTION (this);

  Ptr<const QueueDiscItem> item;

  for (uint32_t i = 0; i < GetNInternalQueues (); i++)
    {
      item = StaticCast<const QueueDiscItem> (GetInternalQueue (i)->Peek ());
      NS_LOG_LOGIC ("Peeked from band " << i << ": " << item);
      NS_LOG_LOGIC ("DoPeek Number packets band " << i << ": " << GetInternalQueue (i)->GetNPackets ());
      return item;
    }

  NS_LOG_LOGIC ("Queue empty");
  return item;
}

bool
TcnQueueDisc::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);
  //std::cout<<Simulator::Now ().GetSeconds ()<<"***"<<"0------"<<GetNInternalQueues ()<<std::endl;
  if (GetNQueueDiscClasses () > 0)
    {
      NS_LOG_ERROR ("TcnQueueDisc cannot have classes");
      return false;
    }

  if (GetNPacketFilters () > 0)
    {
      NS_LOG_ERROR ("TcnQueueDisc cannot have packet filters");
      return false;
    }
  if (GetNInternalQueues () == 0)
    {
      // create 4 DropTail queues with m_queueLimit packets each
      ObjectFactory factory;
      factory.SetTypeId ("ns3::DropTailQueue");
      factory.Set ("Mode", EnumValue (Queue::QUEUE_MODE_PACKETS));
      factory.Set ("MaxPackets", UintegerValue (m_queueLimit));
      AddInternalQueue (factory.Create<Queue> ());
      AddInternalQueue (factory.Create<Queue> ());
      AddInternalQueue (factory.Create<Queue> ());
      AddInternalQueue (factory.Create<Queue> ());
    }
  //std::cout<<Simulator::Now ().GetSeconds ()<<"***"<<"1----"<<GetNInternalQueues ()<<std::endl;

  if (GetNInternalQueues () != 4)
    {
      NS_LOG_ERROR ("PfifoFastQueueDisc needs 4 internal queues");
      return false;
    }

  if (GetInternalQueue (0)-> GetMode () != Queue::QUEUE_MODE_PACKETS ||
      GetInternalQueue (1)-> GetMode () != Queue::QUEUE_MODE_PACKETS ||
      GetInternalQueue (2)-> GetMode () != Queue::QUEUE_MODE_PACKETS ||
      GetInternalQueue (3)-> GetMode () != Queue::QUEUE_MODE_PACKETS)
    {
      NS_LOG_ERROR ("PfifoFastQueueDisc needs 4 internal queues operating in packet mode");
      return false;
    }

  if (GetInternalQueue (0)->GetMode () != m_mode)
    {
      NS_LOG_ERROR ("The mode of the provided queue does not match the mode set on the TcnQueueDisc");
      return false;
    }

  if ((m_mode ==  Queue::QUEUE_MODE_PACKETS && GetInternalQueue (0)->GetMaxPackets () < m_queueLimit) ||
      (m_mode ==  Queue::QUEUE_MODE_BYTES && GetInternalQueue (0)->GetMaxBytes () < m_queueLimit))
    {
      NS_LOG_ERROR ("The size of the internal queue is less than the queue disc limit");
      return false;
    }

  return true;
}

uint32_t
TcnQueueDisc::GetQueueSize (void)
{
  NS_LOG_FUNCTION (this);
  if (GetMode () == Queue::QUEUE_MODE_BYTES)
    {
      return GetInternalQueue (0)->GetNBytes ();
    }
  else if (GetMode () == Queue::QUEUE_MODE_PACKETS)
    {
      return GetInternalQueue (0)->GetNPackets ();
    }
  else
    {
      NS_ABORT_MSG ("Unknown RED mode.");
    }
}

} // namespace ns3
