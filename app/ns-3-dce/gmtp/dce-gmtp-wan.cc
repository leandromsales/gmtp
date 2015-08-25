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
	relays.Create (3);

	NodeContainer clients0;
	clients0.Create (nclients0);

	NodeContainer clients1;
	clients1.Create (nclients1);

	NodeContainer netserver(relays.Get(0), server);
	NodeContainer netrelay0(relays.Get(0), relays.Get(1));
	NodeContainer netrelay1(relays.Get(0), relays.Get(2));
	NodeContainer net0(relays.Get(1), clients0);
	NodeContainer net1(relays.Get(2), clients1);
	NodeContainer clients(clients0, clients1);
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

	NetDeviceContainer ds = csma.Install(netserver);
	NetDeviceContainer dr0 = csma.Install(netrelay0);
	NetDeviceContainer dr1 = csma.Install(netrelay1);
	NetDeviceContainer d0 = csma.Install(net0);
	NetDeviceContainer d1 = csma.Install(net1);

	cout << "Create networks and assign IPv4 Addresses." << endl;
	Ipv4AddressHelper address;

	vector<Ipv4InterfaceContainer> ivector(5);

	address.SetBase("10.1.1.0", "255.255.255.0");
	ivector[0] = address.Assign(ds);

	address.SetBase("10.1.2.0", "255.255.255.0");
	ivector[1] = address.Assign(dr0);

	address.SetBase("10.1.3.0", "255.255.255.0");
	ivector[2] = address.Assign(dr1);

	address.SetBase("10.1.4.0", "255.255.255.0");
	ivector[3] = address.Assign(d0);

	address.SetBase("10.1.5.0", "255.255.255.0");
	ivector[4] = address.Assign(d1);

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

	RunGtmpInter(clients, Seconds(2.5), "off");

	RunApp("gmtp-server", server, Seconds(4.0), 1 << 31);
	RunApp("gmtp-client", clients, Seconds(5.0), "10.1.1.2", 1 << 31);

	csma.EnablePcapAll("dce-gmtp-dumbbell");

	AsciiTraceHelper ascii;
	csma.EnableAsciiAll(ascii.CreateFileStream("dce-gmtp-dumbbell.tr"));

	Simulator::Stop (Seconds (8000.0));
	Simulator::Run();
	Simulator::Destroy();

	cout << "Done." << endl;

	return 0;
}

