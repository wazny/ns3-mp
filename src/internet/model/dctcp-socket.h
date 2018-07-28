#ifndef DCTCP_SOCKET_H
#define DCTCP_SOCKET_H

#include "tcp-socket-base.h"
#include "tcp-congestion-ops.h"
#include "tcp-l4-protocol.h"

namespace ns3 {

class DctcpSocket : public TcpSocketBase
{
public:
  /**
   * Get the type ID.
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Get the instance TypeId
   * \return the instance TypeId
   */
  virtual TypeId GetInstanceTypeId () const;

  /**
   * Create an unbound TCP socket
   */
  DctcpSocket (void);

  /**
   * Clone a TCP socket, for use upon receiving a connection request in LISTEN state
   *
   * \param sock the original Tcp Socket
   */
  DctcpSocket (const DctcpSocket& sock);

protected:

  // inherited from TcpSocketBase
  virtual void SendACK (void);
  // virtual void SendACK (uint8_t prio);
  virtual void EstimateRtt (const TcpHeader& tcpHeader);
  virtual void UpdateRttHistory (const SequenceNumber32 &seq, uint32_t sz,
                                 bool isRetransmission);
  virtual void DoRetransmit (void);
  virtual Ptr<TcpSocketBase> Fork (void);
  virtual void UpdateEcnState (const TcpHeader &tcpHeader);
  virtual void DecreaseWindow (void);
  virtual bool MarkEmptyPacket (void) const;

  void UpdateAlpha (const TcpHeader &tcpHeader);

  // DCTCP related params
  uint32_t ecnCount = 0;
  Time m_lasttime = Seconds(0.0);
  Time m_interval = Seconds(0.00005);
  double m_g;   //!< dctcp g param
  TracedValue<double> m_alpha;   //!< dctcp alpha param
  uint32_t m_ackedBytesEcn; //!< acked bytes with ecn
  uint32_t m_ackedBytesTotal;   //!< acked bytes total
  SequenceNumber32 m_alphaUpdateSeq;
  SequenceNumber32 m_dctcpMaxSeq;
  bool m_ecnTransition; //!< ce state machine to support delayed ACK
  bool m_ecnLimit;

};

} // namespace ns3

#endif // DCTCP_SOCKET_H
