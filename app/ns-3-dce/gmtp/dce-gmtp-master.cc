#include "ns3/network-module.h"
#include "ns3/core-module.h"
#include "ns3/dce-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/attribute.h"

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
	int nclients = 1;
	int ncores = 1;
	int nrelays = 1;
	bool middleman = false;
	std::string data_rate = "100Mbps";
	std::string delay = "1ms";

	CommandLine cmd;
	cmd.AddValue ("nclients", "Number of clients in each router", nclients);
	cmd.AddValue ("ncores", "Number of cores in network (except server core)", ncores);
	//cmd.AddValue ("middleman", "Middleman intercepting requests", middleman);
	cmd.AddValue ("data-rate", "Link capacity. Default value is 10Mbps", data_rate);
	cmd.AddValue ("delay", "Channel delay. Default value is 1ms", delay);
	cmd.Parse(argc, argv);

	cout << "Creating nodes..." << endl;

	/* Server */
	Ptr<Node> server = CreateObject<Node>();
	Ptr<Node> rserver = CreateObject<Node>();

	NodeContainer internet;
	internet.Create(1);

	NodeContainer net_server(internet.Get(0), rserver);
	NodeContainer subnet_server(rserver, server);

	/* Internet Core */
	NodeContainer core;
	core.Create(ncores);
	internet.Add(core);
	cout << "The Internet core has " << internet.GetN() << " nodes." << endl;

	NodeContainer relays;
	vector<NodeContainer> vrelays(ncores);
	for(int i = 0; i < ncores; ++i) {
		vrelays[i].Create(nrelays);
		for(int j = 0; j < nrelays; ++j)
			relays.Add(vrelays[i].Get(j));
		vrelays[i].Add(core.Get(i));
	}
	cout << "The simulation has " << relays.GetN() << " relays." << endl;

	NodeContainer clients;
	vector<NodeContainer> vclients(relays.GetN());
	for(int i = 0; i < relays.GetN(); ++i) {
		vclients[i].Create(nclients);
		for(int j = 0; j < nclients; ++j)
			clients.Add(vclients[i].Get(j));
		vclients[i].Add(relays.Get(i));
	}
	cout << "The simulation has " << clients.GetN() << " clients." << endl;

	NodeContainer all(subnet_server, internet, relays, clients);
	cout << "The simulation has " << all.GetN() << " modes (total).\n" << endl;

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

	NetDeviceContainer dci = csma.Install(internet);
	NetDeviceContainer dcs = csma.Install(net_server);
	NetDeviceContainer dcss = csma.Install(subnet_server);

	vector<NetDeviceContainer> dcr(ncores);
	for(int i = 0; i < ncores; ++i) {
		dcr[i] = csma.Install(vrelays[i]);
	}

	vector<NetDeviceContainer> dcc(relays.GetN());
	for(int i = 0; i < relays.GetN(); ++i) {
		dcc[i] = csma.Install(vclients[i]);
	}

	cout << "Create networks and assign IPv4 Addresses." << endl;
	Ipv4AddressHelper address;

	address.SetBase("10.0.0.0", "255.0.0.0");
	Ipv4InterfaceContainer iw = address.Assign(dci);

	address.SetBase("10.1.0.0", "255.255.0.0");
	Ipv4InterfaceContainer is = address.Assign(dcs);
	address.SetBase("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer iss = address.Assign(dcss);

	/*
	 * To calculate the decimal address from a dotted string, perform the following calculation.
	 * (first octet * 256³) + (second octet * 256²) + (third octet * 256) + (fourth octet)
	 *
	 * 10.2.0.0 = 0x0A020000
	 */
	uint32_t netaddr = 0xA020000;
	Ipv4Mask wmask(0xFFFF0000); //255.255.0.0
	vector<Ipv4InterfaceContainer> irelays(ncores);
	for(int i = 0; i < ncores; ++i, netaddr += 0x10000) {
		address.SetBase(Ipv4Address(netaddr), wmask);
		irelays[i] = address.Assign(dcr[i]);

		uint32_t laddr = netaddr + 0x100;
		Ipv4Mask lmask(0xFFFFFF00); //255.255.255.0
		vector<Ipv4InterfaceContainer> iclients(relays.GetN());
		for(int j = 0; j < relays.GetN(); ++j, laddr += 0x100) {
			cout << "Network Addr (" << i << "): " << Ipv4Address(netaddr) << endl;
			cout << "Local Addr (" << j << "): "   << Ipv4Address(laddr) << endl;
			address.SetBase(Ipv4Address(laddr), lmask);
			iclients[j] = address.Assign(dcc[j]);
		}
	}

	if(middleman) {
		cout << "Starting new relay here...\n";
		Ptr<Node> client_r3 = CreateObject<Node>();
		NodeContainer subnet_r3(internet.Get(0), client_r3);
		stack.Install(client_r3);
		dceManager.Install(client_r3);
		NetDeviceContainer snr3 = csma.Install(subnet_r3);
		address.SetBase("10.1.2.0", "255.255.255.0");
		Ipv4InterfaceContainer ic = address.Assign(snr3);
		RunGtmpInter(client_r3, Seconds(2.3), "off");
//		RunApp("gmtp-client", client_r3, Seconds(3.5), "10.1.1.2", 1 << 31);
		RunApp("gmtp-client", client_r3, Seconds(6.5), "10.1.1.2", 1 << 31);
	}

	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
	LinuxStackHelper::PopulateRoutingTables ();

	cout << "Running simulation..." << endl;

	RunGtmpInter(server, Seconds(2.0), "off");
	RunIp(all, Seconds(2.1), "route");

	RunIp(server, Seconds(2.2), "addr list sim0");
	RunIp(clients, Seconds(2.2), "addr list sim0");

	RunGtmpInter(clients, Seconds(2.3), "off");

//	RunApp("tcp-server", server, Seconds(4.0), 1 << 31);
//	RunAppMulti("tcp-client", clients.Get(0), 5.0, "10.1.1.2", 1 << 16, 30);

	RunApp("gmtp-server", server, Seconds(3.0), 1 << 31);
//	RunAppMulti("gmtp-client", clients, 5.0, "10.1.1.2", 1 << 16, 30);
	RunApp("gmtp-client", clients.Get(1), Seconds(5.0), "10.1.1.2", 1 << 16);

	csma.EnablePcapAll("dce-gmtp-master");

	AsciiTraceHelper ascii;
	csma.EnableAsciiAll(ascii.CreateFileStream("dce-gmtp-master.tr"));

	Simulator::Stop (Seconds (12000.0));
	Simulator::Run();
	Simulator::Destroy();

	cout << "Done." << endl;

	return 0;

}
