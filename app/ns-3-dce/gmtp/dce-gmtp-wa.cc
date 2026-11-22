/*
 * dce-gmtp-wa - Wendell's Topology A (2016 dissertation, section 4.2),
 * rebuilt for article 1 on the current DCE build.
 *
 * Early-deployment case: only the router next to each group of receivers
 * runs GMTP-Inter, and the routes from the server to the relays do not
 * share any GMTP-capable hop, so relays cannot form partnerships:
 *
 *   server --csma-- R0 --p2p-- C1 --p2p-- relay 1 --csma-- n receivers
 *                      \--p2p-- C2 --p2p-- relay 2 --csma-- n receivers
 *                       ...
 * R0 and the Ci are plain IP routers (gmtp-inter off in every cell); the
 * relays run gmtp-inter in the gmtp cell. Wendell's treatments: relays
 * {1, 3, 30}, receivers per relay {1, 10, 20} (N = 1, 10, 20 / 3, 30, 60 /
 * 30, 300, 600). Backbone 100 Mbps / 5 ms, LAN 100 Mbps / 10 us (see the
 * note in dce-gmtp-a2.cc on csma propagation delay). Receivers join in the
 * first third of the transmission, as in the dissertation.
 */
#include "dce-gmtp-exp.h"

using namespace ns3;
using namespace gmtpexp;

int main(int argc, char *argv[])
{
	double wall0 = WallS();
	Cfg c;
	uint32_t nrelays = 1, nclients = 10;
	double join = -1;
	std::string bw = "100Mbps", bdelay = "5ms", lan = "100Mbps",
		    ldelay = "10us";
	CommandLine cmd;
	c.AddTo(cmd);
	cmd.AddValue("relays", "number of relays (disjoint paths)", nrelays);
	cmd.AddValue("nclients", "receivers per relay LAN", nclients);
	cmd.AddValue("join", "join window in s (default duration/3)", join);
	cmd.AddValue("bw", "backbone rate", bw);
	cmd.AddValue("bdelay", "backbone delay", bdelay);
	cmd.AddValue("lan", "LAN rate", lan);
	cmd.AddValue("ldelay", "LAN delay", ldelay);
	cmd.Parse(argc, argv);
	c.Apply();
	if (join < 0)
		join = c.duration / 3.0;

	NodeContainer r0, core, relays, srv, clients;
	r0.Create(1);
	core.Create(nrelays);
	relays.Create(nrelays);
	srv.Create(1);
	std::vector<NodeContainer> lanc(nrelays);
	for (uint32_t r = 0; r < nrelays; ++r) {
		lanc[r].Create(nclients);
		clients.Add(lanc[r]);
	}
	NodeContainer plain(r0, core), hosts(srv, clients);
	NodeContainer all(plain, relays);
	all.Add(hosts);
	InstallStack(all);

	PointToPointHelper p2p;
	p2p.SetDeviceAttribute("DataRate", StringValue(bw));
	p2p.SetChannelAttribute("Delay", StringValue(bdelay));
	CsmaHelper csma;
	csma.SetChannelAttribute("DataRate", StringValue(lan));
	csma.SetChannelAttribute("Delay", StringValue(ldelay));
	Ipv4AddressHelper ip;

	NetDeviceContainer ds = csma.Install(NodeContainer(r0.Get(0), srv));
	ip.SetBase("10.255.0.0", "255.255.255.0");
	ip.Assign(ds);
	Watch("server_lan", ds.Get(1));
	for (uint32_t r = 0; r < nrelays; ++r) {
		NetDeviceContainer a = p2p.Install(r0.Get(0), core.Get(r));
		std::ostringstream b1, n1;
		b1 << "10.2." << r << ".0";
		ip.SetBase(b1.str().c_str(), "255.255.255.0");
		ip.Assign(a);
		n1 << "tree_r0_c" << r;
		Watch(n1.str(), a.Get(0));
		NetDeviceContainer b = p2p.Install(core.Get(r), relays.Get(r));
		std::ostringstream b2, n2;
		b2 << "10.3." << r << ".0";
		ip.SetBase(b2.str().c_str(), "255.255.255.0");
		ip.Assign(b);
		n2 << "tree_c" << r << "_relay" << r;
		Watch(n2.str(), b.Get(0));
		NetDeviceContainer d = csma.Install(NodeContainer(relays.Get(r),
								 lanc[r]));
		std::ostringstream b3;
		b3 << "10.1." << r << ".0";
		ip.SetBase(b3.str().c_str(), "255.255.255.0");
		ip.Assign(d);
	}

	std::cout << "EXP_TOPO scenario=wa proto=" << c.proto << " relays="
		  << nrelays << " nclients=" << clients.GetN() << " bw=" << bw
		  << " bdelay=" << bdelay << " lan=" << lan << " ldelay="
		  << ldelay << " qsize=" << c.qsize << " rate=" << c.rate
		  << " duration=" << c.duration << " join=" << join
		  << std::endl;
	PrintNodes("server", srv);
	PrintNodes("client", clients);
	PrintNodes("router", relays);
	PrintNodes("core", plain);

	Ptr<UniformRandomVariable> u = CreateObject<UniformRandomVariable>();
	SetRelays(c, relays, hosts, 2.0);
	for (uint32_t i = 0; i < plain.GetN(); ++i)
		Run(plain.Get(i), 2.0, "gmtp-inter", "off", 1 << 16);
	double ts = 2.5 + u->GetValue(0, 0.1);
	std::vector<double> tc;
	for (uint32_t i = 0; i < clients.GetN(); ++i)
		tc.push_back(3.5 + u->GetValue(0, join));
	Media(c, srv.Get(0), "10.255.0.2", ts, clients, tc);

	double last = 0;
	for (double t : tc)
		last = std::max(last, t);
	Finish(c, wall0, last + c.duration + c.tail);
	return 0;
}
