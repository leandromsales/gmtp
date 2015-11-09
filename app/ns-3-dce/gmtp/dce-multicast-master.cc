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

enum protocol {
	UDP,
	TCP
};


int main(int argc, char *argv[])
{
	int nclients = 1;
	int ncores = 1;
	int nrelays = 1;
	std::string core_rate = "64Mbps";
	std::string local_rate = "10Mbps";
	std::string delay = "1ms";
	int proton = UDP;

	CommandLine cmd;
	cmd.AddValue ("nclients", "Number of clients in each router", nclients);
	cmd.AddValue ("ncores", "Number of cores in network (except server core)", ncores);
	cmd.AddValue ("nrelays", "Number of relays for each core", nrelays);
	cmd.AddValue ("delay", "Channel delay. Default value is 1ms", delay);
	cmd.AddValue ("proto", "Protocol of simulation (UDP=0, TCP=1)", proton);
	cmd.Parse(argc, argv);

	enum protocol proto = (enum protocol) proton;

	cout << "Creating nodes..." << endl;

	/* Server */
	Ptr<Node> server = CreateObject<Node>();
	Ptr<Node> rserver = CreateObject<Node>();
	NodeContainer subnet_server(rserver, server);

	NodeContainer internet;
	internet.Create(1);

	NodeContainer net_server(internet.Get(0), rserver);

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
	cout << "The simulation has " << all.GetN() << " nodes (total).\n" << endl;

	cout << "Core routers: ";
	for(int i=0; i < internet.GetN(); ++i)
		cout << "files-" << internet.Get(i)->GetId() << ", ";
	cout << endl << "Relays: ";
	for(int i=0; i<relays.GetN(); ++i)
		cout << "files-" << relays.Get(i)->GetId() << ", ";
	cout << endl << "Clients: ";
	for(int i=0; i<clients.GetN(); ++i)
		cout << "files-" << clients.Get(i)->GetId() << ", ";
	cout << endl;

	DceManagerHelper dceManager;
//	dceManager.SetNetworkStack("ns3::LinuxSocketFdFactory",
//				"Library", StringValue("liblinux.so"));
//	LinuxStackHelper stack;
	InternetStackHelper stack;

	stack.Install(all);
	dceManager.Install(all);

	CsmaHelper core_csma;
	core_csma.SetChannelAttribute("DataRate", StringValue(core_rate));
	core_csma.SetChannelAttribute("Delay", StringValue(delay));

	CsmaHelper local_csma;
	local_csma.SetChannelAttribute("DataRate", StringValue(local_rate));
	local_csma.SetChannelAttribute("Delay", StringValue(delay));

	NetDeviceContainer dci = core_csma.Install(internet);
	NetDeviceContainer dcs = core_csma.Install(net_server);
	NetDeviceContainer dcss = local_csma.Install(subnet_server);

	vector<NetDeviceContainer> dcr(ncores);
	for(int i = 0, j = 0; i < ncores; ++i) {
		dcr[i] = core_csma.Install(vrelays[i]);
	}

	vector<NetDeviceContainer> dcc(relays.GetN());
	for(int i = 0; i < relays.GetN(); ++i) {
		dcc[i] = local_csma.Install(vclients[i]);
	}

	cout << "Create networks and assign IPv4 Addresses.\n" << endl;
	Ipv4AddressHelper address;

	address.SetBase("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer iss = address.Assign(dcss);

	address.SetBase("10.1.0.0", "255.255.0.0");
	Ipv4InterfaceContainer is = address.Assign(dcs);

	address.SetBase("10.0.0.0", "255.0.0.0");
	Ipv4InterfaceContainer iw = address.Assign(dci);

	/*
	 * To calculate the decimal address from a dotted string, perform the following calculation.
	 * (first octet * 256³) + (second octet * 256²) + (third octet * 256) + (fourth octet)
	 *
	 * 10.2.0.0 = 0x0A020000
	 */
	uint32_t base_addr = 0xA020000;
	uint32_t netaddr = base_addr;
	vector<Ipv4InterfaceContainer> irelays(ncores);
	for(int i = 0; i < ncores; ++i, netaddr += 0x10000) {
		address.SetBase(Ipv4Address(netaddr), "255.255.0.0");
		irelays[i] = address.Assign(dcr[i]);
	}

	//Setting clients addr separated...
	netaddr = base_addr;
	for(int i = 0; i < ncores; ++i, netaddr += 0x10000) {
		uint32_t laddr = netaddr + 0x100;
		vector<Ipv4InterfaceContainer> iclients(relays.GetN());
		for(int j = 0; j < relays.GetN(); ++j, laddr += 0x100) {
//			cout << "lan [" << i << "][" << j << "]: " << Ipv4Address(laddr) << endl << "\t> ";
			address.SetBase(Ipv4Address(laddr), "255.255.255.0");
			iclients[j] = address.Assign(dcc[j]);

			Ipv4InterfaceContainer::Iterator itc;
			for(itc = iclients[j].Begin(); itc != iclients[j].End(); ++itc) {
				std::pair<Ptr<Ipv4>, uint32_t> cpair = *itc;
			}
		}
	}

	// Just printing IPs
	cout << "Server (files-0): " << iss.GetAddress(1, 0) << endl;
	cout << "Relay  (files-1): " << iss.GetAddress(0, 0) << ", lan(" << is.GetAddress(1, 0) << ")" << endl;
	cout << "Router (files-2): wan(" << iw.GetAddress(0, 0) << "), lan(" << is.GetAddress(0, 0) << ")" << endl << endl;

	if(proto == UDP) {

		cout << "Routing UDP..." << endl;

		// Now we can configure multicasting.  As described above, the multicast
		// source is at node zero, which we assigned the IP address of 10.1.1.1
		// earlier.  We need to define a multicast group to send packets to.  This
		// can be any multicast address from 224.0.0.0 through 239.255.255.255
		// (avoiding the reserved routing protocol addresses).
		//
		Ipv4Address multicastSource ("10.1.1.2");
		Ipv4Address multicastGroup ("225.1.2.4");

		// Now, we will set up multicast routing.  We need to do three things:
		// 1) Configure a (static) multicast route on node n2
		// 2) Set up a default multicast route on the sender n0
		// 3) Have node n4 join the multicast group
		// We have a helper that can help us with static multicast
		Ipv4StaticRoutingHelper multicast;

		// 1) Configure a (static) multicast route on node n2 (multicastRouter)
//		Ptr<Node> multicastRouter = rserver;  // The node in question
//		Ptr<NetDevice> inputIf = dcss.Get(0);  // The input NetDevice
//		NetDeviceContainer outputDevices;  // A container of output NetDevices
//		outputDevices.Add(dcs.Get(1));  // (we only need one NetDevice here)

		//multicast.AddMulticastRoute(multicastRouter, multicastSource,
		//		multicastGroup, inputIf, outputDevices);

		cout << "Adding route from rserver to internet(0)..." << endl;
		multicast.AddMulticastRoute(rserver, multicastSource,
				multicastGroup, dcss.Get(0), dcs.Get(1));

		cout << "Adding route from internet(0) to internet..." << endl;
		multicast.AddMulticastRoute(internet.Get(0), multicastSource,
					multicastGroup, dcs.Get(0), dci.Get(0));

		cout << "Adding route from internet to routers..." << endl;
		for(int i=1; i < internet.GetN(); ++i) {
			multicast.AddMulticastRoute(internet.Get(i),
					multicastSource, multicastGroup,
					dci.Get(i), internet.Get(i)->GetDevice(2));
		}

		cout << "Adding route from routers to clients..." << endl;
		for(int j = 0; j < relays.GetN(); ++j) {
			multicast.AddMulticastRoute(relays.Get(j),
					multicastSource, multicastGroup,
					relays.Get(j)->GetDevice(1),
					relays.Get(j)->GetDevice(2));
		}

		// 2) Set up a default multicast route on the sender (server)
		Ptr<NetDevice> serverIf = dcss.Get(1);
		multicast.SetDefaultMulticastRoute (server, serverIf);
	}

	Ipv4GlobalRoutingHelper::PopulateRoutingTables();
	//LinuxStackHelper::PopulateRoutingTables();

	Ipv4GlobalRoutingHelper g;
	Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>
	("routes.routes", std::ios::out);
	 g.PrintRoutingTableAllAt (Seconds (3.0), routingStream);

	cout << "Running ";

	switch(proto) {
	case UDP:
		cout << "UDP simulation..." << endl;
		RunApp("udp-mcst-server", server, Seconds(5.0), "10.1.1.2", 1 << 31);
		RunAppMulti("udp-mcst-client", clients, 4.5, "sim0", 1 << 16, 30);
		break;
	case TCP:
		cout << "TCP simulation..." << endl;
		RunApp("tcp-server", server, Seconds(4.0), 1 << 31);
		RunAppMulti("tcp-client", /*clients.Get(0)*/internet.Get(0), 5.0, "10.1.1.2", 1 << 16, 30);
//		RunAppMulti("tcp-client", clients, 5.0, "10.1.1.2", 1 << 16, 30);
		break;
	}


	local_csma.EnablePcapAll("dce-multicast-master");
	core_csma.EnablePcapAll("dce-multicast-master");

	AsciiTraceHelper ascii;
	local_csma.EnableAsciiAll(ascii.CreateFileStream("dce-multicast-master.tr"));
	core_csma.EnableAsciiAll(ascii.CreateFileStream("dce-multicast-master.tr"));

	Simulator::Stop (Seconds (12000.0));
	Simulator::Run();
	Simulator::Destroy();

	cout << "Done." << endl;

	return 0;

}
