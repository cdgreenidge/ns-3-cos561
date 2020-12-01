#ifndef TCPLEARNING_H
#define TCPLEARNING_H

#include "tcp-congestion-ops.h"

namespace ns3 {

class TcpSocketState;

class TcpLearning : public TcpNewReno
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * Create an unbound tcp socket.
   */
  TcpLearning (void);

  /**
   * \brief Copy constructor
   * \param sock the object to copy
   */
  TcpLearning (const TcpLearning &sock);
  virtual ~TcpLearning (void);

  virtual std::string GetName () const;

  // /**
  //  * \brief Get slow start threshold after congestion event
  //  *
  //  * \param tcb internal congestion state
  //  * \param bytesInFlight bytes in flight
  //  *
  //  * \return the slow start threshold value
  //  */
  // virtual uint32_t GetSsThresh (Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight);

  // /**
  //  * \brief Adjust cwnd following Illinois congestion avoidance algorithm
  //  *
  //  * \param tcb internal congestion state
  //  * \param segmentsAcked count of segments ACKed
  //  */
  // virtual void IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);

  /**
   * \brief Measure RTT for each ACK
   * Keep track of min and max RTT
   *
   * \param tcb internal congestion state
   * \param segmentsAcked count of segments ACKed
   * \param rtt last RTT
   */
  virtual void PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time &rtt);

  virtual Ptr<TcpCongestionOps> Fork ();
};

} // namespace ns3

#endif // TCPLEARNING_H
