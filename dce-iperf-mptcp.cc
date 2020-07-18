#include "ns3/network-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/dce-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/constant-position-mobility-model.h"

using namespace ns3;
using namespace std;
NS_LOG_COMPONENT_DEFINE ("DceIperfMptcp");
uint32_t nRtrs =2;
NodeContainer nodes, routers;


void
PrintTcpFlags (std::string key, std::string value)
{
  NS_LOG_INFO (key << "=" << value);
}


void setPos (Ptr<Node> n, int x, int y, int z)
{
  Ptr<ConstantPositionMobilityModel> loc = CreateObject<ConstantPositionMobilityModel> ();
  n->AggregateObject (loc);
  Vector locVec2 (x, y, z);
  loc->SetPosition (locVec2);
}


void BandwidthTrace(){
  	int t;
  	t = Simulator::Now ().GetSeconds ();
	NS_LOG_INFO("Changing the of one path delay @"<<t);
        if(nRtrs<=1){
		//Config::Set ("/NodeList/1/DeviceList/0/$ns3::PointToPointNetDevice/DataRate",StringValue ("5Mbps"));//This works
                //Config::Set ("/NodeList/2/DeviceList/0/$ns3::PointToPointNetDevice/DataRate",StringValue ("5Mbps")); //This works
		Config::Set("/ChannelList/0/$ns3::PointToPointChannel/Delay",StringValue ("150ms"));//This works
        }else{
		/*if(t%2==0)
			Config::Set ("/NodeList/3/DeviceList/0/$ns3::PointToPointNetDevice/DataRate",StringValue ("5Mbps"));//This works
		else if(t%2!=0)
			Config::Set ("/NodeList/3/DeviceList/0/$ns3::PointToPointNetDevice/DataRate",StringValue ("100Mbps"));//This works*/
                //Config::Set ("/NodeList/2/DeviceList/0/$ns3::PointToPointNetDevice/DataRate",StringValue ("5Mbps"));//This works
                //Config::Set ("/NodeList/3/DeviceList/0/$ns3::PointToPointNetDevice/DataRate",StringValue ("5Mbps"));//This works
                //Config::Set ("/NodeList/1/DeviceList/0/$ns3::PointToPointNetDevice/DataRate",StringValue ("5Mbps"));//This works
                //Config::Set ("/NodeList/1/DeviceList/1/$ns3::PointToPointNetDevice/DataRate",StringValue ("5Mbps"));//This works
		/*if(t%2==0)
			Config::Set("/ChannelList/0/$ns3::PointToPointChannel/Delay",StringValue ("100ms"));//This works
		else if(t%2!=0)
			Config::Set("/ChannelList/0/$ns3::PointToPointChannel/Delay",StringValue ("10ms"));//This works
		*/
		switch(t){
			case 10:Config::Set("/ChannelList/0/$ns3::PointToPointChannel/Delay",StringValue ("500ms"));
				//Config::Set ("/NodeList/2/DeviceList/0/$ns3::PointToPointNetDevice/DataRate",StringValue ("5Mbps"));
				//Config::Set ("/NodeList/0/DeviceList/0/$ns3::PointToPointNetDevice/DataRate",StringValue ("5Mbps"));
				//Config::Set ("/NodeList/2/DeviceList/0/$ns3::PointToPointNetDevice/Delay",StringValue ("500ms"));
				//Config::Set ("/NodeList/0/DeviceList/0/$ns3::PointToPointNetDevice/Delay",StringValue ("500ms"));
				break;
			case 20:Config::Set("/ChannelList/0/$ns3::PointToPointChannel/Delay",StringValue ("11ms"));
				Config::Set ("/NodeList/2/DeviceList/0/$ns3::PointToPointNetDevice/DataRate",StringValue ("50Mbps"));
				break;
			case 30:Config::Set("/ChannelList/0/$ns3::PointToPointChannel/Delay",StringValue ("150ms"));
				Config::Set ("/NodeList/2/DeviceList/0/$ns3::PointToPointNetDevice/DataRate",StringValue ("5Mbps"));
				break;
			case 40:Config::Set("/ChannelList/0/$ns3::PointToPointChannel/Delay",StringValue ("11ms"));
				Config::Set ("/NodeList/2/DeviceList/0/$ns3::PointToPointNetDevice/DataRate",StringValue ("50Mbps"));
				break;
			case 50:Config::Set("/ChannelList/0/$ns3::PointToPointChannel/Delay",StringValue ("150ms"));
				Config::Set ("/NodeList/2/DeviceList/0/$ns3::PointToPointNetDevice/DataRate",StringValue ("5Mbps"));
				break;
			case 60:Config::Set("/ChannelList/0/$ns3::PointToPointChannel/Delay",StringValue ("11ms"));
				Config::Set ("/NodeList/2/DeviceList/0/$ns3::PointToPointNetDevice/DataRate",StringValue ("50Mbps"));
				break;
			case 70:Config::Set("/ChannelList/0/$ns3::PointToPointChannel/Delay",StringValue ("150ms"));
				Config::Set ("/NodeList/2/DeviceList/0/$ns3::PointToPointNetDevice/DataRate",StringValue ("5Mbps"));
				break;
			case 80:Config::Set("/ChannelList/0/$ns3::PointToPointChannel/Delay",StringValue ("11ms"));
				Config::Set ("/NodeList/2/DeviceList/0/$ns3::PointToPointNetDevice/DataRate",StringValue ("50Mbps"));
				break;
			case 90:Config::Set("/ChannelList/0/$ns3::PointToPointChannel/Delay",StringValue ("150ms"));
				Config::Set ("/NodeList/2/DeviceList/0/$ns3::PointToPointNetDevice/DataRate",StringValue ("5Mbps"));
				break;
			case 100:Config::Set("/ChannelList/0/$ns3::PointToPointChannel/Delay",StringValue ("11ms"));
				Config::Set ("/NodeList/2/DeviceList/0/$ns3::PointToPointNetDevice/DataRate",StringValue ("50Mbps"));
				break;
		}	
		//Config::Set("/ChannelList/2/$ns3::PointToPointChannel/Delay",StringValue ("150ms"));//This works
  	}

}
int main (int argc, char *argv[])
{
  string delay;
  LogComponentEnable ("DceIperfMptcp", LOG_LEVEL_ALL);
  CommandLine cmd;
  cmd.AddValue ("nRtrs", "Number of routers. Default 2", nRtrs);
  cmd.Parse (argc, argv);

  nodes.Create (2);
  routers.Create (nRtrs);
 
  mkdir ("files-0", 0700);
  mkdir ("files-0/tmp", 0700);
  mkdir ("files-1", 0700);
  mkdir ("files-1/tmp", 0700);
  setenv("TMPDIR","files-0/tmp",1);
  DceManagerHelper dceManager;
  dceManager.SetTaskManagerAttribute ("FiberManagerType",
                                      StringValue ("UcontextFiberManager"));

  dceManager.SetNetworkStack ("ns3::LinuxSocketFdFactory",
                              "Library", StringValue ("liblinux.so"));
  LinuxStackHelper stack;
  stack.Install (nodes);
  stack.Install (routers);

  dceManager.Install (nodes);
  dceManager.Install (routers);

  PointToPointHelper pointToPoint;
  NetDeviceContainer devices1, devices2;
  Ipv4AddressHelper address1, address2;
  std::ostringstream cmd_oss;
  address1.SetBase ("10.1.0.0", "255.255.255.0");
  address2.SetBase ("10.2.0.0", "255.255.255.0");

  for (uint32_t i = 0; i < nRtrs; i++)
    {
      // Left link
      if(i==0){
      	pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("20Mbps"));
      	pointToPoint.SetChannelAttribute ("Delay", StringValue ("10ms"));
      	devices1 = pointToPoint.Install (nodes.Get (0), routers.Get (i));
      }else{
	pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("20Mbps"));
        pointToPoint.SetChannelAttribute ("Delay", StringValue ("20ms"));
        devices1 = pointToPoint.Install (nodes.Get (0), routers.Get (i));
	}
      // Assign ip addresses
      Ipv4InterfaceContainer if1 = address1.Assign (devices1);
      address1.NewNetwork ();
      // setup ip routes
      cmd_oss.str ("");
      cmd_oss << "rule add from " << if1.GetAddress (0, 0) << " table " << (i+1);
      LinuxStackHelper::RunIp (nodes.Get (0), Seconds (0.1), cmd_oss.str ().c_str ());
      cmd_oss.str ("");
      cmd_oss << "route add 10.1." << i << ".0/24 dev sim" << i << " scope link table " << (i+1);
      LinuxStackHelper::RunIp (nodes.Get (0), Seconds (0.1), cmd_oss.str ().c_str ());
      cmd_oss.str ("");
      cmd_oss << "route add default via " << if1.GetAddress (1, 0) << " dev sim" << i << " table " << (i+1);
      LinuxStackHelper::RunIp (nodes.Get (0), Seconds (0.1), cmd_oss.str ().c_str ());
      cmd_oss.str ("");
      cmd_oss << "route add 10.1."<< i <<".0/24 via " << if1.GetAddress (1, 0) << " dev sim0";
      LinuxStackHelper::RunIp (routers.Get (i), Seconds (0.2), cmd_oss.str ().c_str ());

      // Right link
      pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
      pointToPoint.SetChannelAttribute ("Delay", StringValue ("1ns"));
      devices2 = pointToPoint.Install (nodes.Get (1), routers.Get (i));
      // Assign ip addresses
      Ipv4InterfaceContainer if2 = address2.Assign (devices2);
      address2.NewNetwork ();
      // setup ip routes
      cmd_oss.str ("");
      cmd_oss << "rule add from " << if2.GetAddress (0, 0) << " table " << (i+1);
      LinuxStackHelper::RunIp (nodes.Get (1), Seconds (0.1), cmd_oss.str ().c_str ());
      cmd_oss.str ("");
      cmd_oss << "route add 10.2." << i << ".0/24 dev sim" << i << " scope link table " << (i+1);
      LinuxStackHelper::RunIp (nodes.Get (1), Seconds (0.1), cmd_oss.str ().c_str ());
      cmd_oss.str ("");
      cmd_oss << "route add default via " << if2.GetAddress (1, 0) << " dev sim" << i << " table " << (i+1);
      LinuxStackHelper::RunIp (nodes.Get (1), Seconds (0.1), cmd_oss.str ().c_str ());
      cmd_oss.str ("");
      cmd_oss << "route add 10.2."<< i <<".0/24 via " << if2.GetAddress (1, 0) << " dev sim1";
      LinuxStackHelper::RunIp (routers.Get (i), Seconds (0.2), cmd_oss.str ().c_str ());

      setPos (routers.Get (i), 50, i * 20, 0);
    }

  // default route
  LinuxStackHelper::RunIp (nodes.Get (0), Seconds (0.1), "route add default via 10.1.0.2 dev sim0");
  LinuxStackHelper::RunIp (nodes.Get (1), Seconds (0.1), "route add default via 10.2.0.2 dev sim0");
  LinuxStackHelper::RunIp (nodes.Get (0), Seconds (0.1), "rule show");

  // Schedule Up/Down (XXX: didn't work...)
  LinuxStackHelper::RunIp (nodes.Get (1), Seconds (1.0), "link set dev sim0 multipath off");
  LinuxStackHelper::RunIp (nodes.Get (1), Seconds (15.0), "link set dev sim0 multipath on");
  LinuxStackHelper::RunIp (nodes.Get (1), Seconds (30.0), "link set dev sim0 multipath off");
  


  // debug
  stack.SysctlSet (nodes, ".net.mptcp.mptcp_debug", "1");

  stack.SysctlSet (nodes, ".net.ipv4.tcp_rmem","4096	8291456	8291456 ");
  //stack.SysctlSet (nodes, ".net.ipv4.tcp_rmem","4096 5999 100000 ");
  stack.SysctlSet (nodes, ".net.ipv4.tcp_wmem","4096	8291456  8291456 ");
  //stack.SysctlSet (nodes, ".net.ipv4.tcp_wmem","4096 5999 100000");
  stack.SysctlSet (nodes, ".net.core.rmem_max","8291456");
  stack.SysctlSet (nodes, ".net.core.wmem_max","8291456");
  stack.SysctlSet (nodes, ".net.ipv4.tcp_congestion_control","cubic");
  stack.SysctlSet (nodes, ".mptcp_sandy_mptcp","1");
  //stack.SysctlSet (nodes, ".net.mptcp.mptcp_scheduler","roundrobin");
  //stack.SysctlSet (nodes, ".net.mptcp.mptcp_scheduler","redundant");
  stack.SysctlSet (nodes, ".net.mptcp.mptcp_scheduler","default");
  //stack.SysctlSet (nodes, ".net.mptcp.mptcp_scheduler","ecf");
  //stack.SysctlSet (nodes, ".net.mptcp.mptcp_scheduler","blest");


  LinuxStackHelper::SysctlGet (nodes.Get(0), Seconds (5),".net.ipv4.tcp_congestion_control", &PrintTcpFlags);
  LinuxStackHelper::SysctlGet (nodes.Get(1), Seconds (5),".net.ipv4.tcp_congestion_control", &PrintTcpFlags);
  //LinuxStackHelper::SysctlGet (nodes.Get(0), Seconds (5),".mptcp_sandy_mptcp", &PrintTcpFlags);
  //LinuxStackHelper::SysctlGet (nodes.Get(1), Seconds (5),".mptcp_sandy_mptcp", &PrintTcpFlags);
  LinuxStackHelper::SysctlGet (nodes.Get(0), Seconds (5),".net.mptcp.mptcp_scheduler", &PrintTcpFlags);
  LinuxStackHelper::SysctlGet (nodes.Get(1), Seconds (5),".net.mptcp.mptcp_scheduler", &PrintTcpFlags);
  LinuxStackHelper::SysctlGet (nodes.Get(1), Seconds (5),".net.ipv4.tcp_moderate_rcvbuf", &PrintTcpFlags);
  


  DceApplicationHelper dce;
  ApplicationContainer apps;

  dce.SetStackSize (1 << 20);

  // Launch iperf client on node 0
  dce.SetBinary ("iperf3");
  dce.ResetArguments ();
  dce.ResetEnvironment ();
  dce.AddArgument ("-V");
  dce.AddArgument ("-c");
  dce.AddArgument ("10.2.0.1");
  dce.AddArgument ("-R");
  dce.AddArgument ("-t");
  //dce.AddArgument ("140");
  dce.AddArgument ("30");
  dce.AddArgument ("-p");
  dce.AddArgument ("1");

  apps = dce.Install (nodes.Get (0));
  apps.Start (Seconds (5.0));
  apps.Stop (Seconds (200));

  // Launch iperf server on node 1
  dce.SetBinary ("iperf3");
  dce.ResetArguments ();
  dce.ResetEnvironment ();
  dce.AddArgument ("-s");
  dce.AddArgument ("-p");
  dce.AddArgument ("1");
  apps = dce.Install (nodes.Get (1));

  pointToPoint.EnablePcapAll ("iperf-mptcp", false);

  apps.Start (Seconds (4));

  setPos (nodes.Get (0), 0, 20 * (nRtrs - 1) / 2, 0);
  setPos (nodes.Get (1), 100, 20 * (nRtrs - 1) / 2, 0);

  //dynamic rate and delay modifications
  for (int i=10;i<=100;i+=10){
  	//Simulator::Schedule (Seconds(i) , &BandwidthTrace);	
   }

  Simulator::Stop (Seconds (310.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
