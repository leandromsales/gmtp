#include "ns3/network-module.h"
#include "ns3/core-module.h"
#include "ns3/dce-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

#include "dce-gmtp.h"

using namespace ns3;
using namespace std;

// Network topology
//
//

int main(int argc, char *argv[])
{
	int nclients = 1;
	int nrelays = 1;
	std::string data_rate = "10Mbps";
	std::string delay = "1ms";

	CommandLine cmd;
	cmd.AddValue ("nrelays", "Number of clients in router 1", nrelays);
	cmd.AddValue ("nclients", "Number of clients in router 1", nclients);
	cmd.AddValue ("data-rate", "Link capacity. Default value is 10Mbps", data_rate);
	cmd.AddValue ("delay", "Channel delay. Default value is 1ms", delay);
	cmd.Parse(argc, argv);

	cout << "Creating nodes..." << endl;

	if(nrelays < 1 || nclients < 1)
		cout << "Error! nrelays and nclients must be greater than 0\n";

	Ptr<Node> server = CreateObject<Node>();

	NodeContainer relays;
	relays.Create (nrelays);

	NodeContainer clients;
	clients.Create (nclients);

	NodeContainer net1(relays, server);
	NodeContainer net2(relays, clients);
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
	csma.SetChannelAttribute("DataRate", StringValue(data_rate));
	csma.SetChannelAttribute("Delay", StringValue(delay));

	NetDeviceContainer d1 = csma.Install(net1);
	NetDeviceContainer d2 = csma.Install(net2);
	NetDeviceContainer dall = csma.Install(all);

	cout << "Create networks and assign IPv4 Addresses." << endl;
	Ipv4AddressHelper address;
	vector<Ipv4InterfaceContainer> ivector(3);

	address.SetBase("10.1.1.0", "255.255.255.0");
	ivector[0] = address.Assign(d1);

	address.SetBase("10.1.2.0", "255.255.255.0");
	ivector[1] = address.Assign(d2);

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

	cout << "Configuring simulation details..." << endl;

	RunGtmpInter(server, Seconds(2.0), "off");
	RunIp(all, Seconds(2.1), "route");

	RunIp(server, Seconds(2.2), "addr list sim0");
	RunIp(clients, Seconds(2.2), "addr list sim0");

	RunGtmpInter(clients, Seconds(2.3), "off");

//	RunApp("tcp-server", server, Seconds(4.0), 1 << 31);
//	RunAppMulti("tcp-client", clients.Get(0), 5.0, "10.1.1.2", 1 << 16, 30);

	RunApp("gmtp-server", server, Seconds(3.0), 1 << 31);
	RunAppMulti("gmtp-client", clients, 5.0, "10.1.1.2", 1 << 16, 30);

	csma.EnablePcapAll("dce-gmtp-master");

	AsciiTraceHelper ascii;
	csma.EnableAsciiAll(ascii.CreateFileStream("dce-gmtp-master.tr"));

	cout << "Running simulation..." << endl;
	Simulator::Stop (Seconds (12000.0));
	Simulator::Run();
	Simulator::Destroy();

	cout << "Done." << endl;

	return 0;
}

