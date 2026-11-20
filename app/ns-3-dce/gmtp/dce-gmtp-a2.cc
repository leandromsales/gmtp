/*
 * dce-gmtp-a2 - Campaign A2 of article 1 (tree scale, thesis Treatment 2
 * rebuilt for DCE).
 *
 * Complete binary tree of 2^(L+1)-1 routers joined by p2p backbone links
 * (100 Mbps / 5 ms). Every router has a LAN (csma, 100 Mbps / 1 ms) with
 * --nclients receivers (10 in the thesis), so L=0..4 gives 10, 30, 70,
 * 150, 310 receivers. The media source sits on its own LAN at the root.
 * In the gmtp cell every router runs the GMTP relay.
 */
#include "dce-gmtp-exp.h"

using namespace ns3;
using namespace gmtpexp;

int main(int argc, char *argv[])
{
	double wall0 = WallS();
	Cfg c;
	uint32_t level = 2, nclients = 10;
	std::string bw = "100Mbps", bdelay = "5ms", lan = "100Mbps",
		    ldelay = "1ms";
	CommandLine cmd;
	c.AddTo(cmd);
	cmd.AddValue("level", "tree level L (routers = 2^(L+1)-1)", level);
	cmd.AddValue("nclients", "receivers per router LAN", nclients);
	cmd.AddValue("bw", "backbone rate", bw);
	cmd.AddValue("bdelay", "backbone delay", bdelay);
	cmd.AddValue("lan", "LAN rate", lan);
	cmd.AddValue("ldelay", "LAN delay", ldelay);
	cmd.Parse(argc, argv);
	c.Apply();

	uint32_t nr = (1u << (level + 1)) - 1;
	NodeContainer routers, srv, clients;
	routers.Create(nr);
	srv.Create(1);
	std::vector<NodeContainer> lanc(nr);
	for (uint32_t r = 0; r < nr; ++r) {
		lanc[r].Create(nclients);
		clients.Add(lanc[r]);
	}
	NodeContainer hosts(srv, clients), all(routers, hosts);
	InstallStack(all);

	PointToPointHelper p2p;
	p2p.SetDeviceAttribute("DataRate", StringValue(bw));
	p2p.SetChannelAttribute("Delay", StringValue(bdelay));
	CsmaHelper csma;
	csma.SetChannelAttribute("DataRate", StringValue(lan));
	csma.SetChannelAttribute("Delay", StringValue(ldelay));
	Ipv4AddressHelper ip;

	/* source LAN: root router + server, 10.255.0.0/24 */
	NetDeviceContainer ds = csma.Install(NodeContainer(routers.Get(0), srv));
	ip.SetBase("10.255.0.0", "255.255.255.0");
	ip.Assign(ds);
	Watch("server_lan", ds.Get(1));
	/* tree links: parent (r-1)/2 -> child r, 10.0.<r>.0/30-ish /24 */
	for (uint32_t r = 1; r < nr; ++r) {
		uint32_t p = (r - 1) / 2;
		NetDeviceContainer d = p2p.Install(routers.Get(p), routers.Get(r));
		std::ostringstream base;
		base << "10.0." << r << ".0";
		ip.SetBase(base.str().c_str(), "255.255.255.0");
		ip.Assign(d);
		std::ostringstream n;
		n << "tree_" << p << "_" << r;
		Watch(n.str(), d.Get(0));
	}
	/* router LANs, 10.1.<r>.0/24 */
	for (uint32_t r = 0; r < nr; ++r) {
		NetDeviceContainer d = csma.Install(NodeContainer(routers.Get(r),
								 lanc[r]));
		std::ostringstream base;
		base << "10.1." << r << ".0";
		ip.SetBase(base.str().c_str(), "255.255.255.0");
		ip.Assign(d);
	}

	std::cout << "EXP_TOPO scenario=a2 proto=" << c.proto << " level="
		  << level << " routers=" << nr << " nclients=" << clients.GetN()
		  << " bw=" << bw << " bdelay=" << bdelay << " lan=" << lan
		  << " ldelay=" << ldelay << " qsize=" << c.qsize << " rate="
		  << c.rate << " duration=" << c.duration << std::endl;
	PrintNodes("server", srv);
	PrintNodes("client", clients);
	PrintNodes("router", routers);

	Ptr<UniformRandomVariable> u = CreateObject<UniformRandomVariable>();
	SetRelays(c, routers, hosts, 2.0);
	double ts = 2.5 + u->GetValue(0, 0.1);
	std::vector<double> tc;
	/* receivers join over 10 simulated s, in random order */
	for (uint32_t i = 0; i < clients.GetN(); ++i)
		tc.push_back(3.5 + u->GetValue(0, 10.0));
	Media(c, srv.Get(0), "10.255.0.2", ts, clients, tc);

	double last = 0;
	for (double t : tc)
		last = std::max(last, t);
	Finish(c, wall0, last + c.duration + c.tail);
	return 0;
}
