#include "tcp-learning.h"
#include "tcp-socket-state.h"

#include "ns3/log.h"

namespace ns3 {

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
