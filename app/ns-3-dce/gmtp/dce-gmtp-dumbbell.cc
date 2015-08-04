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
// //
using namespace ns3;
using namespace std;

int main(int argc, char *argv[])
{
	CommandLine cmd;
	cmd.Parse(argc, argv);

	int nclients = 1;
	int nrelays = 2;

	cout << "Creating nodes..." << endl;
	Ptr<Node> server = CreateObject<Node>();

	NodeContainer relays;
	relays.Create (nrelays);

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

	address.SetBase("10.1.3.0", "255.255.255.0");
	Ipv4InterfaceContainer i3 = address.Assign(r);

	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
	LinuxStackHelper::PopulateRoutingTables ();

	RunIp(server, Seconds(2.0), "addr list sim0");
	for(int n = 0; n < nclients; n++)
		RunIp(clients.Get(n), Seconds(2.1), "addr list sim0");

	for(int n = 0; n < nrelays; n++) {
		RunIp(relays.Get(n), Seconds(2.2), "addr list sim0");
		RunIp(relays.Get(n), Seconds(2.3), "addr list sim1");
	}
//	RunGtmpInter(relays.Get(0), Seconds(2.5), "off");
//	RunGtmpInter(relays.Get(1), Seconds(2.6), "off");

	DceApplicationHelper dce;
	ApplicationContainer apps;

	dce.SetBinary("gmtp-server");
//	dce.SetBinary("tcp-server");
	dce.SetStackSize(1 << 31);
	dce.ResetArguments();
	apps = dce.Install(server);
	apps.Start(Seconds(4.0));

	dce.SetBinary("gmtp-client");
//	dce.SetBinary("tcp-client");
	dce.SetStackSize(1 << 16);
	dce.ResetArguments();
	dce.AddArgument("10.1.1.2");
	apps = dce.Install(clients);
	apps.Start(Seconds(7.0));

	csma.EnablePcapAll("dce-gmtp-dumbbell");

	AsciiTraceHelper ascii;
	csma.EnableAsciiAll(ascii.CreateFileStream("dce-gmtp-dumbbell.tr"));

	Simulator::Stop (Seconds (1200.0));
	Simulator::Run();
	Simulator::Destroy();

	cout << "Done." << endl;

	return 0;
}

