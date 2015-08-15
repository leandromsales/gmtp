#include "ns3/network-module.h"
#include "ns3/core-module.h"
#include "ns3/dce-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

#include "dce-gmtp.h"

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

	NodeContainer clients1;
	clients1.Create (nclients);

	NodeContainer clients2;
	clients2.Create (nclients);

	NodeContainer relays;
	relays.Create (2);

	NodeContainer net0(relays.Get(0), server);
	NodeContainer net1(relays.Get(0), clients1);
	NodeContainer net2(relays.Get(1), clients2);
	NodeContainer clients(clients1, clients2);
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

	NetDeviceContainer d0 = csma.Install(net0);
	NetDeviceContainer d1 = csma.Install(net1);
	NetDeviceContainer d2 = csma.Install(net2);
	NetDeviceContainer r = csma.Install(relays);

	cout << "Create networks and assign IPv4 Addresses." << endl;
	Ipv4AddressHelper address;

	address.SetBase("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer i0 = address.Assign(d0);

	address.SetBase("10.1.2.0", "255.255.255.0");
	Ipv4InterfaceContainer i1 = address.Assign(d1);

	address.SetBase("10.1.3.0", "255.255.255.0");
	Ipv4InterfaceContainer i2 = address.Assign(d2);

	address.SetBase("10.1.4.0", "255.255.255.0");
	Ipv4InterfaceContainer i3 = address.Assign(r);

	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
	LinuxStackHelper::PopulateRoutingTables ();

//	RunGtmpInter(relays.Get(0), Seconds(2.0), "off");
	RunIp(relays, Seconds(2.1), "route");

	RunIp(server, Seconds(2.2), "addr list sim0");
	RunIp(clients, Seconds(2.2), "addr list sim0");

	RunGtmpInter(server, Seconds(2.5), "off");
	RunGtmpInter(clients, Seconds(2.5), "off");

//	RunApp("gmtp-server", server, Seconds(4.0), 1 << 31);
//	RunApp("gmtp-server-multi", server, Seconds(4.0), 1 << 31);
	RunApp("gmtp-server-select", server, Seconds(4.0), 1 << 31);

//	RunApp("gmtp-client", clients, Seconds(5.0), "10.1.1.2", 1 << 16);
	RunApp("gmtp-client-select", clients, Seconds(5.0), "10.1.1.2", 1 << 16);
//	RunApp("gmtp-client", clients2, Seconds(5.0), "10.1.1.2", 1 << 16);

	csma.EnablePcapAll("dce-gmtp-dumbbell");

	AsciiTraceHelper ascii;
	csma.EnableAsciiAll(ascii.CreateFileStream("dce-gmtp-dumbbell.tr"));

	Simulator::Stop (Seconds (1200.0));
	Simulator::Run();
	Simulator::Destroy();

	cout << "Done." << endl;

	return 0;
}

