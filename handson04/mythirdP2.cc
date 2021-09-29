#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/simulator.h"
//#include "ns3/netanim-module.h"
//#include "ns3/object.h"
//#include "ns3/uinteger.h"
//#include "ns3/traced-value.h"
//#include "ns3/trace-source-accessor.h"

// Default Network Topology
//
//   Wifi 10.1.3.0
//                 AP
//  *    *    *    *
//  |    |    |    |    10.1.1.0
// n5   n6   n7   n0 -------------- n1   n2   n3   n4
//                   point-to-point  |    |    |    |
//                                   ================
//                                     LAN 10.1.2.0

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThirdScriptExample");

void
CourseChange (std::string context, Ptr<const MobilityModel> model)
{
  Vector position = model->GetPosition ();
  //NS_LOG_UNCOND (context << " x = " << position.x << ", y = " << position.y);
  NS_LOG_UNCOND (position.x << "," << position.y << "," << context.substr (10,1));
}

int 
main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nCsma = 3;
  uint32_t nWifi = 3;
  bool tracing = true;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("nCsma", "Number of \"extra\" CSMA nodes/devices", nCsma);
  cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc,argv);

  if (nWifi > 18)
    {
      std::cout << "nWifi should be 18 or less; otherwise grid layout exceeds the bounding box" << std::endl;
      return 1;
    }

  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  NodeContainer p2pNodes;
  p2pNodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer p2pDevices;
  p2pDevices = pointToPoint.Install (p2pNodes);

  NodeContainer csmaNodes;
  csmaNodes.Add (p2pNodes.Get (1));
  csmaNodes.Create (nCsma);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install (csmaNodes);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nWifi);
  NodeContainer wifiApNode = p2pNodes.Get (0);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
								 "X", DoubleValue(20.0),
								 "X", DoubleValue(20.0),
				 	 	 	 	 "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=20.0]"));

  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Time", StringValue("1s"),
							 "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=1.8]"),
							 "Direction", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=20.0]"),
							 "Bounds", StringValue("-50|50|-50|50"));

  mobility.Install (wifiStaNodes);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);
  mobility.Install (csmaNodes);
  //AnimationInterface::SetConstantPosition (p2pNodes.Get (0), 13, 15);
  //AnimationInterface::SetConstantPosition (p2pNodes.Get (1), 17, 15);

  InternetStackHelper stack;
  stack.Install (csmaNodes);
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces;
  p2pInterfaces = address.Assign (p2pDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces;
  csmaInterfaces = address.Assign (csmaDevices);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  address.Assign (staDevices);
  address.Assign (apDevices);

  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (csmaNodes.Get (nCsma));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (25.0));

  UdpEchoClientHelper echoClient1 (csmaInterfaces.GetAddress (nCsma), 9);
  echoClient1.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient1.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient1.SetAttribute ("PacketSize", UintegerValue (1024));

  UdpEchoClientHelper echoClient2 (csmaInterfaces.GetAddress (nCsma), 9);
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (2048));

  UdpEchoClientHelper echoClient3 (csmaInterfaces.GetAddress (nCsma), 9);
  echoClient3.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient3.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient3.SetAttribute ("PacketSize", UintegerValue (4096));

  ApplicationContainer clientApps1 = echoClient1.Install (wifiStaNodes.Get (nWifi - 1));
  clientApps1.Start (Seconds (2.0));
  clientApps1.Stop (Seconds (25.0));

  ApplicationContainer clientApps2 = echoClient2.Install (wifiStaNodes.Get (nWifi - 2));
  clientApps2.Start (Seconds (2.0));
  clientApps2.Stop (Seconds (25.0));

  ApplicationContainer clientApps3 = echoClient3.Install (wifiStaNodes.Get (nWifi - 3));
  clientApps3.Start (Seconds (2.0));
  clientApps3.Stop (Seconds (25.0));


  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (25.0));

  if (tracing)
    {
      phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
      pointToPoint.EnablePcapAll ("third");
      phy.EnablePcap ("third", apDevices.Get (0));
      csma.EnablePcap ("third", csmaDevices.Get (0), true);
    }

  //AnimationInterface anim ("third.xml");

  std::ostringstream oss1;
  oss1 << "/NodeList/" << wifiStaNodes.Get (nWifi - 1)->GetId () << "/$ns3::MobilityModel/CourseChange";
  Config::Connect (oss1.str (), MakeCallback (&CourseChange));

  std::ostringstream oss2;
  oss2 << "/NodeList/" << wifiStaNodes.Get (nWifi - 2)->GetId () << "/$ns3::MobilityModel/CourseChange";
  Config::Connect (oss2.str (), MakeCallback (&CourseChange));

  std::ostringstream oss3;
  oss3 << "/NodeList/" << wifiStaNodes.Get (nWifi - 3)->GetId () << "/$ns3::MobilityModel/CourseChange";
  Config::Connect (oss3.str (), MakeCallback (&CourseChange));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}


