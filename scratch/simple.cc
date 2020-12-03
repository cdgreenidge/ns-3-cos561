/* Simple toplogy simulation script */

#include <iostream>
#include <fstream>
#include <string>
#include <math.h>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include "ns3/tcp-header.h"
#include "ns3/udp-header.h"
#include "ns3/enum.h"
#include "ns3/event-id.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/packet-sink.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Simple");

static bool firstCwnd = true;
static bool firstSshThr = true;
static bool firstRtt = true;
static bool firstRto = true;
static Ptr<OutputStreamWrapper> cWndStream;
static Ptr<OutputStreamWrapper> ssThreshStream;
static Ptr<OutputStreamWrapper> rttStream;
static Ptr<OutputStreamWrapper> rtoStream;
static Ptr<OutputStreamWrapper> nextTxStream;
static Ptr<OutputStreamWrapper> nextRxStream;
static Ptr<OutputStreamWrapper> inFlightStream;

static Ptr<OutputStreamWrapper> ackReceiveTimeStream;
static Ptr<OutputStreamWrapper> packetReceiveTimeStream;

AsciiTraceHelper ascii;
std::string throughput_tr_file_name = "simple-topology-throughput.data";
static Ptr<OutputStreamWrapper> throughputStream =
    ascii.CreateFileStream (throughput_tr_file_name.c_str ());

static uint32_t cWndValue;
static uint32_t ssThreshValue;

// for CalculateThroughput1
Ptr<PacketSink> sink; /* Pointer to the packet sink application */
uint64_t lastTotalRx = 0; /* The value of the last total received bytes */

// for CalculateThroughput3
uint32_t pktcounter = 0;
uint32_t oldcounter = 0;
uint32_t oldtime = 0;

// For current state of the system - Tara
// Hardcoded values based on calculation in datafile_analysis.ipynb
static double minAckInterArrivalTime = 3.847737644214032051841216628E-11;
static double maxAckInterArrivalTime = 2.765653250839936847269441079E-9;
// static uint32_t minPacketInterArrivalTime = 
// static uint32_t maxPacketInterArrivalTime = 
static uint32_t minSsThreshValue = 0;
static uint32_t maxSsThreshValue = 4294967295;
static double minRttRatioValue = 0.14173962713973684;
static double maxRttRatioValue = 7.05519;


// For moving averages - Tara
template <typename T, typename Total, size_t N>
class MovingAverage
{
  public:
    void addSample (T sample)
    {
        if (num_samples_ < N)
        {
            samples_[num_samples_++] = sample;
            total_ += sample;
        }
        else
        {
            T oldest = samples_[num_samples_++ % N];
            total_ += sample - oldest;
            oldest = sample;
        }
    }

    T getMovingAverage() const { return total_ / std::min(num_samples_, N); }

  private:
    T samples_[N];
    size_t num_samples_{0};
    Total total_{0};
};

const int movingAvgWindow = 10000; // use last 10000 samples to calculate moving avg
MovingAverage<int64_t, double, movingAvgWindow> movingAvgTimeBetweenAcks;
MovingAverage<int64_t, double, movingAvgWindow> movingAvgTimeBetweenSentPackets;

Time lastAckArrivalTime;
Time lastPacketArrivalTime;

int64_t numAcksSeenSinceLastUpdate = 0;
int64_t numSentPacketsSeenSinceLastUpdate = 0;

double currentRtt = 0;
double bestRtt = INFINITY;

// Assigns a bin for provided value. Assumes value falls between min and max -Tara
int assignBin (double value, double min, double max, int numBins) {
  // edge case
  if (value == max) return numBins - 1; 

  double interval = (max - min) / numBins;
  double shiftedValue = value - min;
  int bin = 0;

  for (int b = 0; b < numBins; b++) {
    if (shiftedValue >= interval * b && shiftedValue < interval * (b + 1)) {
      bin = b;
      break;
    }
  }

  return bin;
}

/* IN PROGRESS: Returns the current discretized state in a bin between 1 - 10: 
   1) Moving average of inter-arrival times between newly received ACKS
   2) Moving average of inter-arrival times between packets sent by sender
   3) Ratio of current RTT to best RTT observed
   4) Slow start threshold
   - Tara
*/ 
void getState(int state[]) {
  int numBins = 10;
  state[0] = assignBin(movingAvgTimeBetweenAcks.getMovingAverage(), minAckInterArrivalTime, maxAckInterArrivalTime, numBins);
  // state[1] = assignBin(movingAvgTimeBetweenSentPackets.getMovingAverage(), minPacketInterArrivalTime, maxPacketInterArrivalTime, numBins);
  state[2] = assignBin(currentRtt / bestRtt, minRttRatioValue, maxRttRatioValue, numBins);
  state[3] = assignBin(ssThreshValue, minSsThreshValue, maxSsThreshValue, numBins);

  std::cout<< "state: " << state[0] << ", " << state[2] << ", " << state[3] <<std::endl;

}

void ReceivePacket (Ptr<const Packet> packet, const Address &)
{
  pktcounter += 1;
}

void
CalculateThroughput3 (uint32_t tcpAduSize)
{
  double now = Simulator::Now ().GetSeconds ();
  double throughput = (pktcounter - oldcounter) * tcpAduSize / (now - oldtime);
  NS_LOG_UNCOND ("Throughput (Mb/sec) = " << throughput * 8 / 1024 / 1024);

  *throughputStream->GetStream () << now << " " << throughput * 8 / 1024 / 1024 << std::endl;

  oldcounter = pktcounter;
  oldtime = now;
  Simulator::Schedule (Seconds (1), &CalculateThroughput3, tcpAduSize);
}

void
CalculateThroughput2 (Ptr<FlowMonitor> flowMon, FlowMonitorHelper *fmhelper)
{
  std::map<FlowId, FlowMonitor::FlowStats> flowStats = flowMon->GetFlowStats ();
  Ptr<Ipv4FlowClassifier> classing = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier ());
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator stats = flowStats.begin ();
       stats != flowStats.end (); ++stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classing->FindFlow (stats->first);
      std::cout << "Flow ID            : " << stats->first << " ; " << t.sourceAddress << " -----> "
                << t.destinationAddress << std::endl;
      //            std::cout<<"Tx Packets = " << stats->second.txPackets<<std::endl;
      //            std::cout<<"Rx Packets = " << stats->second.rxPackets<<std::endl;
      std::cout << "Duration        : "
                << stats->second.timeLastRxPacket.GetSeconds () -
                       stats->second.timeFirstTxPacket.GetSeconds ()
                << std::endl;
      std::cout << "Last Received Packet    : " << stats->second.timeLastRxPacket.GetSeconds ()
                << " Seconds" << std::endl;
      std::cout << "Throughput: "
                << stats->second.rxBytes * 8.0 /
                       (stats->second.timeLastRxPacket.GetSeconds () -
                        stats->second.timeFirstTxPacket.GetSeconds ()) /
                       1024 / 1024
                << " Mbps" << std::endl;
      std::cout << "---------------------------------------------------------------------------"
                << std::endl;
      if (stats->first == 1)
        {
          *throughputStream->GetStream () << Simulator::Now ().GetSeconds () << " "
                                          << stats->second.rxBytes * 8.0 /
                                                 (stats->second.timeLastRxPacket.GetSeconds () -
                                                  stats->second.timeFirstTxPacket.GetSeconds ()) /
                                                 1024 / 1024
                                          << std::endl;
        }
    }
  Simulator::Schedule (Seconds (1), &CalculateThroughput2, flowMon, fmhelper);
}

void
CalculateThroughput1 ()
{
  // https://www.nsnam.org/doxygen/wifi-tcp_8cc_source.html
  Time now = Simulator::Now (); /* Return the simulator's virtual time. */
  double cur = (sink->GetTotalRx () - lastTotalRx) * (double) 8 / 1024 / 1024 /
               0.1; /* Convert Application RX Packets to MBits. */
  //std::cout << now.GetSeconds () << "s: \t" << cur << " Mbit/s" << std::endl;
  lastTotalRx = sink->GetTotalRx ();
  Simulator::Schedule (Seconds (1), &CalculateThroughput1);
  *throughputStream->GetStream () << now.GetSeconds () << " " << cur << std::endl;
}

// Tracer for time between acks - Tara
static void
AckTracer (SequenceNumber32 oldValue, SequenceNumber32 newValue)
{
  NS_UNUSED (oldValue);
  Time currentTime = Simulator::Now ();
  *ackReceiveTimeStream->GetStream () << newValue << " " << currentTime.GetFemtoSeconds() << " " << std::endl;

  // Update estimate for arrival time between acks
  numAcksSeenSinceLastUpdate = newValue - oldValue; // this should never be 0
  double currentEstimatedTimeBetweenAcks = (currentTime.GetFemtoSeconds() - lastAckArrivalTime.GetFemtoSeconds()) / numAcksSeenSinceLastUpdate;
  movingAvgTimeBetweenAcks.addSample(currentEstimatedTimeBetweenAcks);

  lastAckArrivalTime = currentTime;

}

// Tracer for time between packets - Tara
static void
PacketTracer (SequenceNumber32 oldValue, SequenceNumber32 newValue)
{
  NS_UNUSED (oldValue);
  Time currentTime = Simulator::Now ();
  *packetReceiveTimeStream->GetStream () << newValue << " " << currentTime.GetFemtoSeconds() << " " << std::endl;

  // Update estimate for arrival time between packets
  numSentPacketsSeenSinceLastUpdate = newValue - oldValue; // this should never be 0
  double currentEstimatedTimeBetweenPackets = (currentTime.GetFemtoSeconds() - lastPacketArrivalTime.GetFemtoSeconds()) / numSentPacketsSeenSinceLastUpdate;
  movingAvgTimeBetweenSentPackets.addSample(currentEstimatedTimeBetweenPackets);

  lastPacketArrivalTime = currentTime;
}

static void
CwndTracer (uint32_t oldval, uint32_t newval)
{
  if (firstCwnd)
    {
      *cWndStream->GetStream () << "0.0 " << oldval << std::endl;
      firstCwnd = false;
    }
  *cWndStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval << std::endl;
  cWndValue = newval;

  if (!firstSshThr)
    {
      *ssThreshStream->GetStream ()
          << Simulator::Now ().GetSeconds () << " " << ssThreshValue << std::endl;
    }
}

static void
SsThreshTracer (uint32_t oldval, uint32_t newval)
{
  if (firstSshThr)
    {
      *ssThreshStream->GetStream () << "0.0 " << oldval << std::endl;
      firstSshThr = false;
    }
  *ssThreshStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval << std::endl;
  ssThreshValue = newval;

  if (!firstCwnd)
    {
      *cWndStream->GetStream () << Simulator::Now ().GetSeconds () << " " << cWndValue << std::endl;
    }
}

static void
RttTracer (Time oldval, Time newval)
{
  if (firstRtt)
    {
      *rttStream->GetStream () << "0.0 " << oldval.GetSeconds () << std::endl;
      firstRtt = false;
    }
  *rttStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval.GetSeconds ()
                           << std::endl;

  // Update current rtt and best rtt, if necessary
  currentRtt = newval.GetSeconds();
  if (currentRtt < bestRtt) bestRtt = currentRtt;
}

static void
RtoTracer (Time oldval, Time newval)
{
  if (firstRto)
    {
      *rtoStream->GetStream () << "0.0 " << oldval.GetSeconds () << std::endl;
      firstRto = false;
    }
  *rtoStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval.GetSeconds ()
                           << std::endl;
}

static void
NextTxTracer (SequenceNumber32 old, SequenceNumber32 nextTx)
{
  NS_UNUSED (old);
  *nextTxStream->GetStream () << Simulator::Now ().GetSeconds () << " " << nextTx << std::endl;
}

static void
InFlightTracer (uint32_t old, uint32_t inFlight)
{
  NS_UNUSED (old);
  *inFlightStream->GetStream () << Simulator::Now ().GetSeconds () << " " << inFlight << std::endl;
}

static void
NextRxTracer (SequenceNumber32 old, SequenceNumber32 nextRx)
{
  NS_UNUSED (old);
  *nextRxStream->GetStream () << Simulator::Now ().GetSeconds () << " " << nextRx << std::endl;
}

static void
TraceCwnd (std::string cwnd_tr_file_name)
{
  AsciiTraceHelper ascii;
  cWndStream = ascii.CreateFileStream (cwnd_tr_file_name.c_str ());
  Config::ConnectWithoutContext ("/NodeList/1/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",
                                 MakeCallback (&CwndTracer));
}

static void
TraceSsThresh (std::string ssthresh_tr_file_name)
{
  AsciiTraceHelper ascii;
  ssThreshStream = ascii.CreateFileStream (ssthresh_tr_file_name.c_str ());
  Config::ConnectWithoutContext ("/NodeList/1/$ns3::TcpL4Protocol/SocketList/0/SlowStartThreshold",
                                 MakeCallback (&SsThreshTracer));
}

static void
TraceRtt (std::string rtt_tr_file_name)
{
  AsciiTraceHelper ascii;
  rttStream = ascii.CreateFileStream (rtt_tr_file_name.c_str ());
  Config::ConnectWithoutContext ("/NodeList/1/$ns3::TcpL4Protocol/SocketList/0/RTT",
                                 MakeCallback (&RttTracer));
}

static void
TraceRto (std::string rto_tr_file_name)
{
  AsciiTraceHelper ascii;
  rtoStream = ascii.CreateFileStream (rto_tr_file_name.c_str ());
  Config::ConnectWithoutContext ("/NodeList/1/$ns3::TcpL4Protocol/SocketList/0/RTO",
                                 MakeCallback (&RtoTracer));
}

static void
TraceNextTx (std::string &next_tx_seq_file_name)
{
  AsciiTraceHelper ascii;
  nextTxStream = ascii.CreateFileStream (next_tx_seq_file_name.c_str ());
  Config::ConnectWithoutContext ("/NodeList/1/$ns3::TcpL4Protocol/SocketList/0/NextTxSequence",
                                 MakeCallback (&NextTxTracer));
}

static void
TraceInFlight (std::string &in_flight_file_name)
{
  AsciiTraceHelper ascii;
  inFlightStream = ascii.CreateFileStream (in_flight_file_name.c_str ());
  Config::ConnectWithoutContext ("/NodeList/1/$ns3::TcpL4Protocol/SocketList/0/BytesInFlight",
                                 MakeCallback (&InFlightTracer));
}

static void
TraceNextRx (std::string &next_rx_seq_file_name)
{
  AsciiTraceHelper ascii;
  nextRxStream = ascii.CreateFileStream (next_rx_seq_file_name.c_str ());
  Config::ConnectWithoutContext (
      "/NodeList/2/$ns3::TcpL4Protocol/SocketList/1/RxBuffer/NextRxSequence",
      MakeCallback (&NextRxTracer));
}

// Helper to find estimates for the min and max differences in ack arrival times - Tara
static void
TraceAcks (std::string ack_tr_file_name)
{
  AsciiTraceHelper ascii;
  ackReceiveTimeStream = ascii.CreateFileStream (ack_tr_file_name.c_str ());
  // Trace acks on sender (node 1)
  Config::ConnectWithoutContext ("/NodeList/1/$ns3::TcpL4Protocol/SocketList/0/HighestRxAck",
                                 MakeCallback (&AckTracer));
}

// Helper to find estimates for the min and max differences in packet arrival times - Tara
static void
TracePackets (std::string packet_tr_file_name)
{
  AsciiTraceHelper ascii;
  packetReceiveTimeStream = ascii.CreateFileStream (packet_tr_file_name.c_str ());
  // Trace packets on receiver (node 2)
  Config::ConnectWithoutContext ("/NodeList/1/$ns3::TcpL4Protocol/SocketList/0/HighestRxAck",
                                 MakeCallback (&PacketTracer));
  //Config::ConnectWithoutContext ("/NodeList/2/$ns3::TcpL4Protocol/SocketList/1/RxBuffer/NextRxSequence",
                                 //MakeCallback (&PacketTracer));

  // Config::ConnectWithoutContext ("/NodeList/2/$ns3::TcpL4Protocol/SocketList/0/HighestRxSequence",
  //                                MakeCallback (&PacketTracer));
}

/**
 * Callback to restrict bottleneck bandwidth
 *
 * @param bottleneckDevices A NetDeviceContainer holding the nodes on either end of
 *        the bottleneck link
 * @param restrictedBandwidth The bandwidth to restrict the link to
 */
static void
RestrictBandwidth (NetDeviceContainer &bottleneckDevices, std::string restrictedBandwidth)
{
  assert (bottleneckDevices.GetN () == 2);
  NS_LOG_INFO ("Restricting bottleneck bandwidth...");
  Ptr<NetDevice> node = bottleneckDevices.Get (0);
  node->SetAttribute ("DataRate", StringValue ("2.5Mbps"));
  node = bottleneckDevices.Get (1);
  node->SetAttribute ("DataRate", StringValue ("2.5Mbps"));
}

int
main (int argc, char *argv[])
{
  std::string transportProt = "TcpLearning";
  std::string bandwidth = "7.5Mbps"; // Initial bottleneck bandwidth
  double restrictionTime = 800.0; // When to restrict bottleneck bandwidth
  std::string restrictedBandwidth = "2.5Mbps";
  std::string accessBandwidth = "100Mbps";
  std::string delay = "25ms"; // For 100ms RTT total
  bool tracing = false;
  std::string prefixFileName = "simple-topology";
  uint32_t mtuBytes = 15728; // So that 50 packets fit in the buffer
  double duration = 1200.0;
  uint32_t run = 0;
  bool flowMonitor = false;
  bool pcap = false;
  bool sack = true;
  std::string recovery = "ns3::TcpClassicRecovery";

  CommandLine cmd (__FILE__);
  cmd.AddValue ("transportProt",
                "Transport protocol to use: TcpNewReno, TcpLinuxReno, "
                "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                "TcpBic, TcpYeah, TcpIllinois, TcpWestwood, TcpWestwoodPlus, TcpLedbat, "
                "TcpLp, TcpDctcp",
                transportProt);
  cmd.AddValue ("run", "Run index (for setting repeatable seeds)", run);
  cmd.AddValue ("prefix_name", "Prefix of output trace file", prefixFileName);
  cmd.AddValue ("tracing", "Flag to enable/disable tracing", tracing);
  cmd.AddValue ("flow_monitor", "Enable flow monitor", flowMonitor);
  cmd.AddValue ("pcap_tracing", "Enable or disable PCAP tracing", pcap);
  cmd.AddValue ("duration", "Time to allow flow to run in seconds", duration);
  cmd.AddValue ("restriction_time", "Time to restrict bottleneck bandwidth", restrictionTime);
  cmd.AddValue ("sack", "Enable or disable SACK option", sack);
  cmd.AddValue ("recovery", "Recovery algorithm type to use (e.g., ns3::TcpPrrRecovery", recovery);
  cmd.Parse (argc, argv);

  transportProt = std::string ("ns3::") + transportProt;

  SeedManager::SetSeed (1);
  SeedManager::SetRun (run);
  double startTime = 0.1;
  double stopTime = startTime + duration;

  // The user may find it convenient to enable logging
  //LogComponentEnable("TcpVariantsComparison", LOG_LEVEL_ALL);
  //LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
  //LogComponentEnable("PfifoFastQueueDisc", LOG_LEVEL_ALL);

  // Calculate the application data unit size required for our desired packet size
  Header *tempHeader = new Ipv4Header ();
  uint32_t ipHeader = tempHeader->GetSerializedSize ();
  NS_LOG_LOGIC ("IP Header size is: " << ipHeader);
  delete tempHeader;
  tempHeader = new TcpHeader ();
  uint32_t tcpHeader = tempHeader->GetSerializedSize ();
  NS_LOG_LOGIC ("TCP Header size is: " << tcpHeader);
  delete tempHeader;
  uint32_t tcpAduSize = mtuBytes - 20 - (ipHeader + tcpHeader);
  NS_LOG_LOGIC ("TCP ADU size is: " << tcpAduSize);
  NS_LOG_LOGIC ("TCP Protocol is: " << transportProt);

  // Configure TCP parameters
  // We set the TCP buffer to bandwidth-delay product (BDP)
  // RTT (delay) is 100ms, bottleneck bandwidth is 7.5 mebibits, so
  // the BDP is 1e-1 * 7.86432e6 = 786432, roughly 0.7 Mb
  // Packet size should be floor(786432 / 50) = 15,728 bits so 50 packets fit in the
  // buffer
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (786432));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (786432));
  Config::SetDefault ("ns3::TcpSocketBase::Sack", BooleanValue (sack));
  Config::SetDefault ("ns3::TcpL4Protocol::RecoveryType",
                      TypeIdValue (TypeId::LookupByName (recovery)));

  // Select TCP variant
  if (transportProt.compare ("ns3::TcpWestwoodPlus") == 0)
    {
      // TcpWestwoodPlus is not an actual TypeId name; we need TcpWestwood here
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType",
                          TypeIdValue (TcpWestwood::GetTypeId ()));
      // the default protocol type in ns3::TcpWestwood is WESTWOOD
      Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwood::WESTWOODPLUS));
    }
  else
    {
      TypeId tcpTid;
      NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe (transportProt, &tcpTid),
                           "TypeId " << transportProt << " not found");
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType",
                          TypeIdValue (TypeId::LookupByName (transportProt)));
    }

  // Create topology helpers
  PointToPointHelper accessLink;
  accessLink.SetDeviceAttribute ("DataRate", StringValue (accessBandwidth));
  accessLink.SetChannelAttribute ("Delay", StringValue (delay));

  PointToPointHelper bottleneckLink;
  bottleneckLink.SetDeviceAttribute ("DataRate", StringValue (bandwidth));
  bottleneckLink.SetChannelAttribute ("Delay", StringValue (delay));

  InternetStackHelper stack;

  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");

  // Create topology
  NodeContainer gateways;
  gateways.Create (1); // Node ID 0
  NodeContainer sources;
  sources.Create (1); // Node ID 1
  NodeContainer sinks;
  sinks.Create (1); // Node ID 2

  stack.InstallAll ();

  NetDeviceContainer accessDevices = accessLink.Install (sources.Get (0), gateways.Get (0));
  address.NewNetwork ();
  Ipv4InterfaceContainer accessInterfaces = address.Assign (accessDevices);

  NetDeviceContainer bottleneckDevices = bottleneckLink.Install (gateways.Get (0), sinks.Get (0));
  address.NewNetwork ();
  Ipv4InterfaceContainer bottleneckInterfaces = address.Assign (bottleneckDevices);

  Ipv4InterfaceContainer sinkInterfaces;
  sinkInterfaces.Add (bottleneckInterfaces.Get (1));

  NS_LOG_INFO ("Initialize Global Routing.");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Configure FTP application (which will send our data)
  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);

  AddressValue remoteAddress (InetSocketAddress (sinkInterfaces.GetAddress (0, 0), port));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (tcpAduSize));
  BulkSendHelper ftp ("ns3::TcpSocketFactory", Address ());
  ftp.SetAttribute ("Remote", remoteAddress);
  ftp.SetAttribute ("SendSize", UintegerValue (tcpAduSize));
  ftp.SetAttribute ("MaxBytes", UintegerValue (0));

  ApplicationContainer sourceApp = ftp.Install (sources.Get (0));
  sourceApp.Start (Seconds (0));
  sourceApp.Stop (Seconds (stopTime - 3));

  sinkHelper.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));
  ApplicationContainer sinkApp = sinkHelper.Install (sinks.Get (0));

  // sink added by olga
  sink = StaticCast<PacketSink> (sinkApp.Get (0));

  sinkApp.Start (Seconds (0));
  sinkApp.Stop (Seconds (stopTime));

  FlowMonitorHelper flowHelper;

  // added by olga
  Ptr<FlowMonitor> monitor;
  monitor = flowHelper.InstallAll ();

  // Configure monitoring
  if (tracing)
    {
      std::ofstream ascii;
      Ptr<OutputStreamWrapper> ascii_wrap;
      ascii.open ((prefixFileName + "-ascii").c_str ());
      ascii_wrap = new OutputStreamWrapper ((prefixFileName + "-ascii").c_str (), std::ios::out);
      stack.EnableAsciiIpv4All (ascii_wrap);

      Simulator::Schedule (Seconds (0.0000001), &TraceCwnd, prefixFileName + "-cwnd.data");
      Simulator::Schedule (Seconds (0.0000001), &TraceSsThresh, prefixFileName + "-ssth.data");
      Simulator::Schedule (Seconds (0.0000001), &TraceRtt, prefixFileName + "-rtt.data");
      Simulator::Schedule (Seconds (0.0000001), &TraceRto, prefixFileName + "-rto.data");
      Simulator::Schedule (Seconds (0.0000001), &TraceNextTx, prefixFileName + "-next-tx.data");
      Simulator::Schedule (Seconds (0.0000001), &TraceInFlight, prefixFileName + "-inflight.data");

      Simulator::Schedule (Seconds(0.000000001), &TraceAcks, prefixFileName + "-ackreceivetimes.data");
      Simulator::Schedule (Seconds(0.000000001), &TracePackets, prefixFileName + "-packetreceivetimes.data");

      
      // throughput added by olga

      // Schedule throughput2
      //Simulator::Schedule (Seconds (1), &CalculateThroughput1);

      // Schedule throughput2
      //Simulator::Schedule (Seconds (1), &CalculateThroughput2, monitor, &flowHelper);

      //Schedule throughput3
      Config::ConnectWithoutContext ("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx",
                                     MakeCallback (&ReceivePacket));
      Simulator::Schedule (Seconds (1), &CalculateThroughput3, tcpAduSize);

      // Beware: if RTT is modified, the Rx buffer may not be created before
      // trace registration, which will cause an error. Increase the scheduled time
      // as necessary to make sure the trace is registered after buffer creation.
      Simulator::Schedule (Seconds (0.15), &TraceNextRx, prefixFileName + "-next-rx.data");
    }

  if (pcap)
    {
      bottleneckLink.EnablePcapAll (prefixFileName, true);
      accessLink.EnablePcapAll (prefixFileName, true);
    }

  // olga: flowHelper declaration moved earlier
  //  if (flowMonitor)
  //    {
  //      flowHelper.InstallAll ();
  //    }

  // Test getState
  int state[] = { 10, 20, 30, 40 };
  Simulator::Schedule (Seconds (0.5), &getState, state);


  // Run simulation
  Simulator::Stop (Seconds (stopTime));
  Simulator::Schedule (Seconds (restrictionTime), &RestrictBandwidth, bottleneckDevices,
                       restrictedBandwidth);
  Simulator::Run ();

  // Dump flow and clean up
  if (flowMonitor)
    {
      flowHelper.SerializeToXmlFile (prefixFileName + ".flowmonitor", true, true);
    }

  // added by olga
  //double averageThroughput = ((sink->GetTotalRx () * 8) / (1e6 * duration));

  Simulator::Destroy ();
  //std::cout << "\nAverage throughput: " << averageThroughput << " Mbit/s" << std::endl;

  return 0;
}
