/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007, 2014 University of Washington
 *               2015 Universita' degli Studi di Napoli Federico II
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors:  Stefano Avallone <stavallo@unina.it>
 *           Tom Henderson <tomhend@u.washington.edu>
 */

#ifndef TCN_QUEUE_DISC_H
#define TCN_QUEUE_DISC_H

#include "ns3/packet.h"
#include "ns3/queue-disc.h"
#include "ns3/nstime.h"
#include "ns3/boolean.h"
#include "ns3/data-rate.h"
#include "ns3/nstime.h"
#include "ns3/random-variable-stream.h"

namespace ns3 {

class TraceContainer;

/**
 * \ingroup traffic-control
 *
 * \brief A RED packet queue disc
 */
class TcnQueueDisc : public QueueDisc
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \brief TcnQueueDisc Constructor
   *
   * Create a Tcn RED queue disc
   */
  TcnQueueDisc ();

  /**
   * \brief Destructor
   *
   * Destructor
   */ 
  virtual ~TcnQueueDisc ();

  /**
   * \brief Stats
   */
  typedef struct
  {   
    uint32_t unforcedDrop;  //!< Early probability drops
    uint32_t forcedDrop;    //!< Forced drops, qavg > max threshold
    uint32_t qLimDrop;      //!< Drops due to queue limits
    uint32_t unforcedMark;  //!< Early probability marks
  } Stats;

  /**
   * \brief Drop types
   */
  enum
  {
    DTYPE_NONE,        //!< Ok, no drop
    DTYPE_FORCED,      //!< A "forced" drop
    DTYPE_UNFORCED,    //!< An "unforced" (random) drop
  };

  /**
   * \brief Set the operating mode of this queue.
   *  Set operating mode
   *
   * \param mode The operating mode of this queue.
   */
  void SetMode (Queue::QueueMode mode);

  /**
   * \brief Get the encapsulation mode of this queue.
   * Get the encapsulation mode of this queue
   *
   * \returns The encapsulation mode of this queue.
   */
  Queue::QueueMode GetMode (void);

  /**
   * \brief Get the current value of the queue in bytes or packets.
   *
   * \returns The queue size in bytes or packets.
   */
  uint32_t GetQueueSize (void);

   /**
    * \brief Set the alpha value to adapt m_curMaxP.
    *
    * \param alpha The value of alpha to adapt m_curMaxP.
    */
   void SetAredAlpha (double alpha);

   /**
    * \brief Get the alpha value to adapt m_curMaxP.
    *
    * \returns The alpha value to adapt m_curMaxP.
    */
   double GetAredAlpha (void);

   /**
    * \brief Set the beta value to adapt m_curMaxP.
    *
    * \param beta The value of beta to adapt m_curMaxP.
    */
   void SetAredBeta (double beta);

   /**
    * \brief Get the beta value to adapt m_curMaxP.
    *
    * \returns The beta value to adapt m_curMaxP.
    */
   double GetAredBeta (void);

  /**
   * \brief Set the limit of the queue.
   *
   * \param lim The limit in bytes or packets.
   */
  void SetQueueLimit (uint32_t lim);

  /**
   * \brief Set the thresh limits of RED.
   *
   * \param minTh Minimum thresh in bytes or packets.
   * \param maxTh Maximum thresh in bytes or packets.
   */
  void SetTh (double minTh, double maxTh);

  /**
   * \brief Get the RED statistics after running.
   *
   * \returns The drop statistics.
   */
  Stats GetStats ();

 /**
  * Assign a fixed random variable stream number to the random variables
  * used by this model.  Return the number of streams (possibly zero) that
  * have been assigned.
  *
  * \param stream first stream index to use
  * \return the number of stream indices assigned by this model
  */
  int64_t AssignStreams (int64_t stream);

protected:
  /**
   * \brief Dispose of the object
   */
  virtual void DoDispose (void);

private:
  virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
  virtual Ptr<QueueDiscItem> DoDequeue (void);
  virtual Ptr<const QueueDiscItem> DoPeek (void) const;
  virtual bool CheckConfig (void);
  /**
   * \brief Initialize the queue parameters.
   *
   * Note: if the link bandwidth changes in the course of the
   * simulation, the bandwidth-dependent RED parameters do not change.
   * This should be fixed, but it would require some extra parameters,
   * and didn't seem worth the trouble...
   */
  virtual void InitializeParams (void);
  /**
   * \brief Compute the average queue size
   * \param nQueued number of queued packets
   * \param m simulated number of packets arrival during idle period
   * \param qAvg average queue size
   * \param qW queue weight given to cur q size sample
   * \returns new average queue size
   */
  DataRate Estimator (DataRate OutQueueRate, uint32_t i);
   /**
    * \brief Update m_curMaxP
    * \param newAve new average queue length
    * \param now Current Time
    */

  Stats m_stats; //!< RED statistics

  //**TcnQueuedisc
  DataRate m_OutQueueRate[4]; //!< The rate of every Internal Queue Out
  double w = 0.8;                 //!< The Weight of OutQueueRate update weight 
  uint32_t m_count[4] = {0,0,0,0};      //!< Number of packets since last random number generation
  uint32_t m_RoundSendCount;//!< Every Queue send packet count arrive the number then update the  m_OutQueueRate
  Time m_Delay;             //!< queuing delay 
  Time m_Delay_Old[4];      //!< old queuing delay
  Time m_Delay_Diff[4];     //!< queuing delay diff 
  Time m_MaxDelay;          //!< max  queuing delay 
  Time m_MinDelay;          //!< less than the DelayPacket Number then not mark
  uint32_t m_markP[4] = {0,0,0,0};             //!< Ture for mark ecn
  uint32_t DWRR[4] = {2,1,1,1};
  uint32_t DWRR_R[4] = {2,1,1,1};
  uint32_t r;               //round DWRR 
  uint32_t CurrentBytes[4] = {0,0,0,0}; 
  double CountBytes[4] = {0,0,0,0};
  Time  m_lasttime[4]= {Seconds(0.0),Seconds(0.0),Seconds(0.0),Seconds(0.0)};

  // ** Variables supplied by user
  Queue::QueueMode m_mode;  //!< Mode (Bytes or packets)
  uint32_t m_meanPktSize;   //!< Avg pkt size
  uint32_t m_idlePktSize;   //!< Avg pkt size used during idle times
  bool m_isWait;            //!< True for waiting between dropped packets
  bool m_isGentle;          //!< True to increases dropping prob. slowly when ave queue exceeds maxthresh
  bool m_isARED;            //!< True to enable Adaptive RED
  bool m_isAdaptMaxP;       //!< True to adapt m_curMaxP
  double m_minTh;           //!< Min avg length threshold (bytes)
  double m_maxTh;           //!< Max avg length threshold (bytes), should be >= 2*minTh
  uint32_t m_queueLimit;    //!< Queue limit in bytes / packets
  double m_lInterm;         //!< The max probability of dropping a packet
  Time m_targetDelay;       //!< Target average queuing delay in ARED
  Time m_interval;          //!< Time interval to update m_curMaxP
  double m_alpha;           //!< Increment parameter for m_curMaxP in ARED
  double m_beta;            //!< Decrement parameter for m_curMaxP in ARED
  Time m_rtt;               //!< Rtt to be considered while automatically setting m_bottom in ARED
  bool m_isNs1Compat;       //!< Ns-1 compatibility
  DataRate m_linkBandwidth; //!< Link bandwidth
  Time m_linkDelay;         //!< Link delay
  bool m_useEcn;            //!< True for enabling red-queue-disc to use ECN
  bool m_useMarkP;          //!< For deciding when to drop
 

  // ** Variables maintained by RED
  // double m_curMaxP;         //!< Current max_p
  Time m_lastSet;           //!< Last time m_curMaxP was updated
  // double m_vProb;           //!< Prob. of packet drop
  uint32_t m_countBytes;    //!< Number of bytes since last drop
  uint32_t m_old;           //!< 0 when average queue first exceeds threshold
  uint32_t m_idle;          //!< 0/1 idle status
  double m_ptc;             //!< packet time constant in packets/second
  double m_qAvg;         //!< Average queue length
  
  /**
   * 0 for default RED
   * 1 experimental (see red-queue.cc)
   * 2 experimental (see red-queue.cc)
   * 3 use Idle packet size in the ptc
   */
  uint32_t m_cautious;
  Time m_idleTime;          //!< Start of current idle period

  Ptr<UniformRandomVariable> m_uv;  //!< rng stream
};

}; // namespace ns3

#endif // RED_QUEUE_DISC_H