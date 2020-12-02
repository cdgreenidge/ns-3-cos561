#include <assert.h>
#include <cmath>
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

TcpLearning::TcpLearning (void) : TcpNewReno ()
// m_numIntervals (10),
// m_movingAverageWindowSize (5),
// m_ackInterarrivalLower (0.0),
// m_ackInterarrivalUpper (0.1),
// m_packetInterarrivalLower (0.0),
// m_packetInterarrivalUpper (0.1),
// m_rttRatioLower (1.0),
// m_rttRatioUpper (100.0),
// m_ssthreshLower (0.0),
// m_ssthreshUpper (100000.0)
{
  NS_LOG_FUNCTION (this);
}

TcpLearning::TcpLearning (const TcpLearning &sock) : TcpNewReno (sock)
{
  NS_LOG_FUNCTION (this);
}

TcpLearning::~TcpLearning (void)
{
  NS_LOG_FUNCTION (this);
}

std::string
TcpLearning::GetName () const
{
  NS_LOG_FUNCTION (this);

  return "TcpLearning";
}

void
TcpLearning::PktsAcked (Ptr<TcpSocketState> tcb, uint32_t packetsAcked, const Time &rtt)
{
  NS_LOG_FUNCTION (this << tcb << packetsAcked << rtt);
  return;
}

Ptr<TcpCongestionOps>
TcpLearning::Fork (void)
{
  NS_LOG_FUNCTION (this);

  return CopyObject<TcpLearning> (this);
}

} // namespace ns3
