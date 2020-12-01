

#include "ns3/test.h"
#include "ns3/log.h"
#include "ns3/tcp-congestion-ops.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/tcp-learning.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpLearningTestSuite");

/**
 * \ingroup internet-test
 * \ingroup tests
 *
 * \brief TcpLearning C-AIMD algorithm tests.
 */
class MovingAvgTest : public TestCase
{
public:
  MovingAvgTest () : TestCase ("Test moving average calculates correct average"){};

private:
  virtual void
  DoRun ()
  {
    // Test that moving average is zero on initialization
    MovingAvg avg = MovingAvg (3);
    NS_TEST_ASSERT_MSG_EQ (avg.Avg (), 0.0, "Test moving average is zero on initialization");

    // Test average works for two elements
    avg.Enqueue (1.0);
    avg.Enqueue (2.0);
    NS_TEST_ASSERT_MSG_EQ (avg.Avg (), 1.5, "Test moving average works for 2 elements");

    // Test average works for four elements, overwriting the first element
    avg.Enqueue (3.0);
    avg.Enqueue (4.0);
    NS_TEST_ASSERT_MSG_EQ (avg.Avg (), 3.0, "Test moving average works for 4 elements");

    // Test average works for six elements, overwriting the first three elements
    avg.Enqueue (5.0);
    avg.Enqueue (6.0);
    NS_TEST_ASSERT_MSG_EQ (avg.Avg (), 5.0, "Test moving average works for 4 elements");
  };
};

/**
 * \ingroup internet-test
 * \ingroup tests
 *
 * \brief TCP Learning TestSuite
 */
class TcpLearningTestSuite : public TestSuite
{
public:
  TcpLearningTestSuite () : TestSuite ("tcp-learning-test", UNIT)
  {
    AddTestCase (new MovingAvgTest (), TestCase::QUICK);
  }
};

static TcpLearningTestSuite g_tcpLearningTest; //!< Static variable for test initialization
