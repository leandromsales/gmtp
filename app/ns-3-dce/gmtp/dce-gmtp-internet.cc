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
	int nclients0 = 1;
	int nclients1 = 1;

	CommandLine cmd;
	cmd.AddValue ("nclients0", "Number of clients in router 0 (next to server)", nclients0);
	cmd.AddValue ("nclients1", "Number of clients in router 1 (far from server)", nclients1);
	cmd.Parse(argc, argv);

	cout << "Creating nodes..." << endl;
	Ptr<Node> server = CreateObject<Node>();

	NodeContainer relays;
	relays.Create (5);

	NodeContainer clients1;
	clients1.Create (nclients0);

	NodeContainer clients2;
	clients2.Create (nclients1);

	NodeContainer clients(clients1, clients2);
	NodeContainer all(server, relays, clients);

	NodeContainer subnet1(relays.Get(1), clients1);
	NodeContainer subnet2(relays.Get(2), clients2);
	NodeContainer net_clients(relays.Get(3), relays.Get(1), relays.Get(2));
	NodeContainer subnet_server(relays.Get(0), server);
	NodeContainer net_server(relays.Get(4), relays.Get(0));
	NodeContainer net(relays.Get(3), relays.Get(4));

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

	NetDeviceContainer sn1 = csma.Install(subnet1);
	NetDeviceContainer sn2 = csma.Install(subnet2);
	NetDeviceContainer nc = csma.Install(net_clients);
	NetDeviceContainer sns = csma.Install(subnet_server);
	NetDeviceContainer ns = csma.Install(net_server);
	NetDeviceContainer n = csma.Install(net);

	cout << "Create networks and assign IPv4 Addresses." << endl;
	Ipv4AddressHelper address;

	vector<Ipv4InterfaceContainer> ivector(6);

	address.SetBase("10.0.0.0", "255.0.0.0");
	ivector[0] = address.Assign(n);

	address.SetBase("10.1.0.0", "255.255.0.0");
	ivector[1] = address.Assign(ns);
	address.SetBase("10.1.1.0", "255.255.255.0");
	ivector[2] = address.Assign(sns);

	address.SetBase("10.2.0.0", "255.255.0.0");
	ivector[3] = address.Assign(nc);
	address.SetBase("10.2.1.0", "255.255.255.0");
	ivector[4] = address.Assign(sn1);
	address.SetBase("10.2.2.0", "255.255.255.0");
	ivector[5] = address.Assign(sn2);

	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
	LinuxStackHelper::PopulateRoutingTables ();

	for(int i = 0; i < ivector.size(); ++i) {
		cout << "net " << i << ":" << endl;
		Ipv4InterfaceContainer::Iterator it;
		for(it = ivector[i].Begin(); it != ivector[i].End(); ++it) {
			std::pair<Ptr<Ipv4>, uint32_t> pair = *it;
			cout << pair.first->GetAddress(0, 0) << endl;
		}
		cout << "------------" << endl;
	}

	cout << "Running simulation..." << endl;

//	RunGtmpInter(relays.Get(0), Seconds(2.0), "off");
	RunIp(relays, Seconds(2.1), "route");

	RunIp(server, Seconds(2.2), "addr list sim0");
	RunIp(clients, Seconds(2.2), "addr list sim0");

	RunGtmpInter(clients, Seconds(2.3), "off");

	RunApp("gmtp-server", server, Seconds(4.0), 1 << 31);
	RunAppMulti("gmtp-client", clients, 5.0, "10.1.1.2", 1 << 16, 30);

	csma.EnablePcapAll("dce-gmtp-internet");

	AsciiTraceHelper ascii;
	csma.EnableAsciiAll(ascii.CreateFileStream("dce-gmtp-internet.tr"));

	Simulator::Stop (Seconds (12000.0));
	Simulator::Run();
	Simulator::Destroy();

	cout << "Done." << endl;

	return 0;
}

