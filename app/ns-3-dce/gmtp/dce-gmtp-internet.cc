#include "ns3/network-module.h"
#include "ns3/core-module.h"
#include "ns3/dce-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

#include "dce-gmtp.h"

using namespace ns3;
using namespace std;

// Network topology
// //
//			     s
//			    /
//			   /
//  c1.0		  r0		c2.0
//     \ 		 /		 /
//      \          	/		/
//       r1-----r4-----r3------r5------r2
//      /		|		\
//     /  	       (c)		 \
//   c1.2				 c2.2
//    ..				  ..
//   c1.n				 c2.m
// //

int main(int argc, char *argv[])
{
	int nclients1 = 1;
	int nclients2 = 1;
	std::string data_rate = "10Mbps";
	std::string delay = "1ms";

	CommandLine cmd;
	cmd.AddValue ("nclients1", "Number of clients in router 1", nclients1);
	cmd.AddValue ("nclients2", "Number of clients in router 2", nclients2);
	cmd.AddValue ("data-rate", "Link capacity. Default value is 10Mbps", data_rate);
	cmd.AddValue ("delay", "Channel delay. Default value is 1ms", delay);
	cmd.Parse(argc, argv);

	cout << "Creating nodes..." << endl;

	Ptr<Node> server = CreateObject<Node>();

	NodeContainer relays;
	relays.Create (6);

	NodeContainer clients1;
	clients1.Create (nclients1);

	NodeContainer clients2;
	clients2.Create (nclients2);

	NodeContainer clients(clients1, clients2);
	NodeContainer all(server, relays, clients);

	NodeContainer internet(relays.Get(3), relays.Get(4), relays.Get(5));

	NodeContainer net_server(relays.Get(3), relays.Get(0));
	NodeContainer subnet_server(relays.Get(0), server);

	NodeContainer net_clients1(relays.Get(1), relays.Get(4));
	NodeContainer subnet_clients1(relays.Get(1), clients1);

	NodeContainer net_clients2(relays.Get(2), relays.Get(5));
	NodeContainer subnet_clients2(relays.Get(2), clients2);

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

	NetDeviceContainer iw = csma.Install(internet);
	NetDeviceContainer ns = csma.Install(net_server);
	NetDeviceContainer sns = csma.Install(subnet_server);
	NetDeviceContainer nc1 = csma.Install(net_clients1);
	NetDeviceContainer snc1 = csma.Install(subnet_clients1);
	NetDeviceContainer nc2 = csma.Install(net_clients2);
	NetDeviceContainer snc2 = csma.Install(subnet_clients2);

	cout << "Create networks and assign IPv4 Addresses." << endl;
	Ipv4AddressHelper address;

	vector<Ipv4InterfaceContainer> ivector(8);

	address.SetBase("10.0.0.0", "255.0.0.0");
	ivector[0] = address.Assign(iw);

	address.SetBase("10.1.0.0", "255.255.0.0");
	ivector[1] = address.Assign(ns);
	address.SetBase("10.1.1.0", "255.255.255.0");
	ivector[2] = address.Assign(sns);

	address.SetBase("10.2.0.0", "255.255.0.0");
	ivector[3] = address.Assign(nc1);
	address.SetBase("10.2.1.0", "255.255.255.0");
	ivector[4] = address.Assign(snc1);

	address.SetBase("10.3.0.0", "255.255.0.0");
	ivector[5] = address.Assign(nc2);
	address.SetBase("10.3.1.0", "255.255.255.0");
	ivector[6] = address.Assign(snc2);

	Ptr<Node> client_r3 = CreateObject<Node>();
	clients.Add(client_r3);
	NodeContainer subnet_r3(relays.Get(3), client_r3);
	stack.Install(client_r3);
	dceManager.Install(client_r3);
	NetDeviceContainer snr3 = csma.Install(subnet_r3);
	address.SetBase("10.1.2.0", "255.255.255.0");
	Ipv4InterfaceContainer ic = address.Assign(snr3);
	RunApp("gmtp-client", client_r3, Seconds(4.5), "10.1.1.2", 1 << 31);

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

	RunGtmpInter(server, Seconds(2.0), "off");
	RunIp(all, Seconds(2.1), "route");

	RunIp(server, Seconds(2.2), "addr list sim0");
	RunIp(clients, Seconds(2.2), "addr list sim0");

	RunGtmpInter(clients, Seconds(2.3), "off");

//	RunApp("tcp-server", server, Seconds(4.0), 1 << 31);
//	RunAppMulti("tcp-client", clients.Get(0), 5.0, "10.1.1.2", 1 << 16, 30);

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

