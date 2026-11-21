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
}
static inline void OnDrop(std::string name, Ptr<const Packet>)
{
	g_links[name].drops++;
}

static inline void Watch(const std::string &name, Ptr<NetDevice> d)
{
	g_links[name];
	d->TraceConnect("PhyTxBegin", name, MakeCallback(&OnTx));
	Ptr<PointToPointNetDevice> p2p = DynamicCast<PointToPointNetDevice>(d);
	if (p2p) {
		p2p->GetQueue()->TraceConnect("Drop", name, MakeCallback(&OnDrop));
		p2p->TraceConnect("MacTxDrop", name, MakeCallback(&OnDrop));
	}
	Ptr<CsmaNetDevice> cs = DynamicCast<CsmaNetDevice>(d);
	if (cs) {
		cs->GetQueue()->TraceConnect("Drop", name, MakeCallback(&OnDrop));
		cs->TraceConnect("MacTxDrop", name, MakeCallback(&OnDrop));
	}
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
