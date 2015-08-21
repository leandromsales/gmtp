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

	cout << "Creating " << nclients << " clients..." << endl;
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

	cout << "Running simulation..." << endl;

	RunIp(relays, Seconds(2.1), "route");

	RunIp(server, Seconds(2.2), "addr list sim0");
	RunIp(clients, Seconds(2.2), "addr list sim0");

	RunApp("gmtp-server", server, Seconds(4.0), 1 << 31);

	DceApplicationHelper process;
	ApplicationContainer apps;
	process.SetBinary("gmtp-client");
	process.SetStackSize(1 << 16);
	process.ResetArguments();
	process.ParseArguments("10.1.1.2");

	int i = 0;
	int j = 1;
	int k = nclients / 30;
	float t = 5.0;
	float step = 0.5;
	cout << "k = " << k << endl;
	for(; j < k; ++j, t += step) {
		cout << "i = " << i << ", j = " << j << ", t = " << t << endl;
		for(; i < (j * nclients / k); ++i) {
			apps = process.Install(clients.Get(i));
			apps.Start(Seconds(t));
		}
	}

	cout << "i = " << i << ", j = " << j << ", t = " << t << endl;
	t += step;
	for(; i < nclients; ++i) {
		apps = process.Install(clients.Get(i));
		apps.Start(Seconds(t));
	}
	cout << "i = " << i << ", j = " << j << ", t = " << t << endl;

	csma.EnablePcapAll("dce-gmtp-dumbbell");

	AsciiTraceHelper ascii;
	csma.EnableAsciiAll(ascii.CreateFileStream("dce-gmtp-dumbbell.tr"));

	Simulator::Stop (Seconds (1200.0));
	Simulator::Run();
	Simulator::Destroy();

	cout << "Done." << endl;

	return 0;
}

