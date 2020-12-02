

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
    avg.Record (1.0);
    avg.Record (2.0);
    NS_TEST_ASSERT_MSG_EQ (avg.Avg (), 1.5, "Test moving average works for 2 elements");

    // Test average works for four elements, overwriting the first element
    avg.Record (3.0);
    avg.Record (4.0);
    NS_TEST_ASSERT_MSG_EQ (avg.Avg (), 3.0, "Test moving average works for 4 elements");

    // Test average works for six elements, overwriting the first three elements
    avg.Record (5.0);
    avg.Record (6.0);
    NS_TEST_ASSERT_MSG_EQ (avg.Avg (), 5.0, "Test moving average works for 4 elements");
  };
};

class CurrentBestRatioTest : public TestCase
{
public:
  CurrentBestRatioTest () : TestCase ("Test current best ratio"){};

private:
  virtual void
  DoRun ()
  {
    // Test that current-best ratio is 0 on initialization
    CurrentBestRatio cbr = CurrentBestRatio ();
    NS_TEST_ASSERT_MSG_EQ (cbr.Ratio (), 1.0, "Test current best ratio is one on initialization");

    // Record a value
    cbr.Record (4.0);
    NS_TEST_ASSERT_MSG_EQ (cbr.Ratio (), 1.0, "Test current best ratio is one on first record");

    // Record another value, and some extraneous ones
    cbr.Record (8.0);
    NS_TEST_ASSERT_MSG_EQ (cbr.Ratio (), 2.0, "Test current best ratio is correct");

    // Record some extraneous values
    cbr.Record (10.0);
    cbr.Record (2.0);
    cbr.Record (5.0);
    NS_TEST_ASSERT_MSG_EQ (cbr.Ratio (), 2.5, "Test current best ratio is correct");
  };
};

class DiscretizeTest : public TestCase
{
public:
  DiscretizeTest () : TestCase ("Test discretize"){};

private:
  virtual void
  DoRun ()
  {
    double lower = 10.0;
    double upper = 11.0;
    int numSteps = 10;

    NS_TEST_ASSERT_MSG_EQ (discretize (lower, upper, numSteps, 9.0), 0, "Test discretize");
    NS_TEST_ASSERT_MSG_EQ (discretize (lower, upper, numSteps, 10.0), 0, "Test discretize");
    NS_TEST_ASSERT_MSG_EQ (discretize (lower, upper, numSteps, 10.15), 1, "Test discretize");
    NS_TEST_ASSERT_MSG_EQ (discretize (lower, upper, numSteps, 10.55), 5, "Test discretize");
    NS_TEST_ASSERT_MSG_EQ (discretize (lower, upper, numSteps, 11.0), 9, "Test discretize");
    NS_TEST_ASSERT_MSG_EQ (discretize (lower, upper, numSteps, 12.0), 9, "Test discretize");
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
    AddTestCase (new CurrentBestRatioTest (), TestCase::QUICK);
    AddTestCase (new DiscretizeTest (), TestCase::QUICK);
  }
};

static TcpLearningTestSuite g_tcpLearningTest; //!< Static variable for test initialization
