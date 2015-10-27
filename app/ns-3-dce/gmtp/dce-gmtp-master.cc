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
//
//

int main(int argc, char *argv[])
{
	int nclients = 1;
	int nrelays = 1;
	int ncores = 1;
	std::string local_data_rate = "10Mbps";
	std::string relay_data_rate = "32Mbps";
	std::string local_delay = "1ms";
	std::string relay_delay = "1ms";

	std::string endr = "000000bps";
	std::string nendr = "Mbps";

	CommandLine cmd;
	cmd.AddValue ("ncores", "Number of core routers", ncores);
	cmd.AddValue ("nrelays", "Number relays foreach WAN", nrelays);
	cmd.AddValue ("nclients", "Number of clients foreach LAN", nclients);
	cmd.Parse(argc, argv);

	cout << "Creating nodes..." << endl;

	if(nrelays < 1 || nclients < 1)
		cout << "Error! nrelays and nclients must be greater than 0\n";

	Ptr<Node> server = CreateObject<Node>();
	Ptr<Node> r0 = CreateObject<Node>(); //local router
	Ptr<Node> r1 = CreateObject<Node>(); //core router
	NodeContainer snet(r0, server);
	NodeContainer swan(r1, r0);

	NodeContainer core;
	core.Create(ncores);
	NodeContainer internet(r1, core);

	NodeContainer rx;
	vector<NodeContainer> vrelays(ncores);
	for(int i = 0; i < vrelays.size(); ++i) {
		vrelays[i].Create(nrelays);
		rx.Add(vrelays[i].Get(0));
	}
	NodeContainer relays(r0, rx);

	vector<NodeContainer> wan(ncores);
	for(int i = 0; i < wan.size(); ++i) {
		wan[i] = NodeContainer(core.Get(i), vrelays[i]);
	}

	NodeContainer clients;
	vector<NodeContainer> vclients(nrelays);
	for(int i=0; i < vclients.size(); ++i) {
		vclients[i].Create(nclients);
		clients.Add(vclients[i].Get(0));
	}

	vector<NodeContainer> lan(nrelays);
	for(int i = 0; i < lan.size(); ++i) {
		lan[i] = NodeContainer(rx.Get(i), vclients[i]);
	}

	NodeContainer all(snet, internet, rx, clients);

	DceManagerHelper dceManager;
	dceManager.SetTaskManagerAttribute("FiberManagerType",
			StringValue("UcontextFiberManager"));
	dceManager.SetNetworkStack("ns3::LinuxSocketFdFactory", "Library",
			StringValue("liblinux.so"));

	LinuxStackHelper stack;
	stack.Install(all);
	dceManager.Install(all);

	CsmaHelper local_csma;
	local_csma.SetChannelAttribute("DataRate", StringValue(local_data_rate));
	local_csma.SetChannelAttribute("Delay", StringValue(relay_delay));

	CsmaHelper relay_csma;
	relay_csma.SetChannelAttribute("DataRate", StringValue(relay_data_rate));
	relay_csma.SetChannelAttribute("Delay", StringValue(relay_delay));

//	NetDeviceContainer ds = local_csma.Install(snet);
	NetDeviceContainer ds = relay_csma.Install(snet);
	NetDeviceContainer dsw = relay_csma.Install(swan);
	NetDeviceContainer di = relay_csma.Install(internet);

	vector<NetDeviceContainer> dr(ncores);
	for(int i = 0; i < dr.size(); ++i) {
		dr[i] = relay_csma.Install(wan[i]);
	}
	vector<NetDeviceContainer> dc(nrelays);
	for(int i = 0; i < dc.size(); ++i) {
		dc[i] = local_csma.Install(lan[i]);
	}

	/*NetDeviceContainer dall = local_csma.Install(all);*/

	cout << "Create networks and assign IPv4 Addresses...\n" << endl;
	Ipv4AddressHelper address;
	Ipv4InterfaceContainer is;
	Ipv4InterfaceContainer isw;
	Ipv4InterfaceContainer iint;
	vector<Ipv4InterfaceContainer> irelays(ncores);
	vector<Ipv4InterfaceContainer> iclients(nrelays);

	address.SetBase("10.0.0.0", "255.0.0.0");
	iint = address.Assign(di);

	address.SetBase("10.1.0.0", "255.255.0.0");
	isw = address.Assign(dsw);

	address.SetBase("10.1.1.0", "255.255.255.0");
	is = address.Assign(ds);

	/*
	 * To calculate the decimal address from a dotted string, perform the following calculation.
	 * (first octet * 256³) + (second octet * 256²) + (third octet * 256) + (fourth octet)
	 *
	 * 10.2.0.0 = 0x0A020000
	 */
	uint32_t netaddr = 0xA020000;
	Ipv4Mask wmask(0xFFFF0000); //255.255.0.0
	for(int i = 0; i < irelays.size(); ++i, netaddr += 0x10000) {
		address.SetBase(Ipv4Address(netaddr), wmask);
		irelays[i] = address.Assign(dr[i]);

		cout << "NAddr: " << Ipv4Address(netaddr) << endl;
		uint32_t laddr = netaddr + 0x100;
		cout << "LAddr: " << Ipv4Address(laddr) << endl;
		Ipv4Mask lmask(0xFFFFFF00); //255.255.255.0
		for(int i=0; i < iclients.size(); ++i, netaddr+=0x100) {
			address.SetBase(Ipv4Address(laddr), lmask);
			iclients[i] = address.Assign(dc[i]);
		}
	}

	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
	LinuxStackHelper::PopulateRoutingTables ();

	cout << "Total nodes: " << all.GetN() << endl << endl;
	cout << "Server (files-0): " << is.GetAddress(1, 0) << endl;
	cout << "Relay  (files-1): " << is.GetAddress(0, 0) << ", lan(" << isw.GetAddress(1, 0) << ")" << endl;
	cout << "Router (files-2): wan(" << iint.GetAddress(0, 0) << "), lan(" << isw.GetAddress(0, 0) << ")" << endl << endl;

	int i = 0;
	Ipv4InterfaceContainer::Iterator it;
	for(it = iint.Begin(); it != iint.End(); ++i, ++it) {
		std::pair<Ptr<Ipv4>, uint32_t> pair = *it;
		cout << "Core (files-" << i + 2 << "): ";
		for(int j = 0; j < pair.first->GetNInterfaces(); ++j) {
			StringValue data_rate;
			pair.first->GetNetDevice(j)->GetChannel()->GetAttribute("DataRate", data_rate);
			cout << pair.first->GetAddress(j, 0).GetLocal() << " (" << data_rate.Get().replace(2, 10, nendr) << "), ";
		}
		cout << endl;
		int k = i - 1;
		if(k < 0) {
			cout << endl;
			continue;
		}

		int w = 0;
		Ipv4InterfaceContainer::Iterator itr;
		for(itr = irelays[k].Begin(); itr != irelays[k].End(); ++itr, ++w) {
			if(w == 0)
				continue;

			cout << "\t=> Router: ";
			std::pair<Ptr<Ipv4>, uint32_t> rpair = *itr;
			for(int m = 0; m < rpair.first->GetNInterfaces(); ++m) {
				StringValue data_rate;
				rpair.first->GetNetDevice(m)->GetChannel()->GetAttribute("DataRate", data_rate);
				cout << rpair.first->GetAddress(m, 0).GetLocal() << " (" <<  data_rate.Get().replace(2, 10, nendr) << "), ";
			}
			cout << endl;

			int z = 0;
			Ipv4InterfaceContainer::Iterator itc;
			for(itc = iclients[k].Begin(); itc != iclients[k].End(); ++itc, ++z) {
				if(z == 0)
					continue; //Jump first node (relay)
				StringValue data_rate;
				std::pair<Ptr<Ipv4>, uint32_t> cpair = *itc;
				cpair.first->GetNetDevice(0)->GetChannel()->GetAttribute( "DataRate", data_rate);
				cout << "\t\t> Client: " << cpair.first->GetAddress(0, 0).GetLocal() << " (" << data_rate.Get().replace(2, 10, nendr) << ")" << endl;
			}
			cout << "------------" << endl;
		}
		cout << "------------" << endl;
	}


	cout << "Configuring simulation details..." << endl << endl;

	RunGtmpInter(server, Seconds(2.0), "off");
	RunIp(all, Seconds(2.1), "route");

	RunIp(internet, Seconds(2.2), "addr list sim0");
	RunIp(internet, Seconds(2.2), "addr list sim1");
	RunIp(server, Seconds(2.2), "addr list sim0");
	RunIp(clients, Seconds(2.2), "addr list sim0");

	RunGtmpInter(clients, Seconds(2.3), "off");

//	RunApp("tcp-server", server, Seconds(4.0), 1 << 31);
//	RunApp("tcp-client", clients.Get(0), Seconds(5.0), "10.1.1.2", 1 << 16);

	RunApp("gmtp-server", server, Seconds(3.0), 1 << 31);
//
//	cout << "We have " << clients.GetN() << " clients." << endl;
//	for(int i=0; i < clients.GetN(); ++i) {
//		RunApp("gmtp-client", clients.Get(i), Seconds(5.0), "10.1.1.2", 1 << 16);
//	}
	RunApp("gmtp-client", clients.Get(0), Seconds(5.0), "10.1.1.2", 1 << 16);
//	RunAppMulti("gmtp-client", clients, 5.0, "10.1.1.2", 1 << 16, 30);

	relay_csma.EnablePcapAll("dce-gmtp-master");

	AsciiTraceHelper ascii;
	relay_csma.EnableAsciiAll(ascii.CreateFileStream("dce-gmtp-master.tr"));

	cout << "\nRunning simulation..." << endl;
	Simulator::Stop (Seconds (12000.0));
	Simulator::Run();
	Simulator::Destroy();

	cout << "Done." << endl;

	return 0;
}

