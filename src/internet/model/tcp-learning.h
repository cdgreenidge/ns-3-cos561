#ifndef TCPLEARNING_H
#define TCPLEARNING_H

#include "tcp-congestion-ops.h"
#include <array>
#include <random>
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

class FuzzyKanerva
{
public:
  const int m_seed;

  // State discretization parameters. All times in ms.
  const int m_numIntervals;
  const double m_ackTimeLower;
  const double m_ackTimeUpper;
  const double m_packetTimeLower;
  const double m_packetTimeUpper;
  const double m_rttRatioLower;
  const double m_rttRatioUpper;
  const double m_ssThreshLower;
  const double m_ssThreshUpper;

  // Fuzzy Kanerva learning parameters
  const int m_numActions;
  const int m_numPrototypes;
  const int m_stateDim;
  const double m_delta; // Weight factor for utility
  const double m_t; // Reward time step, in ms, not in s as in the paper
  const double m_sigma; // Membership grade standard deviation
  const double m_gamma; // Discount factor, in [0, 1]
  const double m_epsilon; // Exploration rate
  const double m_alpha; // Learning rate
  const double m_alphaDecayFactor; // Learning rate decay factor
  const double m_alphaDecayInterval; // Learning rate decay interval, in ms
  const std::vector<int> m_actionToCwndChange; // Table II in paper

  FuzzyKanerva ();
  // Returns action to apply to cwnd
  int Main (double time, double ackTime, double packetTime, double rttRatio, double ssThresh,
            double throughput, double delay);
  int Learn ();
  std::vector<int> GetCurrentState ();
  double GetMembershipGrade (const std::vector<int> &state0, int action0,
                             const std::vector<int> &state1, int action1);
  void UpdateState (double time, double ackTime, double packetTime, double rttRatio,
                    double ssThresh, double throughput, double delay);
  void UpdateReward ();

private:
  // Note: the "prev" variables are only updated when the Learn() method is called,
  // the current state, utility, and time get updated on every ACK
  std::mt19937 m_rng;
  double m_prevTime; // In ms
  double m_currentTime; // In ms
  std::vector<int> m_prevState;
  std::vector<int> m_currentState;
  double m_prevUtility; // Only updated when the reward updates
  double m_currentUtility;
  int m_prevAction;
  int m_reward;
  std::vector<std::vector<int>> m_prototypeStates;
  std::vector<int> m_prototypeActions;
  std::vector<double> m_thetas;
  std::uniform_int_distribution<int> m_actionDist; // Random distribution over actions
};

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
  // State helpers
  int m_movingAverageWindowSize;
  RunningDifference m_ackTimeDiff;
  RunningDifference m_packetTimeDiff;
  MovingAvg m_ackTimeAvg;
  MovingAvg m_packetTimeAvg;
  CurrentBestRatio m_rttRatio;
  double m_ssThresh;

  // Learning agent
  FuzzyKanerva m_agent;
};

} // namespace ns3

#endif // TCPLEARNING_H
