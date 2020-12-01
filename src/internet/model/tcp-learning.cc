#include "tcp-learning.h"
#include "tcp-socket-state.h"

#include "ns3/log.h"

namespace ns3 {

MovingAvg::MovingAvg (int size)
    : m_underfilled (true), m_index (0), m_maxSize (size), m_buffer (std::vector<float> ())
{
  m_buffer.resize (m_maxSize);
}

void
MovingAvg::Enqueue (float item)
{
  m_buffer[m_index] = item;
  int nextIndex = (m_index + 1) % m_maxSize;
  if (nextIndex == 0 && m_underfilled)
    {
      m_underfilled = false;
    }
  m_index = nextIndex;
}

float
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

  float sum = 0.0;
  for (auto value : m_buffer)
    {
      sum += value;
    }
  return sum / size;
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
// m_rttRatioLower (0.0),
// m_rttRatioUpper (1.0),
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
