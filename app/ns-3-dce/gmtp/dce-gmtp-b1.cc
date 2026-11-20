/*
 * dce-gmtp-b1 - Campaign B1 of article 1 (reduced GEANT, live media),
 * rebuilt for DCE from the thesis (analise-desempenho-mudccp.tex and
 * the link table in detalhes-experimentos.tex).
 *
 * Backbone: the 27 GEANT PoPs and the 37 links of the thesis table, with
 * their rates and delays (p2p). LAN routers hang off 12 PoPs through
 * access links (--access, 2 Mbps in the thesis); each LAN router has a
 * csma LAN (100 Mbps / --ldelay) with receivers. The --nservers sources
 * sit on their own LAN routers (DE, UK, FR, ...). Receivers are spread
 * round-robin over the 12 client LANs and join uniformly at random in
 * the first --arrival seconds (flash crowd; the thesis used 200 s of a
 * 900 s run). Queues default to 100 packets (thesis router buffer).
 * Churn is not modelled yet.
 */
#include "dce-gmtp-exp.h"

using namespace ns3;
using namespace gmtpexp;

static const char *kPops[] = {
	"DE", "SE", "CZ", "FR", "IT", "NL", "AT", "GR", "IE", "UK", "EE",
	"LV", "LT", "PL", "IL", "CH", "BE", "LU", "ES", "SK", "HU", "HR",
	"SI", "RO", "PT", "CY", "BG"};
struct GLink {
	const char *a, *b;
	const char *rate, *delay;
};
static const GLink kLinks[] = {
	{"DE", "SE", "32Mbps", "6.34ms"}, {"DE", "CZ", "32Mbps", "2.10ms"},
	{"DE", "FR", "32Mbps", "11.39ms"}, {"DE", "IT", "32Mbps", "11.93ms"},
	{"DE", "NL", "27Mbps", "5.23ms"}, {"DE", "AT", "27Mbps", "4.71ms"},
	{"DE", "GR", "16Mbps", "21.69ms"}, {"DE", "IE", "10Mbps", "13.69ms"},
	{"SE", "UK", "32Mbps", "14.59ms"}, {"SE", "EE", "10Mbps", "8.78ms"},
	{"SE", "LV", "3Mbps", "9.71ms"}, {"SE", "LT", "5Mbps", "10.78ms"},
	{"SE", "PL", "27Mbps", "6.49ms"}, {"UK", "FR", "32Mbps", "5.51ms"},
	{"UK", "NL", "27Mbps", "1.29ms"}, {"UK", "IE", "10Mbps", "3.97ms"},
	{"UK", "IL", "5Mbps", "36.09ms"}, {"UK", "GR", "10Mbps", "31.25ms"},
	{"FR", "CH", "32Mbps", "3.84ms"}, {"FR", "BE", "27Mbps", "4.56ms"},
	{"FR", "LU", "10Mbps", "3.34ms"}, {"CH", "AT", "32Mbps", "4.01ms"},
	{"CH", "IT", "32Mbps", "2.14ms"}, {"IT", "ES", "27Mbps", "7.54ms"},
	{"NL", "BE", "27Mbps", "1.18ms"}, {"BE", "LU", "10Mbps", "1.16ms"},
	{"PL", "CZ", "27Mbps", "3.64ms"}, {"CZ", "SK", "27Mbps", "2.21ms"},
	{"SK", "HU", "27Mbps", "2.13ms"}, {"AT", "HU", "27Mbps", "3.68ms"},
	{"AT", "HR", "3Mbps", "3.88ms"}, {"AT", "SI", "16Mbps", "1.18ms"},
	{"HU", "SI", "10Mbps", "2.18ms"}, {"HU", "RO", "3Mbps", "2.18ms"},
	{"ES", "PT", "16Mbps", "2.57ms"}, {"CY", "GR", "3Mbps", "2.57ms"},
	{"BG", "GR", "3Mbps", "2.57ms"}};
/* PoPs that get a client LAN (12, as the thesis' LAN routers) */
static const char *kLanPops[] = {"DE", "UK", "FR", "IT", "ES", "NL",
				 "SE", "PL", "AT", "CH", "GR", "HU"};
/* PoPs that get a server LAN, in order (S = 1, 3, 5) */
static const char *kSrvPops[] = {"DE", "UK", "FR", "IT", "ES"};

int main(int argc, char *argv[])
{
	double wall0 = WallS();
	Cfg c;
	c.rate = 125000;  /* 1 Mbps: fits the 2 Mbps access links */
	c.qsize = 100;    /* thesis router buffer */
	uint32_t nclients = 50, nservers = 1;
	double arrival = 20.0;
	std::string access = "2Mbps", lan = "100Mbps", ldelay = "1ms";
	CommandLine cmd;
	c.AddTo(cmd);
	cmd.AddValue("nclients", "receivers N", nclients);
	cmd.AddValue("nservers", "sources S (1..5)", nservers);
	cmd.AddValue("arrival", "flash-crowd window (simulated s)", arrival);
	cmd.AddValue("access", "LAN router <-> PoP rate", access);
	cmd.AddValue("lan", "LAN rate", lan);
	cmd.AddValue("ldelay", "LAN and access delay", ldelay);
	cmd.Parse(argc, argv);
	c.Apply();
	nservers = std::max(1u, std::min(nservers, 5u));

	const uint32_t npop = sizeof(kPops) / sizeof(kPops[0]);
	const uint32_t nlan = sizeof(kLanPops) / sizeof(kLanPops[0]);
	std::map<std::string, uint32_t> idx;
	for (uint32_t i = 0; i < npop; ++i)
		idx[kPops[i]] = i;

	NodeContainer pops, lrs, srs, srv, clients;
	pops.Create(npop);
	lrs.Create(nlan);
	srs.Create(nservers);
	srv.Create(nservers);
	std::vector<NodeContainer> lanc(nlan);
	for (uint32_t i = 0; i < nclients; ++i) {
		Ptr<Node> n = CreateObject<Node>();
		lanc[i % nlan].Add(n);
		clients.Add(n);
	}
	NodeContainer routers(pops, lrs, srs), hosts(srv, clients),
		all(routers, hosts);
	InstallStack(all);

	Ipv4AddressHelper ip;
	uint32_t net = 0;
	auto assign = [&](NetDeviceContainer d) {
		std::ostringstream b;
		b << "10." << (net / 256) << "." << (net % 256) << ".0";
		ip.SetBase(b.str().c_str(), "255.255.255.0");
		++net;
		return ip.Assign(d);
	};
	PointToPointHelper p2p;
	for (const GLink &l : kLinks) {
		p2p.SetDeviceAttribute("DataRate", StringValue(l.rate));
		p2p.SetChannelAttribute("Delay", StringValue(l.delay));
		NetDeviceContainer d = p2p.Install(pops.Get(idx[l.a]),
						   pops.Get(idx[l.b]));
		assign(d);
		Watch(std::string("geant_") + l.a + "_" + l.b, d.Get(0));
		Watch(std::string("geant_") + l.b + "_" + l.a, d.Get(1));
	}
	p2p.SetDeviceAttribute("DataRate", StringValue(access));
	p2p.SetChannelAttribute("Delay", StringValue(ldelay));
	CsmaHelper csma;
	csma.SetChannelAttribute("DataRate", StringValue(lan));
	csma.SetChannelAttribute("Delay", StringValue(ldelay));
	for (uint32_t i = 0; i < nlan; ++i) {
		NetDeviceContainer d = p2p.Install(pops.Get(idx[kLanPops[i]]),
						   lrs.Get(i));
		assign(d);
		Watch(std::string("access_") + kLanPops[i], d.Get(0));
		assign(csma.Install(NodeContainer(lrs.Get(i), lanc[i])));
	}
	std::vector<std::string> sips;
	for (uint32_t s = 0; s < nservers; ++s) {
		NetDeviceContainer d = p2p.Install(srs.Get(s),
						   pops.Get(idx[kSrvPops[s]]));
		assign(d);
		Watch(std::string("srvaccess_") + kSrvPops[s], d.Get(0));
		NetDeviceContainer dl =
			csma.Install(NodeContainer(srs.Get(s), srv.Get(s)));
		Ipv4InterfaceContainer ic = assign(dl);
		std::ostringstream a;
		ic.GetAddress(1).Print(a);
		sips.push_back(a.str());
		Watch(std::string("server_lan_") + kSrvPops[s], dl.Get(1));
	}

	std::cout << "EXP_TOPO scenario=b1 proto=" << c.proto << " nclients="
		  << nclients << " nservers=" << nservers << " pops=" << npop
		  << " links=" << sizeof(kLinks) / sizeof(kLinks[0])
		  << " lans=" << nlan << " access=" << access << " lan=" << lan
		  << " ldelay=" << ldelay << " qsize=" << c.qsize << " rate="
		  << c.rate << " duration=" << c.duration << " arrival="
		  << arrival << std::endl;
	PrintNodes("server", srv);
	PrintNodes("client", clients);
	PrintNodes("router", routers);

	Ptr<UniformRandomVariable> u = CreateObject<UniformRandomVariable>();
	SetRelays(c, routers, hosts, 2.0);
	double last = 0;
	for (uint32_t s = 0; s < nservers; ++s) {
		std::ostringstream flow, a;
		flow << "flow" << s << "ABCDEFGHIJK"; /* 16 chars */
		a << c.MediaProto() << " " << c.rate << " " << c.duration << " "
		  << flow.str().substr(0, 16);
		Run(srv.Get(s), 2.5 + u->GetValue(0, 0.1), "media-server",
		    a.str(), 1 << 22);
	}
	for (uint32_t i = 0; i < clients.GetN(); ++i) {
		uint32_t s = i % nservers;
		std::ostringstream flow, a;
		flow << "flow" << s << "ABCDEFGHIJK";
		a << c.MediaProto() << " " << sips[s] << " "
		  << flow.str().substr(0, 16);
		double t = 3.5 + u->GetValue(0, arrival);
		last = std::max(last, t);
		Run(clients.Get(i), t, "media-client", a.str(), 1 << 20);
	}
	Finish(c, wall0, last + c.duration + c.tail);
	return 0;
}
