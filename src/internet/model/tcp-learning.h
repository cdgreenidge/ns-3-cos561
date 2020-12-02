#ifndef TCPLEARNING_H
#define TCPLEARNING_H

#include "tcp-congestion-ops.h"
#include <vector>

namespace ns3 {

class RunningDifference
{
public:
  RunningDifference ();
  double RecordAndCalculate (double item);

private:
  double m_old;
};

class MovingAvg
{
public:
  MovingAvg (int size);
  void Record (double item);
  double Avg ();

private:
  bool m_underfilled;
  int m_index; // Next position to write
  int m_maxSize;
  std::vector<double> m_buffer;
};

class CurrentBestRatio
{
public:
  CurrentBestRatio ();
  void Record (double item);
  double Ratio ();

private:
  double m_best;
  double m_current;
};

int discretize (double lower, double upper, int numSteps, double item);

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

private:
  // State discretization parameters. All times in seconds.
  // int m_numIntervals;
  // int m_movingAverageWindowSize;
  // double m_ackInterarrivalLower;
  // double m_ackInterarrivalUpper;
  // double m_packetInterarrivalLower;
  // double m_packetInterarrivalUpper;
  // double m_rttRatioLower;
  // double m_rttRatioUpper;
  // double m_ssthreshLower;
  // double m_ssthreshUpper;
};

} // namespace ns3

#endif // TCPLEARNING_H
