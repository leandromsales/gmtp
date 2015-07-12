#include "ns3/network-module.h"
#include "ns3/core-module.h"
#include "ns3/dce-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

// Network topology
// //
// //             n0   r    n1
// //             |    _    |
// //             ====|_|====
// //                router
// //
using namespace ns3;
using namespace std;

int main(int argc, char *argv[])
{
	CommandLine cmd;
	cmd.Parse(argc, argv);

	cout << "Create nodes." << endl;
	Ptr<Node> n0 = CreateObject<Node>();
	Ptr<Node> r = CreateObject<Node>();
	Ptr<Node> n1 = CreateObject<Node>();

	NodeContainer net1(r, n0);
	NodeContainer net2(r, n1);
	NodeContainer all(r, n0, n1);

	DceManagerHelper dceManager;
	dceManager.SetTaskManagerAttribute("FiberManagerType",
			StringValue("UcontextFiberManager"));
	dceManager.SetNetworkStack("ns3::LinuxSocketFdFactory", "Library",
			StringValue("liblinux.so"));
	LinuxStackHelper stack;
	stack.Install(all);

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

	// It does not work in DCE
	//Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

	dceManager.Install(all);

	DceApplicationHelper dce;
	ApplicationContainer apps;

	//  dce.SetBinary ("gmtp-inter");
	//  dce.SetStackSize (1 << 16);
	//  dce.ResetArguments ();
	//  dce.ParseArguments ("off");
	//  apps = dce.Install (r);
	//  apps.Start (Seconds (2.0));

	dce.SetBinary("ip");
	dce.SetStackSize(1 << 16);
	dce.ResetArguments();
	dce.ParseArguments("route add default via 10.1.1.1 dev sim0");
	apps = dce.Install(n0);
	apps.Start(Seconds(2.5));

	dce.ResetArguments();
	dce.ParseArguments("route add default via 10.1.2.1 dev sim0");
	apps = dce.Install(n1);
	apps.Start(Seconds(2.5));

	dce.ResetArguments();
	dce.ParseArguments("route");
	apps = dce.Install(all);
	apps.Start(Seconds(3.0));

	dce.ResetArguments();
	dce.ParseArguments("link set sim0 up");
	apps = dce.Install(all);
	apps.Start(Seconds(3.5));

	dce.ResetArguments();
	dce.ParseArguments("link set sim1 up");
	apps = dce.Install(r);
	apps.Start(Seconds(3.6));

	dce.ResetArguments();
	dce.ParseArguments("addr show dev sim0");
	apps = dce.Install(all);
	apps.Start(Seconds(3.9));

	dce.ResetArguments();
	dce.ParseArguments("addr show dev sim1");
	apps = dce.Install(r);
	apps.Start(Seconds(4.0));

	dce.SetBinary("gmtp-server");
	dce.SetStackSize(1 << 16);
	dce.ResetArguments();
	apps = dce.Install(n0);
	apps.Start(Seconds(5.0));

	dce.SetBinary("gmtp-client");
	dce.SetStackSize(1 << 16);
	dce.ResetArguments();
	dce.AddArgument("10.1.1.2");
	apps = dce.Install(n1);
	apps.Start(Seconds(7.0));

	csma.EnablePcapAll("dce-gmtp-relay");

	AsciiTraceHelper ascii;
	csma.EnableAsciiAll(ascii.CreateFileStream("dce-gmtp.tr"));

	Simulator::Stop(Seconds(30.0));
	Simulator::Run();
	Simulator::Destroy();

	cout << "Done." << endl;

	return 0;
}

