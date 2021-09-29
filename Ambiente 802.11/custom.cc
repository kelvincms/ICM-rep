/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// Default network topology includes some number of AP nodes specified by
// the variable nWifis (defaults to two).  Off of each AP node, there are some
// number of STA nodes specified by the variable nStas (defaults to two).
// Each AP talks to its associated STA nodes.  There are bridge net devices
// on each AP node that bridge the whole thing into one network.
//
//      +-----+      +-----+            +-----+      +-----+
//      | STA |      | STA |            | STA |      | STA |
//      +-----+      +-----+            +-----+      +-----+
//    192.168.0.2  192.168.0.3        192.168.0.5  192.168.0.6
//      --------     --------           --------     --------
//      WIFI STA     WIFI STA           WIFI STA     WIFI STA
//      --------     --------           --------     --------
//        ((*))       ((*))       |      ((*))        ((*))
//                                |
//              ((*))             |             ((*))
//             -------                         -------
//             WIFI AP   CSMA ========= CSMA   WIFI AP
//             -------   ----           ----   -------
//             ##############           ##############
//                 BRIDGE                   BRIDGE
//             ##############           ##############
//               192.168.0.1              192.168.0.4
//               +---------+              +---------+
//               | AP Node |              | AP Node |
//               +---------+              +---------+

#include "ns3/command-line.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/rectangle.h"
#include "ns3/string.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/csma-helper.h"
#include "ns3/bridge-helper.h"
#include "ns3/packet-socket-address.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  uint32_t nWifis = 4;
  uint32_t nStas = 4;
  uint32_t nPackg = 10;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("nWifis", "Number of wifi networks", nWifis);
  cmd.AddValue ("nStas", "Number of stations per wifi network", nStas);
  cmd.AddValue ("nStas", "Number of packages sent randomly", nPackg);
  cmd.Parse (argc, argv);

  Ipv4InterfaceContainer backboneInterfaces;
  std::vector<NodeContainer> staNodes;
  std::vector<NetDeviceContainer> staDevices;
  std::vector<NetDeviceContainer> apDevices;
  std::vector<Ipv4InterfaceContainer> staInterfaces;
  std::vector<Ipv4InterfaceContainer> apInterfaces;


  YansWifiPhyHelper wifiPhy;
  CsmaHelper csma;
  Ipv4AddressHelper ip;
  ip.SetBase ("192.168.0.0", "255.255.255.0");

  NodeContainer backboneNodes;
  backboneNodes.Create (nWifis);

  InternetStackHelper stack;
  stack.Install (backboneNodes);

  NetDeviceContainer backboneDevices;
  backboneDevices = csma.Install (backboneNodes);

  double wifiX = 0.0;


  for (uint32_t i = 0; i < nWifis; ++i)
    {
      // calculate ssid for wifi subnetwork
      std::ostringstream oss;
      oss << "wifi-default-" << i;
      Ssid ssid = Ssid (oss.str ());

      NodeContainer sta;
      sta.Create (nStas);

      MobilityHelper mobility;
      mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue (wifiX),
                                     "MinY", DoubleValue (0.0),
                                     "DeltaX", DoubleValue (5.0),
                                     "DeltaY", DoubleValue (5.0),
                                     "GridWidth", UintegerValue (1),
                                     "LayoutType", StringValue ("RowFirst"));
      mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
      mobility.Install (backboneNodes.Get (i));

      WifiMacHelper wifiMac;
      wifiMac.SetType ("ns3::ApWifiMac",
                       "Ssid", SsidValue (ssid));

      YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
      wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
      wifiPhy.SetChannel (wifiChannel.Create ());

      NetDeviceContainer apDev;
      WifiHelper wifi;
      wifi.SetStandard(WIFI_STANDARD_80211g);
      apDev = wifi.Install (wifiPhy, wifiMac, backboneNodes.Get (i));

      BridgeHelper bridge;
      NetDeviceContainer bridgeDev;
      bridgeDev = bridge.Install (backboneNodes.Get (i), NetDeviceContainer (apDev, backboneDevices.Get (i)));

      // assign AP IP address to bridge, not wifi
      Ipv4InterfaceContainer apInterface;
      apInterface = ip.Assign (bridgeDev);

      // setup the STAs
      stack.Install (sta);
      mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                 "Mode", StringValue ("Time"),
                                 "Time", StringValue ("0.1s"),
                                 "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=8.0]"),
                                 "Bounds", RectangleValue (Rectangle (wifiX, wifiX + 5.0,0.0, (nStas + 1) * 5.0)));
      mobility.Install (sta);
      wifiMac.SetType ("ns3::StaWifiMac",
                       "Ssid", SsidValue (ssid));

      NetDeviceContainer staDev;
      staDev = wifi.Install (wifiPhy, wifiMac, sta);

      Ipv4InterfaceContainer staInterface;
      staInterface = ip.Assign (staDev);

      // save everything in containers.
      staNodes.push_back (sta);
      apDevices.push_back (apDev);
      apInterfaces.push_back (apInterface);
      staDevices.push_back (staDev);
      staInterfaces.push_back (staInterface);

      wifiX += 20.0;
    }

	  Address dest;
	  std::string protocol;
	  dest = InetSocketAddress (staInterfaces[1].GetAddress (1), 1025);
	  protocol = "ns3::UdpSocketFactory";
	  OnOffHelper onoff = OnOffHelper (protocol, dest);
	  onoff.SetConstantRate (DataRate ("500kb/s"));
	  ApplicationContainer apps = onoff.Install (staNodes[0].Get (0));
	  apps.Start (Seconds (0.5));
	  apps.Stop (Seconds (3.0));

	  for (uint32_t i = 0; i < nPackg; ++i){
		  dest = InetSocketAddress (staInterfaces[rand() % nStas].GetAddress (1), 1025);
		  onoff = OnOffHelper (protocol, dest);
		  apps = onoff.Install (staNodes[rand() % nStas].Get(0));
		  apps.Start (Seconds (0.5));
		  apps.Stop (Seconds (3.0));
	  }


  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();

  //wifiPhy.EnablePcap ("custom", apDevices[0]);
  //wifiPhy.EnablePcap ("custom", apDevices[1]);

  //AsciiTraceHelper ascii;
  //MobilityHelper::EnableAsciiAll (ascii.CreateFileStream ("custom.mob"));

  Simulator::Stop (Seconds (5.0));

  AnimationInterface anim("Custom.xml");
  Simulator::Run ();

  flowMonitor->SerializeToXmlFile("CustomFlow.xml", true, true);

  Simulator::Destroy ();
}
