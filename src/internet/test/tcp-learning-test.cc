

#include "ns3/test.h"
#include "ns3/log.h"
#include "ns3/tcp-congestion-ops.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/tcp-learning.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpLearningTestSuite");

class RunningDifferenceTest : public TestCase
{
public:
  RunningDifferenceTest () : TestCase ("Test running difference"){};

private:
  virtual void
  DoRun ()
  {
    // Test that moving average is the initial value on initialization
    // It's technically undefined, but this value won't blow anything up like a NaN
    // would
    RunningDifference diff = RunningDifference ();

    double out;

    out = diff.RecordAndCalculate (1.0);
    NS_TEST_ASSERT_MSG_EQ (out, 1.0, "Test running difference");
    out = diff.RecordAndCalculate (1.5);
    NS_TEST_ASSERT_MSG_EQ (out, 1.5 - 1.0, "Test running difference");
    out = diff.RecordAndCalculate (1.7);
    NS_TEST_ASSERT_MSG_EQ (out, 1.7 - 1.5, "Test running difference");
  };
};

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

class FuzzyKanervaUpdateStateTest : public TestCase
{
public:
  FuzzyKanervaUpdateStateTest () : TestCase ("Test FuzzyKanerva.UpdateState ()"){};

private:
  virtual void
  DoRun ()
  {
    FuzzyKanerva agent;
    std::vector<int> initial = {0, 0, 0, 0};
    bool isEqual = initial == agent.GetCurrentState ();
    NS_TEST_ASSERT_MSG_EQ (isEqual, true, "Test FuzzyKanerva.UpdateState ()");

    // Test that state is discretized and updated properly
    double ackTime = agent.m_ackTimeLower +
                     0.1 * (agent.m_ackTimeUpper - agent.m_ackTimeLower) / agent.m_numIntervals;
    double packetTime =
        agent.m_packetTimeLower +
        2.1 * (agent.m_packetTimeUpper - agent.m_packetTimeLower) / agent.m_numIntervals;
    double rttRatio = agent.m_rttRatioLower +
                      4.1 * (agent.m_rttRatioUpper - agent.m_rttRatioLower) / agent.m_numIntervals;
    double ssThresh = agent.m_ssThreshLower +
                      6.1 * (agent.m_ssThreshUpper - agent.m_ssThreshLower) / agent.m_numIntervals;
    agent.UpdateState (1.0, ackTime, packetTime, rttRatio, ssThresh, 1.0, 0.1);
    std::vector<int> expected = {0, 2, 4, 6};
    isEqual = expected == agent.GetCurrentState ();
    NS_TEST_ASSERT_MSG_EQ (isEqual, true, "Test FuzzyKanerva.UpdateState ()");
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
    AddTestCase (new RunningDifferenceTest (), TestCase::QUICK);
    AddTestCase (new MovingAvgTest (), TestCase::QUICK);
    AddTestCase (new CurrentBestRatioTest (), TestCase::QUICK);
    AddTestCase (new DiscretizeTest (), TestCase::QUICK);
    AddTestCase (new FuzzyKanervaUpdateStateTest (), TestCase::QUICK);
  }
};

static TcpLearningTestSuite g_tcpLearningTest; //!< Static variable for test initialization
