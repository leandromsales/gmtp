/*
 * dce-gmtp-a1 - Campaign A1 of article 1 (fairness at the backbone
 * bottleneck, thesis Treatment 1 rebuilt for DCE).
 *
 *   S  X1..Xk                         C1..Cn  Y1..Yk
 *   |__|__ LAN (csma) __ RS ===== RC __ LAN (csma) __|__|
 *                          bottleneck (p2p)
 *
 * S is the media source, C1..Cn the receivers behind router RC (the GMTP
 * relay in the gmtp cell), Xi -> Yi greedy TCP (--cc, cubic) cross flows
 * that compete for the RS->RC bottleneck. Thesis defaults: backbone
 * 100 Mbps / 5 ms, LAN 100 Mbps / 1 ms, queue 3000 packets, 10 hosts per
 * LAN. The media cell is chosen with --proto (see dce-gmtp-exp.h).
 */
#include "dce-gmtp-exp.h"

using namespace ns3;
using namespace gmtpexp;

int main(int argc, char *argv[])
{
	double wall0 = WallS();
	Cfg c;
	uint32_t nclients = 10, ncross = 1;
	std::string bw = "100Mbps", bdelay = "5ms", lan = "100Mbps",
		    ldelay = "10us";
	/* LAN propagation delay. ns-3 CSMA keeps the shared channel busy
	 * for transmission time plus propagation delay, so the thesis' 1 ms
	 * (an NS-2 duplex link) on a csma LAN caps the whole LAN at about
	 * 1000 frames/s (~6 Mbit/s of TCP with ACKs). 10 us is a switched
	 * LAN; the 1 ms per hop of the thesis is kept on the p2p links. */
	CommandLine cmd;
	c.AddTo(cmd);
	cmd.AddValue("nclients", "receivers behind the bottleneck", nclients);
	cmd.AddValue("ncross", "greedy TCP cross flows", ncross);
	cmd.AddValue("bw", "bottleneck rate", bw);
	cmd.AddValue("bdelay", "bottleneck delay", bdelay);
	cmd.AddValue("lan", "LAN rate", lan);
	cmd.AddValue("ldelay", "LAN delay", ldelay);
	cmd.Parse(argc, argv);
	c.Apply();

	NodeContainer rs, rc, srv, clients, xs, ys;
	rs.Create(1);
	rc.Create(1);
	srv.Create(1);
	clients.Create(nclients);
	xs.Create(ncross);
	ys.Create(ncross);
	NodeContainer routers(rs, rc);
	NodeContainer hosts(srv, clients, xs, ys);
	NodeContainer all(routers, hosts);
	InstallStack(all);

	PointToPointHelper p2p;
	p2p.SetDeviceAttribute("DataRate", StringValue(bw));
	p2p.SetChannelAttribute("Delay", StringValue(bdelay));
	CsmaHelper csma;
	csma.SetChannelAttribute("DataRate", StringValue(lan));
	csma.SetChannelAttribute("Delay", StringValue(ldelay));

	NetDeviceContainer dbn = p2p.Install(rs.Get(0), rc.Get(0));
	NodeContainer lanS(rs, srv, xs), lanC(rc, clients, ys);
	NetDeviceContainer dls = csma.Install(lanS), dlc = csma.Install(lanC);

	Ipv4AddressHelper ip;
	ip.SetBase("10.0.0.0", "255.255.255.0");
	ip.Assign(dbn);
	ip.SetBase("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer is = ip.Assign(dls);
	ip.SetBase("10.2.1.0", "255.255.255.0");
	Ipv4InterfaceContainer ic = ip.Assign(dlc);

	Watch("bottleneck_fwd", dbn.Get(0));
	Watch("bottleneck_rev", dbn.Get(1));
	Watch("server_lan", dls.Get(1));

	std::cout << "EXP_TOPO scenario=a1 proto=" << c.proto << " nclients="
		  << nclients << " ncross=" << ncross << " bw=" << bw
		  << " bdelay=" << bdelay << " lan=" << lan << " ldelay="
		  << ldelay << " qsize=" << c.qsize << " rate=" << c.rate
		  << " duration=" << c.duration << " cc=" << c.cc << std::endl;
	PrintNodes("server", srv);
	PrintNodes("client", clients);
	PrintNodes("xsend", xs);
	PrintNodes("xsink", ys);
	PrintNodes("router", routers);

	Ptr<UniformRandomVariable> u = CreateObject<UniformRandomVariable>();
	SetRelays(c, routers, hosts, 2.0);
	std::string sip = "10.1.1.2"; /* srv is the 2nd device of lanS */
	double ts = 2.5 + u->GetValue(0, 0.1);
	std::vector<double> tc;
	for (uint32_t i = 0; i < nclients; ++i)
		tc.push_back(3.5 + 0.2 * i + u->GetValue(0, 0.1));
	Media(c, srv.Get(0), sip, ts, clients, tc);

	/* cross flows start with the receivers and outlast the media */
	for (uint32_t i = 0; i < ncross; ++i) {
		Ipv4Address yip = ic.GetAddress(1 + nclients + i);
		std::ostringstream sa, sd;
		yip.Print(sd);
		Run(ys.Get(i), 2.5, "tcp-bulk", "sink 5001");
		sa << "send " << sd.str() << " 5001 " << (c.duration + 5) << " "
		   << c.cc;
		Run(xs.Get(i), 3.0 + u->GetValue(0, 0.5), "tcp-bulk", sa.str());
	}

	double stop = tc.back() + c.duration + c.tail;
	Finish(c, wall0, stop);
	return 0;
}
