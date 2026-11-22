/*
 * dce-gmtp-exp.h - helpers shared by the article-1 scenarios
 * (dce-gmtp-a1, dce-gmtp-a2, dce-gmtp-b1).
 *
 * Protocol cells (--proto):
 *   gmtp         GMTP with relay cooperation: gmtp-inter on in routers,
 *                off in end hosts (as dce-gmtp-master).
 *   gmtp-nocoop  ablation: gmtp-inter off everywhere (each receiver is an
 *                end-to-end GMTP flow to the server).
 *   tcp          the media goes over one TCP connection per receiver,
 *                congestion control --cc (cubic by default).
 * Seeds: --RngRun=<seed> drives the start-time jitter of every app.
 * Results: each node writes media-client.txt / media-server.txt /
 * bulk-*.txt in files-<id>/; the scenario prints EXP_* lines on stdout
 * (topology, link counters) that provision/collect-trial.py parses.
 */
#ifndef DCE_GMTP_EXP_H_
#define DCE_GMTP_EXP_H_

#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <sys/resource.h>
#include <unistd.h>
#include <sys/time.h>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/dce-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"

namespace gmtpexp {

using namespace ns3;

static std::string g_sysctl; /* --sysctl=.path=value,... */

struct Cfg {
	std::string proto = "gmtp";
	std::string cc = "cubic";
	uint32_t rate = 300000;      /* media rate, B/s */
	double duration = 60.0;      /* media duration, simulated s */
	uint32_t qsize = 3000;       /* device queue, packets (thesis) */
	double tail = 15.0;          /* simulated s after the media ends */

	void AddTo(CommandLine &cmd)
	{
		cmd.AddValue("proto", "gmtp | gmtp-nocoop | tcp", proto);
		cmd.AddValue("cc", "TCP congestion control for tcp cells", cc);
		cmd.AddValue("rate", "media rate in bytes/s", rate);
		cmd.AddValue("duration", "media duration (simulated s)", duration);
		cmd.AddValue("qsize", "device queue size in packets", qsize);
		cmd.AddValue("tail", "extra simulated s after the media", tail);
		cmd.AddValue("sysctl", "extra sysctls: .net.x.y=v,.net.z=w",
			     g_sysctl);
	}
	bool Gmtp() const { return proto != "tcp"; }
	bool Coop() const { return proto == "gmtp"; }
	std::string MediaProto() const { return Gmtp() ? "gmtp" : "tcp"; }
	void Apply() const
	{
		std::ostringstream q;
		q << qsize << "p";
		Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize",
				   QueueSizeValue(QueueSize(q.str())));
	}
};

static inline double WallS()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec + tv.tv_usec / 1e6;
}

static inline void Run(Ptr<Node> n, double at, const std::string &bin,
		       const std::string &args, uint32_t stack = 1 << 20)
{
	DceApplicationHelper p;
	p.SetBinary(bin);
	p.SetStackSize(stack);
	p.ResetArguments();
	if (!args.empty())
		p.ParseArguments(args);
	ApplicationContainer a = p.Install(n);
	a.Start(Seconds(at));
}

static inline void InstallStack(NodeContainer all)
{
	DceManagerHelper dce;
	dce.SetTaskManagerAttribute("FiberManagerType",
				    StringValue("UcontextFiberManager"));
	dce.SetNetworkStack("ns3::LinuxSocketFdFactory", "Library",
			    StringValue("liblinux.so"));
	LinuxStackHelper stack;
	stack.Install(all);
	dce.Install(all);
	/* libos defaults leave TCP window-limited (~8 KiB, a few Mbit/s at
	 * 10 ms RTT); use the usual Linux host autotuning limits. */
	stack.SysctlSet(all, ".net.ipv4.tcp_rmem",
				    "4096 131072 6291456");
	stack.SysctlSet(all, ".net.ipv4.tcp_wmem",
				    "4096 16384 4194304");
	stack.SysctlSet(all, ".net.core.rmem_max", "6291456");
	stack.SysctlSet(all, ".net.core.wmem_max", "4194304");
	stack.SysctlSet(all, ".net.ipv4.tcp_window_scaling", "1");
	/* SACK compression arms an hrtimer (tcp_compressed_ack_kick) that
	 * libos runs outside any DCE task: get_current() is NULL and the
	 * simulator segfaults as soon as there is loss (Task::GetExtraContext
	 * this=0). Disable it. */
	stack.SysctlSet(all, ".net.ipv4.tcp_comp_sack_nr", "0");
	std::stringstream ss(g_sysctl);
	std::string kv;
	while (std::getline(ss, kv, ',')) {
		size_t eq = kv.find('=');
		if (eq != std::string::npos)
			stack.SysctlSet(all, kv.substr(0, eq), kv.substr(eq + 1));
	}
}

/* gmtp-inter on/off per node, following the protocol cell. */
static inline void SetRelays(const Cfg &c, NodeContainer routers,
			     NodeContainer hosts, double at = 2.0)
{
	for (uint32_t i = 0; i < hosts.GetN(); ++i)
		Run(hosts.Get(i), at, "gmtp-inter", "off", 1 << 16);
	if (!c.Coop())
		for (uint32_t i = 0; i < routers.GetN(); ++i)
			Run(routers.Get(i), at, "gmtp-inter", "off", 1 << 16);
}

/* Media source and receivers; receivers start at their own times. */
static inline void Media(const Cfg &c, Ptr<Node> server,
			 const std::string &server_ip, double ts,
			 NodeContainer clients, const std::vector<double> &tc)
{
	std::ostringstream a;
	a << c.MediaProto() << " " << c.rate << " " << c.duration;
	Run(server, ts, "media-server", a.str(), 1 << 22);
	for (uint32_t i = 0; i < clients.GetN(); ++i)
		Run(clients.Get(i), tc[i], "media-client",
		    c.MediaProto() + " " + server_ip, 1 << 20);
}

/*
 * Control-overhead accounting (experimental design, metrics 4 and 7; thesis
 * overhead analysis). Every frame seen by a traced device is classified
 * from its IPv4 header, outside the simulated kernels (the protocol stack
 * is not touched):
 *   GMTP (IP protocol 254): by packet type (DATA vs each control type);
 *   media TCP (port 2000):   syn, fin/rst, pure ACK, data, retransmission
 *                            (data whose end does not pass the highest end
 *                            seen for that direction at that observer);
 *   other TCP (cross flows) and other IP are counted apart.
 * Bytes are IP bytes (tot_len), independent of the link-layer framing.
 * Observers are links (EXP_LINK names, transmit side) and node roles
 * (server, router = relays when gmtp-inter is on, client), where the frames
 * transmitted and received by every device of every node of the role are
 * counted.
 */
static const char *const kGmtpType[] = {
	"REQUEST", "REQUESTNOTIFY", "RESPONSE", "REGISTER", "REGISTER_REPLY",
	"ROUTE_NOTIFY", "RELAYQUERY", "RELAYQUERY_REPLY", "DATA", "ACK",
	"DATAACK", "MEDIADESC", "DATAPULL_REQUEST", "DATAPULL_RESPONSE",
	"ELECT_REQUEST", "ELECT_RESPONSE", "CLOSE", "RESET", "FEEDBACK",
	"DELEGATE", "DELEGATE_REPLY"};
enum { OV_GMTP_TYPES = 21, OV_TCP_SYN = 32, OV_TCP_FINRST, OV_TCP_PUREACK,
       OV_TCP_DATA, OV_TCP_RETX, OV_TCP_CROSS, OV_GMTP_OTHER, OV_OTHER,
       OV_NCAT };
static const char *OvName(int c)
{
	if (c < OV_GMTP_TYPES)
		return kGmtpType[c];
	switch (c) {
	case OV_TCP_SYN: return "TCP_SYN";
	case OV_TCP_FINRST: return "TCP_FIN_RST";
	case OV_TCP_PUREACK: return "TCP_PURE_ACK";
	case OV_TCP_DATA: return "TCP_DATA";
	case OV_TCP_RETX: return "TCP_RETX";
	case OV_TCP_CROSS: return "TCP_CROSS";
	case OV_GMTP_OTHER: return "GMTP_OTHER";
	case OV_OTHER: return "OTHER";
	}
	return 0;
}
struct OvStat {
	uint64_t p[OV_NCAT] = {0}, b[OV_NCAT] = {0};
};
typedef std::map<std::pair<uint64_t, uint32_t>, uint32_t> OvSeq;
static std::map<std::string, OvStat> g_ovh;
/* highest TCP end seen per flow direction, per single vantage point
 * (one device direction), so a packet forwarded A -> B is not taken as a
 * retransmission when both A and B belong to the same role */
static std::map<std::string, OvSeq> g_ovseq;

static inline uint32_t Be32(const uint8_t *q)
{
	return (uint32_t)q[0] << 24 | (uint32_t)q[1] << 16 | (uint32_t)q[2] << 8 | q[3];
}
static inline void OvCount(OvStat &o, OvSeq &hi, Ptr<const Packet> pkt)
{
	uint8_t f[128];
	uint32_t n = pkt->CopyData(f, sizeof(f));
	uint32_t ip;
	if (n >= 16 && f[12] == 0x08 && f[13] == 0x00)
		ip = 14;			/* Ethernet (CSMA) */
	else if (n >= 4 && f[0] == 0x00 && f[1] == 0x21)
		ip = 2;				/* PPP (point-to-point) */
	else
		return;				/* ARP and the like */
	if (n < ip + 20 || (f[ip] >> 4) != 4)
		return;
	const uint8_t *h = f + ip;
	uint32_t ihl = (h[0] & 0x0f) * 4, tot = (uint32_t)h[2] << 8 | h[3];
	int c = OV_OTHER;
	const uint8_t *l4 = h + ihl;
	if (h[9] == 254 && n >= ip + ihl + 1) {
		uint32_t t = (l4[0] >> 2) & 0x1f;
		c = t < OV_GMTP_TYPES ? (int)t : OV_GMTP_OTHER;
	} else if (h[9] == 6 && n >= ip + ihl + 14) {
		uint32_t sp = (uint32_t)l4[0] << 8 | l4[1];
		uint32_t dp = (uint32_t)l4[2] << 8 | l4[3];
		uint32_t doff = (l4[12] >> 4) * 4, fl = l4[13];
		uint32_t pay = tot > ihl + doff ? tot - ihl - doff : 0;
		if (sp != 2000 && dp != 2000)
			c = OV_TCP_CROSS;
		else if (fl & 0x02)
			c = OV_TCP_SYN;
		else if (fl & 0x05)
			c = OV_TCP_FINRST;
		else if (pay == 0)
			c = OV_TCP_PUREACK;
		else {
			std::pair<uint64_t, uint32_t> k(
				(uint64_t)Be32(h + 12) << 32 | Be32(h + 16),
				sp << 16 | dp);
			uint32_t end = Be32(l4 + 4) + pay;
			auto it = hi.find(k);
			if (it == hi.end()) {
				hi[k] = end;
				c = OV_TCP_DATA;
			} else if ((int32_t)(end - it->second) > 0) {
				it->second = end;
				c = OV_TCP_DATA;
			} else
				c = OV_TCP_RETX;
		}
	}
	o.p[c]++;
	o.b[c] += tot;
}
/* context "role_<role>|<node>|tx" or "...|rx": counts go to the role */
static inline void OnOvh(std::string ctx, Ptr<const Packet> p)
{
	OvCount(g_ovh[ctx.substr(0, ctx.find('|'))], g_ovseq[ctx], p);
	/* network-wide transmissions (every hop counted once, sender side) */
	if (ctx.size() > 3 && ctx.compare(ctx.size() - 3, 3, "|tx") == 0)
		OvCount(g_ovh["net_tx"], g_ovseq["net|" + ctx], p);
}
/* count every frame sent and received by the devices of these nodes */
static inline void WatchRole(const std::string &role, NodeContainer n)
{
	std::string k = "role_" + role;
	g_ovh[k];
	for (uint32_t i = 0; i < n.GetN(); ++i)
		for (uint32_t d = 0; d < n.Get(i)->GetNDevices(); ++d) {
			Ptr<NetDevice> dev = n.Get(i)->GetDevice(d);
			if (!dev->IsPointToPoint() &&
			    !DynamicCast<CsmaNetDevice>(dev))
				continue;
			std::ostringstream c;
			c << k << "|" << n.Get(i)->GetId() << "." << d;
			dev->TraceConnect("PhyTxBegin", c.str() + "|tx",
					  MakeCallback(&OnOvh));
			dev->TraceConnect("MacRx", c.str() + "|rx",
					  MakeCallback(&OnOvh));
		}
}
static inline void PrintOvh()
{
	for (auto &kv : g_ovh)
		for (int c = 0; c < OV_NCAT; ++c)
			if (kv.second.p[c] && OvName(c))
				std::cout << "EXP_OVH obs=" << kv.first
					  << " cat=" << OvName(c) << " pkts="
					  << kv.second.p[c] << " bytes="
					  << kv.second.b[c] << std::endl;
}

/* Link counters: transmitted packets/bytes and queue drops per device. */
struct LinkStat {
	uint64_t txp = 0, txb = 0, drops = 0;
};
static std::map<std::string, LinkStat> g_links;

static inline void OnTx(std::string name, Ptr<const Packet> p)
{
	LinkStat &s = g_links[name];
	s.txp++;
	s.txb += p->GetSize();
	OvCount(g_ovh["link_" + name], g_ovseq["link_" + name], p);
}
static inline void OnDrop(std::string name, Ptr<const Packet>)
{
	g_links[name].drops++;
}

static inline void Watch(const std::string &name, Ptr<NetDevice> d)
{
	g_links[name];
	d->TraceConnect("PhyTxBegin", name, MakeCallback(&OnTx));
	/*
	 * Count each drop once. A full DropTail queue fires its own "Drop"
	 * trace and then the device fires "MacTxDrop" for the same packet
	 * (PointToPointNetDevice::Send, CsmaNetDevice::Send), so tracing both
	 * doubled every queue drop. MacTxDrop alone covers queue overflow and
	 * a link that is down.
	 */
	d->TraceConnect("MacTxDrop", name, MakeCallback(&OnDrop));
}

static inline void PrintLinks()
{
	for (auto &kv : g_links)
		std::cout << "EXP_LINK name=" << kv.first << " tx_pkts="
			  << kv.second.txp << " tx_bytes=" << kv.second.txb
			  << " drops=" << kv.second.drops << std::endl;
}

static inline void PrintNodes(const std::string &role, NodeContainer n)
{
	WatchRole(role, n);
	std::cout << "EXP_NODES role=" << role << " ids=";
	for (uint32_t i = 0; i < n.GetN(); ++i)
		std::cout << (i ? "," : "") << n.Get(i)->GetId();
	std::cout << std::endl;
}

static inline void Finish(const Cfg &c, double wall0, double stop)
{
	Ipv4GlobalRoutingHelper::PopulateRoutingTables();
	LinuxStackHelper::PopulateRoutingTables();
	Simulator::Stop(Seconds(stop));
	double w1 = WallS();
	Simulator::Run();
	double w2 = WallS();
	PrintLinks();
	PrintOvh();
	struct rusage ru;
	getrusage(RUSAGE_SELF, &ru);
	std::cout << "EXP_DONE proto=" << c.proto << " sim_stop_s=" << stop
		  << " wall_setup_s=" << (w1 - wall0) << " wall_run_s="
		  << (w2 - w1) << " maxrss_kb=" << ru.ru_maxrss
		  << " seed_run=" << RngSeedManager::GetRun() << std::endl;
	/*
	 * All results are on disk at this point. Simulator::Destroy() tears
	 * down DCE with GMTP sockets still referenced by kernel tasks and can
	 * switch to a fiber whose stack was already freed (SIGSEGV in munmap
	 * after EXP_DONE, seen in B1). Skip the teardown: flush and leave.
	 */
	std::cout.flush();
	fflush(NULL);
	_exit(0);
}

} // namespace gmtpexp

#endif /* DCE_GMTP_EXP_H_ */
