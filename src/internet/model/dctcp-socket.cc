#include "dctcp-socket.h"

#include "ns3/log.h"
#include "ns3/node.h"
namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("DctcpSocket");

NS_OBJECT_ENSURE_REGISTERED (DctcpSocket);

TypeId
DctcpSocket::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::DctcpSocket")
      .SetParent<TcpSocketBase> ()
      .SetGroupName ("Internet")
      .AddConstructor<DctcpSocket> ()
      .AddAttribute ("DctcpWeight",
                     "Weigt for calculating DCTCP's alpha parameter",
                     DoubleValue (1.0 / 16.0),
                     MakeDoubleAccessor (&DctcpSocket::m_g),
                     MakeDoubleChecker<double> (0.0, 1.0))
      .AddAttribute ("ECNLimit",
                     "True to enable ECN limit",
                     BooleanValue (false),
                     MakeBooleanAccessor (&DctcpSocket::m_ecnLimit),
                     MakeBooleanChecker ())
      .AddTraceSource ("DctcpAlpha",
                       "Alpha parameter stands for the congestion status",
                       MakeTraceSourceAccessor (&DctcpSocket::m_alpha),
                       "ns3::TracedValueCallback::Double");
  return tid;
}

TypeId
DctcpSocket::GetInstanceTypeId () const
{
  return DctcpSocket::GetTypeId ();
}

DctcpSocket::DctcpSocket (void)
  : TcpSocketBase (),
    m_g (1.0 / 16.0),
    m_alpha (1.0),
    m_ackedBytesEcn (0),
    m_ackedBytesTotal (0),
    m_alphaUpdateSeq (0),
    m_dctcpMaxSeq (0),
    m_ecnTransition (false)
{
}

DctcpSocket::DctcpSocket (const DctcpSocket &sock)
  : TcpSocketBase (sock),
    m_g (sock.m_g),
    m_alpha (sock.m_alpha),
    m_ackedBytesEcn (sock.m_ackedBytesEcn),
    m_ackedBytesTotal (sock.m_ackedBytesTotal),
    m_alphaUpdateSeq (sock.m_alphaUpdateSeq),
    m_dctcpMaxSeq (sock.m_dctcpMaxSeq),
    m_ecnTransition (sock.m_ecnTransition)
{
}

void
DctcpSocket::SendACK (void)
{
  NS_LOG_FUNCTION (this);
  // std::cout<<"DctcpSocket::SendACK"<<std::endl;
  uint8_t flag = TcpHeader::ACK;
  Time now = Simulator::Now();
  //std::cout<<"SendACK  "<<TcpHeader::ACK<<std::endl;
  if (m_ecnState & ECN_CONN)
    {
      if (m_ecnTransition)
        {
          if (!(m_ecnState & ECN_TX_ECHO))
            {
              NS_LOG_DEBUG ("Sending ECN Echo.");
              //std::cout<<"Sending ECN Echo.";
              if(!m_ecnLimit || now >= m_lasttime + m_interval)
                {
                  flag |= TcpHeader::ECE;
                  m_lasttime = now ;
                //  std::cout<<"now >= m_lasttime + m_interval";
                }
              
            }
          m_ecnTransition = false;
        }
      else if (m_ecnState & ECN_TX_ECHO)
        {
          NS_LOG_DEBUG ("Sending ECN Echo.");
          if(!m_ecnLimit || now >= m_lasttime + m_interval)
            {
              flag |= TcpHeader::ECE;
              m_lasttime = now ;
            }
        }
    }
  SendEmptyPacket (flag);

}

// void
// DctcpSocket::SendACK (uint8_t prio)
// {
//   NS_LOG_FUNCTION (this);
//   std::cout<<"DctcpSocket::SendACK   "<< (uint32_t)prio <<std::endl;
//   uint8_t flag = TcpHeader::ACK;
//   //std::cout<<"DctcpSocket::SendACK   "<< (uint32_t)flag <<std::endl;
//   //std::cout<<"SendACK  "<<TcpHeader::ACK<<std::endl;
//   if (m_ecnState & ECN_CONN)
//     {
//       if (m_ecnTransition)
//         {
//           if (!(m_ecnState & ECN_TX_ECHO))
//             {
//               NS_LOG_DEBUG ("Sending ECN Echo.");
//               flag |= TcpHeader::ECE;
//             }
//           m_ecnTransition = false;
//         }
//       else if (m_ecnState & ECN_TX_ECHO)
//         {
//           NS_LOG_DEBUG ("Sending ECN Echo.");
//           flag |= TcpHeader::ECE;
//         }
//     }
//   flag |=(prio & 0B11100000);
//   std::cout<<"DctcpSocket::SendACK   "<< (uint32_t)flag <<std::endl;
//   SendEmptyPacket (flag);
// }
void
DctcpSocket::EstimateRtt (const TcpHeader &tcpHeader)
{
  if (!(tcpHeader.GetFlags () & TcpHeader::SYN))
    {
      UpdateAlpha (tcpHeader);
    }
  TcpSocketBase::EstimateRtt (tcpHeader);
}

void
DctcpSocket::UpdateRttHistory (const SequenceNumber32 &seq, uint32_t sz,
                               bool isRetransmission)
{
  // set dctcp max seq to highTxMark
  m_dctcpMaxSeq =std::max (std::max (seq + sz, m_tcb->m_highTxMark.Get ()), m_dctcpMaxSeq);
  TcpSocketBase::UpdateRttHistory (seq, sz, isRetransmission);
}

void
DctcpSocket::UpdateAlpha (const TcpHeader &tcpHeader)
{
  NS_LOG_FUNCTION (this);
  int32_t ackedBytes = tcpHeader.GetAckNumber () - m_highRxAckMark.Get ();
  if (ackedBytes > 0)
    {
      m_ackedBytesTotal += ackedBytes;
      if (tcpHeader.GetFlags () & TcpHeader::ECE)
        {
          m_ackedBytesEcn += ackedBytes;
        }
    }
  /*
   * check for barrier indicating its time to recalculate alpha.
   * this code basically updated alpha roughly once per RTT.
   */
  if (tcpHeader.GetAckNumber () > m_alphaUpdateSeq)
    {
      m_alphaUpdateSeq = m_dctcpMaxSeq;
      // NS_LOG_DEBUG ("Before alpha update: " << m_alpha.Get ());
      m_alpha = (1 - m_g) * m_alpha + m_g * ((double)m_ackedBytesEcn / (m_ackedBytesTotal ? m_ackedBytesTotal : 1));
      // NS_LOG_DEBUG ("After alpha update: " << m_alpha.Get ());
      NS_LOG_DEBUG ("[ALPHA] " << Simulator::Now ().GetSeconds () << " " << m_alpha.Get ());
      m_ackedBytesEcn = m_ackedBytesTotal = 0;
    }
}

void
DctcpSocket::DoRetransmit (void)
{
  NS_LOG_FUNCTION (this);
  // reset dctcp seq value to  if retransmit (why?)
  m_alphaUpdateSeq = m_dctcpMaxSeq = m_tcb->m_nextTxSequence;
  TcpSocketBase::DoRetransmit ();
}

Ptr<TcpSocketBase>
DctcpSocket::Fork (void)
{
  return CopyObject<DctcpSocket> (this);
}

void
DctcpSocket::UpdateEcnState (const TcpHeader &tcpHeader)
{
  NS_LOG_FUNCTION (this << tcpHeader);

  if (m_ceReceived && !(m_ecnState & ECN_TX_ECHO))
    {
      NS_LOG_INFO ("Congestion was experienced. Start sending ECN Echo.");
      m_ecnState |= ECN_TX_ECHO;
      m_ecnTransition = true;
      m_delAckCount = m_delAckMaxCount;
    }
  else if (!m_ceReceived && (m_ecnState & ECN_TX_ECHO))
    {
      m_ecnState &= ~ECN_TX_ECHO;
      m_ecnTransition = true;
      m_delAckCount = m_delAckMaxCount;
    }
}

void
DctcpSocket::DecreaseWindow (void)
{
  NS_LOG_FUNCTION (this);
  // halve cwnd according to DCTCP algo

  uint32_t newCwnd = (1 - m_alpha / 2.0) * Window ();
  m_tcb->m_ssThresh = std::max (newCwnd, 2 * GetSegSize ());
  m_tcb->m_cWnd = std::max (newCwnd, GetSegSize ());
  // Address address;
  // GetSockName(address);
  // InetSocketAddress inetaddress = InetSocketAddress::ConvertFrom(address);
  // std::cout<<" [node " << m_node->GetId ()<<" "<< inetaddress.GetIpv4()<<":"<< inetaddress.GetPort()<< "]DctcpSocket::DecreaseWindow         "<<m_tcb->m_cWnd <<std::endl;
}

bool
DctcpSocket::MarkEmptyPacket (void) const
{
  NS_LOG_FUNCTION (this);
  // mark empty packet if we use DCTCP && ECN is enabled
  return m_ecn;
}

} // namespace ns3
