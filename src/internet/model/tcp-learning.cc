#include <assert.h>
#include <cmath>
#include "ns3/simulator.h"
#include "tcp-learning.h"
#include "tcp-socket-state.h"

#include "ns3/log.h"

namespace ns3 {

RunningDifference::RunningDifference () : m_old (0.0)
{
}

double
RunningDifference::RecordAndCalculate (double item)
{
  double difference = item - m_old;
  m_old = item;
  return difference;
}

MovingAvg::MovingAvg (int size)
    : m_underfilled (true), m_index (0), m_maxSize (size), m_buffer (std::vector<double> ())
{
  m_buffer.resize (m_maxSize);
}

void
MovingAvg::Record (double item)
{
  m_buffer[m_index] = item;
  int nextIndex = (m_index + 1) % m_maxSize;
  if (nextIndex == 0 && m_underfilled)
    {
      m_underfilled = false;
    }
  m_index = nextIndex;
}

double
MovingAvg::Avg ()
{
  if (m_underfilled && m_index == 0)
    {
      return 0.0;
    }
  int size;
  if (m_underfilled)
    {
      size = m_index;
    }
  else
    {
      size = m_maxSize;
    }

  double sum = 0.0;
  for (auto value : m_buffer)
    {
      sum += value;
    }
  return sum / size;
}

CurrentBestRatio::CurrentBestRatio ()
    : m_best (std::numeric_limits<double>::max ()), m_current (std::numeric_limits<double>::max ())
{
}

void
CurrentBestRatio::Record (double item)
{
  if (item < m_best)
    {
      m_best = item;
    }
  m_current = item;
}

double
CurrentBestRatio::Ratio ()
{
  return m_current / m_best;
}

int
discretize (double lower, double upper, int numSteps, double item)
{
  assert (upper >= lower);
  double zeroToOne = (item - lower) / (upper - lower);
  // Set upper bound to the first float smaller than 1.0 so we don't get
  // numsteps + 1 bins when we floor
  double upperBound = std::nextafter (1.0, 0.0);
  if (zeroToOne < 0.0)
    {
      zeroToOne = 0.0;
    }
  else if (zeroToOne > upperBound)
    {
      zeroToOne = upperBound;
    }
  return (int) (10 * zeroToOne);
}

FuzzyKanerva::FuzzyKanerva ()
    : m_seed (42),
      m_numIntervals (10),
      m_ackTimeLower (0.0),
      m_ackTimeUpper (10.0),
      m_packetTimeLower (0.0),
      m_packetTimeUpper (10.0),
      m_rttRatioLower (1.0),
      m_rttRatioUpper (5.0),
      m_ssThreshLower (1000.0),
      m_ssThreshUpper (10000000000.0),
      m_numActions (5),
      m_numPrototypes (10),
      m_stateDim (4),
      m_delta (1.0),
      m_t (100),
      m_sigma (1.0),
      m_gamma (0.9),
      m_epsilon (0.1),
      m_alpha (0.3),
      m_alphaDecayFactor (0.995),
      m_alphaDecayInterval (10000),
      m_actionToCwndChange{-1, 0, 5, 10, 20},
      m_rng (m_seed),
      m_prevTime (0.0),
      m_currentTime (0.0),
      m_prevState (m_stateDim, 0),
      m_currentState (m_stateDim, 0),
      m_prevUtility (0.0),
      m_currentUtility (0.0),
      m_prevAction (0),
      m_reward (0),
      m_prototypeStates (m_numPrototypes),
      m_prototypeActions (m_numPrototypes),
      m_thetas (m_numPrototypes, 0.0),
      m_actionDist (0, m_numActions - 1)
{
  assert (m_actionToCwndChange.size () == m_numActions);

  std::uniform_int_distribution<int> stateDist (0, m_numIntervals - 1);
  for (std::vector<int> &i : m_prototypeStates)
    {
      i.resize (m_stateDim);
      std::generate (i.begin (), i.end (), [&] () { return stateDist (m_rng); });
    }

  std::generate (m_prototypeActions.begin (), m_prototypeActions.end (),
                 [&] () { return m_actionDist (m_rng); });
}

int
FuzzyKanerva::Main ()
{
  return Learn ();
}

int
FuzzyKanerva::Learn ()
{
  // Algorithm 1, line 9
  UpdateReward ();
  // We already have the current state, no need to get it

  // Algorithm 1, lines 10 - 11
  std::vector<double> prevMemberships (m_numPrototypes, 0);
  double prevQ = 0;
  for (int i = 0; i < m_numPrototypes; i++)
    {
      prevMemberships[i] = GetMembershipGrade (m_prevState, m_prevAction, m_prototypeStates[i],
                                               m_prototypeActions[i]);
      prevQ += prevMemberships[i] * m_thetas[i];
    }

  // Algorithm 1, lines 12 - 14
  double newMembership = 0;
  std::vector<double> newQs (m_numActions, 0);
  for (int i = 0; i < m_numActions; i++)
    {
      for (int j = 0; j < m_numPrototypes; j++)
        {
          newMembership =
              GetMembershipGrade (m_currentState, i, m_prototypeStates[i], m_prototypeActions[i]);
          newQs[i] += newMembership * m_thetas[i];
        }
    }

  // Algorithm 1, line 15
  const auto maxNewQs = std::max_element (newQs.begin (), newQs.end ());
  double delta = m_reward + m_gamma * (*maxNewQs) - prevQ;

  // Algorithm 1, lines 16 - 17
  double alphaT = m_alpha * pow (m_alphaDecayFactor, (int) (m_currentTime / m_alphaDecayInterval));
  for (int i = 0; i < m_numPrototypes; i++)
    {
      m_thetas[i] += prevMemberships[i] * alphaT * delta;
    }

  // Algorithm 1, lines 18 - 21
  int action = 0;
  // We use std::nextafter to get a distribution on [0, 1] instead of [0, 1)
  std::uniform_real_distribution<double> probDist (0.0, std::nextafter (1.0, 2.0));
  double prob = probDist (m_rng);
  if (prob > m_epsilon)
    {
      // Algorithm 1, line 19
      action = std::distance (newQs.begin (), maxNewQs); // This calculate the argmax
    }
  else
    {
      // Algorithm 1, line 21
      action = m_actionDist (m_rng);
    }

  // Algorithm 1, lines 23 - 24
  m_prevTime = m_currentTime;
  m_prevState = m_currentState;
  m_prevUtility = m_currentUtility;
  m_prevAction = action;

  // Algorithm 1, lines 22, 25
  return m_actionToCwndChange[action];
}

std::vector<int>
FuzzyKanerva::GetCurrentState ()
{
  return m_currentState;
}

double
FuzzyKanerva::GetMembershipGrade (const std::vector<int> &state0, int action0,
                                  const std::vector<int> &state1, int action1)
{
  assert (state0.size () == state1.size ());
  int dist = 0;
  for (int i = 0; i < state0.size (); i++)
    {
      dist += state0[i] != state1[i];
    }
  dist += action0 != action1;
  return exp (-(dist * dist) / (2 * m_sigma * m_sigma));
}

void
FuzzyKanerva::UpdateState (double time, double ackTime, double packetTime, double rttRatio,
                           double ssThresh, double throughput, double delay)
{
  m_currentTime = time;
  m_currentState[0] = discretize (m_ackTimeLower, m_ackTimeUpper, m_numIntervals, ackTime);
  m_currentState[1] = discretize (m_packetTimeLower, m_packetTimeUpper, m_numIntervals, packetTime);
  m_currentState[2] = discretize (m_rttRatioLower, m_rttRatioUpper, m_numIntervals, rttRatio);
  m_currentState[3] = discretize (m_ssThreshLower, m_ssThreshUpper, m_numIntervals, ssThresh);
  m_currentUtility = log (throughput) - m_delta * log (delay);
}

void
FuzzyKanerva::UpdateReward ()
{
  if (m_currentTime - m_prevTime < m_t)
    {
      return;
    }
  double eps = 1.0e-3;
  double delta = m_currentUtility - m_prevUtility;
  if (delta > 0 && delta > eps)
    {
      m_reward = 2;
    }
  else if (abs (delta) < eps)
    {
      m_reward = 0;
    }
  else
    {
      m_reward = -2;
    }
}

NS_LOG_COMPONENT_DEFINE ("TcpLearning");
NS_OBJECT_ENSURE_REGISTERED (TcpLearning);

TypeId
TcpLearning::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpLearning")
                          .SetParent<TcpNewReno> ()
                          .AddConstructor<TcpLearning> ()
                          .SetGroupName ("Internet");
  return tid;
}

TcpLearning::TcpLearning ()
    : TcpNewReno (),
      m_movingAverageWindowSize (100),
      m_ackTimeDiff (),
      m_packetTimeDiff (),
      m_ackTimeAvg (m_movingAverageWindowSize),
      m_packetTimeAvg (m_movingAverageWindowSize),
      m_rttRatio (),
      m_ssThresh (0),
      m_agent ()
{
}

TcpLearning::TcpLearning (const TcpLearning &sock)
    : TcpNewReno (sock),
      m_movingAverageWindowSize (sock.m_movingAverageWindowSize),
      m_ackTimeDiff (sock.m_ackTimeDiff),
      m_packetTimeDiff (sock.m_packetTimeDiff),
      m_ackTimeAvg (sock.m_ackTimeAvg),
      m_packetTimeAvg (sock.m_packetTimeAvg),
      m_rttRatio (sock.m_rttRatio),
      m_ssThresh (sock.m_ssThresh),
      m_agent (sock.m_agent)
{
}

TcpLearning::~TcpLearning (void)
{
}

std::string
TcpLearning::GetName () const
{
  return "TcpLearning";
}

void
TcpLearning::CongestionAvoidance (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  int action = m_agent.Main ();
  tcb->m_cWnd += action * tcb->m_segmentSize;
  NS_LOG_DEBUG ("learn," << m_agent.m_currentTime << ",action," << action);
  NS_LOG_DEBUG ("learn," << m_agent.m_currentTime << ",reward," << m_agent.m_reward);
}

// This function is called on every ACK
void
TcpLearning::PktsAcked (Ptr<TcpSocketState> tcb, uint32_t packetsAcked, const Time &rtt)
{
  double time = Simulator::Now ().GetMilliSeconds ();
  m_ackTimeAvg.Record (m_ackTimeDiff.RecordAndCalculate (time));
  m_packetTimeAvg.Record (m_packetTimeDiff.RecordAndCalculate (tcb->m_rcvTimestampValue));
  m_rttRatio.Record (rtt.GetMilliSeconds ());
  m_ssThresh = (double) tcb->m_ssThresh;

  // Calculate throughput
  double throughput = (tcb->m_segmentSize * packetsAcked); // in bytes
  double delay = rtt.GetMilliSeconds () / 2;

  double ackTimeAvg = m_ackTimeAvg.Avg ();
  double packetTimeAvg = m_packetTimeAvg.Avg ();
  double rttRatio = m_rttRatio.Ratio ();

  m_agent.UpdateState (time, ackTimeAvg, packetTimeAvg, rttRatio, m_ssThresh, throughput, delay);

  // Log values
  NS_LOG_DEBUG ("ack," << time << ",ackTimeAvg," << ackTimeAvg);
  NS_LOG_DEBUG ("ack," << time << ",packetTimeAvg," << packetTimeAvg);
  NS_LOG_DEBUG ("ack," << time << ",rttRatio," << rttRatio);
  NS_LOG_DEBUG ("ack," << time << ",ssthresh," << m_ssThresh);
  NS_LOG_DEBUG ("ack," << time << ",throughput," << throughput);
  NS_LOG_DEBUG ("ack," << time << ",delay," << delay);
  std::vector<int> state = m_agent.GetCurrentState ();
  NS_LOG_DEBUG ("ack," << time << ",state0," << state[0]);
  NS_LOG_DEBUG ("ack," << time << ",state1," << state[1]);
  NS_LOG_DEBUG ("ack," << time << ",state2," << state[2]);
  NS_LOG_DEBUG ("ack," << time << ",state3," << state[3]);
  NS_LOG_DEBUG ("ack," << time << ",utility," << m_agent.m_currentUtility);
}

Ptr<TcpCongestionOps>
TcpLearning::Fork (void)
{
  return CopyObject<TcpLearning> (this);
}

} // namespace ns3
