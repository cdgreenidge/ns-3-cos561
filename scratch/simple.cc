/* Simple toplogy simulation script */

#include <iostream>
#include <fstream>
#include <string>

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
static uint32_t cWndValue;
static uint32_t ssThreshValue;

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
  std::string transportProt = "TcpWestwood";
  std::string bandwidth = "7.5Mbps"; // Initial bottleneck bandwidth
  double restrictionTime = 800.0; // When to restrict bottleneck bandwidth
  std::string restrictedBandwidth = "2.5Mbps";
  std::string accessBandwidth = "1000Mbps";
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
  sinkApp.Start (Seconds (0));
  sinkApp.Stop (Seconds (stopTime));

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

  FlowMonitorHelper flowHelper;
  if (flowMonitor)
    {
      flowHelper.InstallAll ();
    }

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
  Simulator::Destroy ();

  return 0;
}
