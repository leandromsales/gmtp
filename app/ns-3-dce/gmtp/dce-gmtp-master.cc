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
	std::string local_data_rate = "10Mbps";
	std::string relay_data_rate = "332Mbps";
	std::string local_delay = "1ms";
	std::string relay_delay = "1ms";

	std::string endr = "000000bps";
	std::string nendr = "Mbps";

	CommandLine cmd;
	cmd.AddValue ("nrelays", "Number of clients in router 1", nrelays);
	cmd.AddValue ("nclients", "Number of clients in router 1", nclients);
	cmd.Parse(argc, argv);

	cout << "Creating nodes..." << endl;

	if(nrelays < 1 || nclients < 1)
		cout << "Error! nrelays and nclients must be greater than 0\n";

	Ptr<Node> server = CreateObject<Node>();
	Ptr<Node> r0 = CreateObject<Node>();
	NodeContainer snet(r0, server);

	NodeContainer rx;
	rx.Create(nrelays);
	NodeContainer relays(r0, rx);

	NodeContainer clients;
	vector<NodeContainer> vclients(nrelays);
	for(int i=0; i < vclients.size(); ++i) {
		vclients[i].Create(nclients);
		clients.Add(vclients[i].Get(0));
	}

	vector<NodeContainer> net(nrelays);
	for(int i = 0; i < net.size(); ++i) {
		net[i] = NodeContainer(rx.Get(i), vclients[i]);
	}

	NodeContainer all(snet, rx, clients);

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
	NetDeviceContainer dr = relay_csma.Install(relays);
	vector<NetDeviceContainer> dc(nrelays);
	for(int i = 0; i < dc.size(); ++i) {
		dc[i] = local_csma.Install(net[i]);
	}

	/*NetDeviceContainer dall = local_csma.Install(all);*/

	cout << "Create networks and assign IPv4 Addresses...\n" << endl;
	Ipv4AddressHelper address;
	Ipv4InterfaceContainer iserver;
	Ipv4InterfaceContainer irelays;
	vector<Ipv4InterfaceContainer> iclients(nrelays);

	address.SetBase("10.1.1.0", "255.255.255.0");
	iserver = address.Assign(ds);

	address.SetBase("10.2.0.0", "255.255.0.0");
	irelays = address.Assign(dr);

	/*
	 * To calculate the decimal address from a dotted string, perform the following calculation.
	 * (first octet * 256³) + (second octet * 256²) + (third octet * 256) + (fourth octet)
	 *
	 * 10.3.1.0 = 0x0A030100
	 */
	uint32_t netaddr = 0xA030300;
	Ipv4Mask mask(0xFFFFFF00); //255.255.255.0
	for(int i=0; i < iclients.size(); ++i, netaddr+=0x100) {
		address.SetBase(Ipv4Address(netaddr), mask);
		iclients[i] = address.Assign(dc[i]);
	}

	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
	LinuxStackHelper::PopulateRoutingTables ();

	cout << "Total nodes: " << all.GetN() << endl << endl;
	cout << "S (files-0): " << iserver.GetAddress(1, 0) << endl;

	int i;
	Ipv4InterfaceContainer::Iterator itr;
	for(i = 0, itr = irelays.Begin(); itr != irelays.End(); ++i, ++itr) {
		std::pair<Ptr<Ipv4>, uint32_t> rpair = *itr;
		cout << "R (files-" << i+1 << "): ";
		for(int j = 0; j < rpair.first->GetNInterfaces(); ++j) {
			StringValue data_rate;
			rpair.first->GetNetDevice(j)->GetChannel()->GetAttribute("DataRate", data_rate);
			cout << rpair.first->GetAddress(j, 0).GetLocal() << " (" <<  data_rate.Get()/*.replace(2, 10, nendr)*/ << "), ";
		}
		cout << endl;
		int k = i - 1;
		if(k < 0) { cout << endl; continue; }

		int w = 0;
		Ipv4InterfaceContainer::Iterator itc;
		for(itc = iclients[k].Begin(); itc != iclients[k].End(); ++itc, ++w) {
			if(w == 0) continue; //Jump first node (relay)
			StringValue data_rate;
			std::pair<Ptr<Ipv4>, uint32_t> cpair = *itc;
			cpair.first->GetNetDevice(0)->GetChannel()->GetAttribute("DataRate", data_rate);
			cout << "\t" << cpair.first->GetAddress(0, 0).GetLocal() << " (" << data_rate.Get()/*.replace(2, 10, nendr)*/ << ")" << endl;
		}
		cout << "------------" << endl;
	}


	cout << "Configuring simulation details..." << endl << endl;

	RunGtmpInter(server, Seconds(2.0), "off");
	RunIp(all, Seconds(2.1), "route");

	RunIp(server, Seconds(2.2), "addr list sim0");
	RunIp(clients, Seconds(2.2), "addr list sim0");

	RunGtmpInter(clients, Seconds(2.3), "off");

/*	RunApp("tcp-server", server, Seconds(4.0), 1 << 31);
	RunApp("tcp-client", clients.Get(nrelays-1), Seconds(5.0), "10.1.1.2", 1 << 16);*/

	RunApp("gmtp-server", server, Seconds(3.0), 1 << 31);

	cout << "We have " << clients.GetN() << " clients." << endl;
	for(int i=0; i < clients.GetN(); ++i) {
		RunApp("gmtp-client", clients.Get(i), Seconds(5.0), "10.1.1.2", 1 << 16);
	}
//	RunApp("gmtp-client", clients, Seconds(5.0), "10.1.1.2", 1 << 16);
//	RunAppMulti("gmtp-client", clients, 5.0, "10.1.1.2", 1 << 16, 30);

	local_csma.EnablePcapAll("dce-gmtp-master");

	AsciiTraceHelper ascii;
	local_csma.EnableAsciiAll(ascii.CreateFileStream("dce-gmtp-master.tr"));

	cout << "\nRunning simulation..." << endl;
	Simulator::Stop (Seconds (12000.0));
	Simulator::Run();
	Simulator::Destroy();

	cout << "Done." << endl;

	return 0;
}

