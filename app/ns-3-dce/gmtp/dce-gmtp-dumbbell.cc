#include "ns3/network-module.h"
#include "ns3/core-module.h"
#include "ns3/dce-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

#include "dce-gmtp.h"

// Network topology
// //
//  c0					  s
//     \ 10 Mb/s, 1ms			 /
//      \          10Mb/s, 1ms		/
//       r1 --------------------------r0
//      /
//     / 10 Mb/s, 1ms
//   c1
//   ...
//   cn
// //
using namespace ns3;
using namespace std;

int main(int argc, char *argv[])
{
	int nclients = 1;

	CommandLine cmd;
	cmd.AddValue ("nclients", "Number of clients in router 1 (far from server)", nclients);
	cmd.Parse(argc, argv);

	cout << "Creating nodes..." << endl;
	Ptr<Node> server = CreateObject<Node>();

	NodeContainer relays;
	relays.Create (2);

	NodeContainer clients;
	clients.Create (nclients);

	NodeContainer net1(relays.Get(0), server);
	NodeContainer net2(relays.Get(1), clients);
	NodeContainer all(server, relays, clients);

	DceManagerHelper dceManager;
	dceManager.SetTaskManagerAttribute("FiberManagerType",
			StringValue("UcontextFiberManager"));
	dceManager.SetNetworkStack("ns3::LinuxSocketFdFactory", "Library",
			StringValue("liblinux.so"));
	LinuxStackHelper stack;
	stack.Install(all);
	dceManager.Install(all);

	CsmaHelper csma;
	csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
	csma.SetChannelAttribute("Delay", StringValue("1ms"));

	NetDeviceContainer d1 = csma.Install(net1);
	NetDeviceContainer d2 = csma.Install(net2);
	NetDeviceContainer r = csma.Install(relays);

	cout << "Create networks and assign IPv4 Addresses." << endl;
	Ipv4AddressHelper address;
	address.SetBase("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer i1 = address.Assign(d1);

	address.SetBase("10.1.2.0", "255.255.255.0");
	Ipv4InterfaceContainer i2 = address.Assign(d2);

	address.SetBase("10.1.4.0", "255.255.255.0");
	Ipv4InterfaceContainer i3 = address.Assign(r);

	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
	LinuxStackHelper::PopulateRoutingTables ();

	RunIp(all, Seconds(2.2), "addr list sim0");
	RunIp(relays, Seconds(2.3), "addr list sim1");

	RunGtmpInter(server, Seconds(2.5), "off");
	RunGtmpInter(clients, Seconds(2.5), "off");

	RunApp("gmtp-server", server, Seconds(4.0));

//	float j = 5.0;
//	for(int i = 0; i < nclients; ++i, j += 0.1)
//		RunApp("gmtp-client", clients.Get(i), Seconds(j), "10.1.1.2");
	RunApp("gmtp-client", clients, Seconds(5.0), "10.1.1.2");

	csma.EnablePcapAll("dce-gmtp-dumbbell");

	AsciiTraceHelper ascii;
	csma.EnableAsciiAll(ascii.CreateFileStream("dce-gmtp-dumbbell.tr"));

	Simulator::Stop (Seconds (1200.0));
	Simulator::Run();
	Simulator::Destroy();

	cout << "Done." << endl;

	return 0;
}

