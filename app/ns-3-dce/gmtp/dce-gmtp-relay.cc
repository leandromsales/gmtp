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
	CommandLine cmd;
	cmd.Parse(argc, argv);

	int nclients = 1;

	cout << "Creating nodes..." << endl;
	Ptr<Node> server = CreateObject<Node>();
	Ptr<Node> relay = CreateObject<Node>();

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

	for(int n = 0; n < 2+nclients; n++) {
//		RunIp(all.Get(n), Seconds(2), "link show");
		RunIp(all.Get(n), Seconds(2.1), "route show table all");
		RunIp(all.Get(n), Seconds(2.2), "addr list sim0");
	}

//	RunGtmpInter(relay, Seconds(3.0), "off");
	RunIp(relay, Seconds(2.3), "addr list sim1");

	DceApplicationHelper dce;
	ApplicationContainer apps;

	dce.SetBinary("gmtp-server");
	dce.SetStackSize(1 << 31);
	dce.ResetArguments();
	apps = dce.Install(server);
	apps.Start(Seconds(4.0));

	dce.SetBinary("gmtp-client");
	dce.SetStackSize(1 << 16);
	dce.ResetArguments();
	dce.AddArgument("10.1.1.2");
	apps = dce.Install(clients);
	apps.Start(Seconds(5));


	csma.EnablePcapAll("dce-gmtp-relay");

	AsciiTraceHelper ascii;
	csma.EnableAsciiAll(ascii.CreateFileStream("dce-gmtp-relay.tr"));

	Simulator::Stop (Seconds (1200.0));
	Simulator::Run();
	Simulator::Destroy();

	cout << "Done." << endl;

	return 0;
}

