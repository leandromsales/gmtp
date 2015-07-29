#include "ns3/network-module.h"
#include "ns3/core-module.h"
#include "ns3/dce-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

#include "dce-gmtp.h"

// Network topology
// //
//  c0					  s
//     \ 5 Mb/s, 2ms			 /
//      \          1.5Mb/s, 10ms	/
//       r0 --------------------------r1
//      /
//     / 5 Mb/s, 2ms
//   c1
// //
using namespace ns3;
using namespace std;

int main(int argc, char *argv[])
{
	CommandLine cmd;
	cmd.Parse(argc, argv);

	cout << "Creating nodes..." << endl;
	Ptr<Node> server = CreateObject<Node>();
	Ptr<Node> relay = CreateObject<Node>();
	Ptr<Node> client = CreateObject<Node>();

	NodeContainer net1(relay, server);
	NodeContainer net2(relay, client);
	NodeContainer all(relay, server, client);

	DceManagerHelper dceManager;
	dceManager.SetTaskManagerAttribute("FiberManagerType",
			StringValue("UcontextFiberManager"));
	dceManager.SetNetworkStack("ns3::LinuxSocketFdFactory", "Library",
			StringValue("liblinux.so"));
	LinuxStackHelper stack;
	stack.Install(all);
	dceManager.Install(all);

	CsmaHelper csma;
	csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
	csma.SetChannelAttribute("Delay", StringValue("2ms"));

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


	for(int n = 0; n < 2; n++) {
		RunIp(all.Get(n), Seconds(2), "link show");
		RunIp(all.Get(n), Seconds(2.1), "route show table all");
		RunIp(all.Get(n), Seconds(2.2), "addr list");
	}

	DceApplicationHelper dce;
	ApplicationContainer apps;

	dce.SetBinary("gmtp-server");
	dce.SetStackSize(1 << 16);
	dce.ResetArguments();
	apps = dce.Install(server);
	apps.Start(Seconds(5.0));

	dce.SetBinary("gmtp-client");
	dce.SetStackSize(1 << 16);
	dce.ResetArguments();
	dce.AddArgument("10.1.1.2");
	apps = dce.Install(client);
	apps.Start(Seconds(7.0));

	csma.EnablePcapAll("dce-gmtp-dumbbel");

	AsciiTraceHelper ascii;
	csma.EnableAsciiAll(ascii.CreateFileStream("dce-gmtp-dumbbel.tr"));

	Simulator::Stop(Seconds(30.0));
	Simulator::Run();
	Simulator::Destroy();

	cout << "Done." << endl;

	return 0;
}

