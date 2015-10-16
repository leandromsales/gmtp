#include "ns3/network-module.h"
#include "ns3/core-module.h"
#include "ns3/dce-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

#include "dce-gmtp.h"

// Network topology
// //
// //             s    r    c0	c1
// //             |    _    |	|
// //             ====|_|========
// //                router
// //
using namespace ns3;
using namespace std;

int main(int argc, char *argv[])
{
	int nclients = 1;

	CommandLine cmd;
	cmd.AddValue ("nclients", "Number of clients in simulation", nclients);
	cmd.Parse(argc, argv);

	cout << "Creating server and relay nodes..." << endl;
	Ptr<Node> server = CreateObject<Node>();
	Ptr<Node> relay = CreateObject<Node>();

	cout << "Creating " << nclients << " clients..." << endl;
	NodeContainer clients;
	clients.Create (nclients);

	NodeContainer net1(relay, server);
	NodeContainer net2(relay, clients);
	NodeContainer all(relay, server, clients);

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

	cout << "Create networks and assign IPv4 Addresses." << endl;
	Ipv4AddressHelper address;
	address.SetBase("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer i1 = address.Assign(d1);

	address.SetBase("10.1.2.0", "255.255.255.0");
	Ipv4InterfaceContainer i2 = address.Assign(d2);

	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
	LinuxStackHelper::PopulateRoutingTables ();

	cout << "Running simulation..." << endl;

	RunIp(relay, Seconds(2.1), "route");
	RunIp(server, Seconds(2.2), "addr list sim0");
	RunIp(clients, Seconds(2.2), "addr list sim0");
	RunGtmpInter(clients, Seconds(2.3), "off");

	RunApp("gmtp-server", server, Seconds(4.0), 1 << 31);
	RunAppMulti("gmtp-client", clients, 5.0, "10.1.1.2", 1 << 16, 30);

	csma.EnablePcapAll("dce-gmtp-relay");

	AsciiTraceHelper ascii;
	csma.EnableAsciiAll(ascii.CreateFileStream("dce-gmtp-relay.tr"));

	Simulator::Stop (Seconds (1200.0));
	Simulator::Run();
	Simulator::Destroy();

	cout << "Done." << endl;

	return 0;
}


