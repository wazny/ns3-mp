#include "tcn-tag.h"

#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcnTag");


NS_OBJECT_ENSURE_REGISTERED (TcnTag);

TypeId
TcnTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcnTag")
    .SetParent<Tag> ()
    .SetGroupName("Internet")
    .AddConstructor<TcnTag> ()
  ;
  return tid;
}

TcnTag::TcnTag ()
  : Tag (),
    m_tenantId (0),
    m_flowSize (0),
    m_packetSize (0),
    m_deadline ()
{
  NS_LOG_FUNCTION_NOARGS ();
}

TcnTag::~TcnTag ()
{
  NS_LOG_FUNCTION_NOARGS ();
}

TypeId
TcnTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t
TcnTag::GetSerializedSize (void) const
{
  NS_LOG_FUNCTION (this);
  ///\todo update size when add new member
  return 
      sizeof (m_tenantId)
      + sizeof (m_flowSize)
      + sizeof (m_packetSize)
      + sizeof (double);
}

void
TcnTag::Serialize (TagBuffer buf) const
{
  NS_LOG_FUNCTION (this << &buf);
  buf.WriteU32 (m_tenantId);
  buf.WriteU32 (m_flowSize);
  buf.WriteU32 (m_packetSize);
  buf.WriteDouble (m_deadline.GetSeconds ());  ///time resolution in second
}

void
TcnTag::Deserialize (TagBuffer buf)
{
  NS_LOG_FUNCTION (this << &buf);
  m_tenantId = buf.ReadU32 ();
  m_flowSize = buf.ReadU32 ();
  m_packetSize = buf.ReadU32 ();
  m_deadline = Time::FromDouble (buf.ReadDouble (), Time::S);
}

void
TcnTag::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << "Tcn INFO [FlowSize: " << m_flowSize;
  os << ", PacketSize: " << m_packetSize;
  os << ", Deadline: " << m_deadline;
  os << "] ";
}



void
TcnTag::SetTenantId (uint32_t tenantId)
{
  NS_LOG_FUNCTION (this << tenantId);
  m_tenantId = tenantId;
}

uint32_t
TcnTag::GetTenantId (void) const
{
  return m_tenantId;
}

void
TcnTag::SetFlowSize (uint32_t flowSize)
{
  NS_LOG_FUNCTION (this << flowSize);
  m_flowSize = flowSize;
}

uint32_t
TcnTag::GetFlowSize (void) const
{
  return m_flowSize;
}

void
TcnTag::SetPacketSize (uint32_t packetSize)
{
  NS_LOG_FUNCTION (this << packetSize);
  m_packetSize = packetSize;
}

uint32_t
TcnTag::GetPacketSize (void) const
{
  return m_packetSize;
}

void
TcnTag::SetDeadline (Time deadline)
{
  NS_LOG_FUNCTION (this << deadline);
  m_deadline = deadline;
}

Time
TcnTag::GetDeadline (void) const
{
  return m_deadline;
}

bool
TcnTag::operator == (const TcnTag &other) const
{
  return 
       m_tenantId == other.m_tenantId
      && m_flowSize == other.m_flowSize
      && m_packetSize == other.m_packetSize
      && m_deadline == other.m_deadline;
}

bool
TcnTag::operator != (const TcnTag &other) const
{
  return !operator == (other);
}

} //namespace ns3