#include "ns3/network-module.h"
#include "ns3/core-module.h"
#include "ns3/dce-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

#include "dce-gmtp.h"

using namespace ns3;

int main(int argc, char *argv[])
{
	CommandLine cmd;
	cmd.Parse(argc, argv);

	NodeContainer nodes;
	nodes.Create(2);

	CsmaHelper csma;
	csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
	csma.SetChannelAttribute("Delay", StringValue("1ms"));
	NetDeviceContainer devices = csma.Install(nodes);

	DceManagerHelper dceManager;
	dceManager.SetTaskManagerAttribute("FiberManagerType",
			StringValue("UcontextFiberManager"));
	dceManager.SetNetworkStack("ns3::LinuxSocketFdFactory", "Library",
			StringValue("liblinux.so"));

	LinuxStackHelper stack;
	stack.Install(nodes);
	Ipv4AddressHelper address;
	address.SetBase("10.0.0.0", "255.255.255.0");
	Ipv4InterfaceContainer interfaces = address.Assign(devices);
	dceManager.Install(nodes);

	RunGtmpInter(nodes, Seconds(2.0), "off");
//	RunApp("gmtp-server", nodes.Get(0), Seconds(2.5), 1 << 16);
	RunApp("gmtp-server-multi", nodes.Get(0), Seconds(2.5), 1 << 16);
	RunApp("gmtp-client", nodes.Get(1), Seconds(3.5), "10.0.0.1", 1 << 16);
//	RunApp("gmtp-client", nodes.Get(1), Seconds(4.0), "10.0.0.1", 1 << 16);

	csma.EnablePcapAll("dce-gmtp-simple");

	Simulator::Stop(Seconds(1200.0));
	Simulator::Run();
	Simulator::Destroy();

	return 0;
}
