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
	bool verbose = false;
	std::string core_rate = "100Mbps";
	std::string local_rate = "10Mbps";
	srand(time(NULL));

	std::string delay = "1ms";
	bool middleman = false;

	CommandLine cmd;
	cmd.AddValue ("nclients", "Number of clients in each router", nclients);
	cmd.AddValue ("ncores", "Number of cores in network (except server core)", ncores);
	cmd.AddValue ("nrelays", "Number of relays for each core", nrelays);
	cmd.AddValue ("delay", "Channel delay. Default value is 1ms", delay);
	cmd.AddValue ("verbose", "Print routing details", verbose);
	cmd.AddValue ("middleman", "Middleman intercepting requests", middleman);
	cmd.Parse(argc, argv);

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
	cout << "The simulation has " << all.GetN() << " modes (total).\n" << endl;

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
	dceManager.SetTaskManagerAttribute("FiberManagerType",
			StringValue("UcontextFiberManager"));
	dceManager.SetNetworkStack("ns3::LinuxSocketFdFactory", "Library",
			StringValue("liblinux.so"));

	LinuxStackHelper stack;
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
	NetDeviceContainer dcss = core_csma.Install(subnet_server);

	vector<NetDeviceContainer> dcr(ncores);
	for(int i = 0; i < ncores; i++) {
		dcr[i] = core_csma.Install(vrelays[i]);
	}

	vector<NetDeviceContainer> dcc(relays.GetN());
	for(int i = 0; i < relays.GetN(); ++i) {
		dcc[i] = local_csma.Install(vclients[i]);
	}

	cout << "Create networks and assign IPv4 Addresses.\n" << endl;
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
	uint32_t base_addr = 0xA020000;
	uint32_t netaddr = base_addr;
	vector<Ipv4InterfaceContainer> irelays(ncores);
	for(int i = 0; i < ncores; i++, netaddr += 0x10000) {
		address.SetBase(Ipv4Address(netaddr), "255.255.0.0");
		irelays[i] = address.Assign(dcr[i]);
	}

	//Setting clients addr separated...
	netaddr = base_addr;
	for(int i = 0; i < ncores; ++i, netaddr += 0x10000) {
		uint32_t laddr = netaddr + 0x100;
		vector<Ipv4InterfaceContainer> iclients(relays.GetN());
		for(int j = 0; j < relays.GetN(); ++j, laddr += 0x100) {
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

	if(verbose) {
		int i = 0;
		Ipv4InterfaceContainer::Iterator it;
		for(it = iw.Begin(); it != iw.End(); ++i, ++it) {
			std::pair<Ptr<Ipv4>, uint32_t> pair = *it;
			cout << "Core (files-" << i+2 << "): ";
			for(int j = 0; j < pair.first->GetNInterfaces(); ++j) {
				StringValue data_rate;
				pair.first->GetNetDevice(j)->GetChannel()->GetAttribute("DataRate", data_rate);
				cout << pair.first->GetAddress(j, 0).GetLocal();
				//			cout << " ("<< data_rate.Get() << ")";
				cout << ", ";
			}
			cout << endl;
			int k = i - 1;
			if(k < 0) {
				cout << endl;
				continue;
			}

			Ipv4InterfaceContainer::Iterator itr;
			for(itr = irelays[k].Begin(); itr != irelays[k].End(); ++itr) {
				cout << "\t=> Router: ";
				std::pair<Ptr<Ipv4>, uint32_t> rpair = *itr;
				for(int m = 0; m < rpair.first->GetNInterfaces(); ++m) {
					StringValue data_rate;
					rpair.first->GetNetDevice(m)->GetChannel()->GetAttribute(
							"DataRate", data_rate);
					cout << rpair.first->GetAddress(m, 0).GetLocal();
					//				cout << " (" << data_rate.Get() << ")";
					cout << ", ";
				}
				cout << endl;
				cout << "------------" << endl;
			}
			cout << "------------" << endl;
		}
	}


	if(middleman) {
		uint32_t b_addr = 0xA800000; /* 10.128.0.0 */
		uint32_t naddr = b_addr;
		for(int i=0; i < internet.GetN(); ++i, naddr += 0x10000) {
			Ptr<Node> midc = CreateObject<Node>();
			NodeContainer subnet_mid(internet.Get(i), midc);
			//		NodeContainer subnet_r3(core.Get(0), client_r3);
			stack.Install(midc);
			dceManager.Install(midc);
			NetDeviceContainer snr3 = local_csma.Install(subnet_mid);
			cout << "mid [" << i << "]: " << Ipv4Address(naddr) << endl;
			address.SetBase(Ipv4Address(naddr), "255.255.255.0");
			Ipv4InterfaceContainer ic = address.Assign(snr3);
			RunGtmpInter(midc, Seconds(2.3), "off");
			//		RunApp("gmtp-client", client_r3, Seconds(3.5), "10.1.1.2", 1 << 16);
			/*if(i%2)*/
			if(i==4)
				RunApp("gmtp-client", midc, Seconds(3.5), "10.1.1.2", 1 << 16);
			/*else
				RunApp("gmtp-client", midc, Seconds(4.5), "10.1.1.2", 1 << 16);*/
		}
	}

	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
	LinuxStackHelper::PopulateRoutingTables ();

	cout << "Running GMTP simulation..." << endl;

	RunGtmpInter(server, Seconds(2.0), "off");
//	RunGtmpInter(relays, Seconds(2.0), "off");
	RunIp(all, Seconds(2.1), "route");

	RunIp(server, Seconds(2.2), "addr list sim0");
	RunIp(clients, Seconds(2.2), "addr list sim0");
	RunIp(rserver, Seconds(2.2), "addr list sim0");
	RunIp(rserver, Seconds(2.2), "addr list sim1");

	RunGtmpInter(clients, Seconds(2.3), "off");

	RunApp("gmtp-server", server, Seconds(3.0), 1 << 31);
	/*RunAppMulti("gmtp-client", clients, 4.0, "10.1.1.2", 1 << 16, 3);*/

	ns3::DceApplicationHelper process;
	ns3::ApplicationContainer apps;
	process.SetBinary("gmtp-client");
	process.SetStackSize(1 << 16);
	process.ResetArguments();
	process.ParseArguments("10.1.1.2");

	int i, j;
	double t = 3.5 + (double)(rand()%1000)/10000;
	double step = 0.2;
	cout << "Starting clients at " << t << " secs" << endl;
	//for(i = clients.GetN()-1; i >= 0; --i, t += step) {
	for(i = 0; i < clients.GetN(); ++i, t += step) {
		apps = process.Install(clients.Get(i));
		apps.Start(ns3::Seconds(t));
	}

	local_csma.EnablePcapAll("dce-gmtp-master");
	core_csma.EnablePcapAll("dce-gmtp-master");

	AsciiTraceHelper ascii;
	local_csma.EnableAsciiAll(ascii.CreateFileStream("dce-gmtp-master.tr"));
	core_csma.EnableAsciiAll(ascii.CreateFileStream("dce-gmtp-master.tr"));

	Simulator::Stop (Seconds (12000.0));
	Simulator::Run();
	Simulator::Destroy();

	cout << "Done." << endl;

	return 0;

}
