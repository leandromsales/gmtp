/*
 * dce-gmtp.h
 *
 *  Created on: 29/07/2015
 *      Author: wendell
 */

#ifndef DCE_GMTP_H_
#define DCE_GMTP_H_

#include "ns3/network-module.h"
#include "ns3/core-module.h"
#include "ns3/dce-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"

static void print_node_app(const ns3::Ptr<ns3::Node> &node, ns3::Time at, std::string appname) {
	std::cout << "Node " << node->GetId() << " >> " << appname << " at "
			<< at.GetSeconds() << "s" << std::endl;
}

static void print_node_app(const ns3::NodeContainer &container, ns3::Time at, std::string appname) {
	ns3::NodeContainer::Iterator i;
	for(i = container.Begin(); i != container.End(); ++i) {
		print_node_app((const ns3::Ptr<ns3::Node>) (*i), at, appname);
	}
}

static void RunApp(std::string appname,
		ns3::Ptr<ns3::Node> node, ns3::Time at,
		std::string args, uint32_t stackSize)
{
	//print_node_app(node, at, appname);
	ns3::DceApplicationHelper process;
	ns3::ApplicationContainer apps;
	process.SetBinary(appname.c_str());
	process.SetStackSize(stackSize);
	process.ResetArguments();
	process.ParseArguments(args.c_str());
	apps = process.Install(node);
	apps.Start(at);
}

static void RunApp(std::string appname,
		ns3::NodeContainer nodes, ns3::Time at,
		std::string args, uint32_t stackSize)
{
	//print_node_app(nodes, at, appname);
	ns3::DceApplicationHelper process;
	ns3::ApplicationContainer apps;
	process.SetBinary(appname.c_str());
	process.SetStackSize(stackSize);
	process.ResetArguments();
	process.ParseArguments(args.c_str());
	apps = process.Install(nodes);
	apps.Start(at);
}

static void RunApp(std::string appname,
		ns3::Ptr<ns3::Node> node, ns3::Time at, uint32_t stackSize)
{
	//print_node_app(node, at, appname);
	ns3::DceApplicationHelper process;
	ns3::ApplicationContainer apps;
	process.SetBinary(appname.c_str());
	process.SetStackSize(stackSize);
	process.ResetArguments();
	apps = process.Install(node);
	apps.Start(at);
}

static void RunApp(std::string appname,
		ns3::NodeContainer nodes, ns3::Time at, uint32_t stackSize)
{
	//print_node_app(nodes, at, appname);
	ns3::DceApplicationHelper process;
	ns3::ApplicationContainer apps;
	process.SetBinary(appname.c_str());
	process.SetStackSize(stackSize);
	process.ResetArguments();
	apps = process.Install(nodes);
	apps.Start(at);
}

static void RunIp(ns3::Ptr<ns3::Node> node, ns3::Time at, std::string args)
{
	RunApp("ip", node, at, args, 1 << 16);
}

static void RunIp(ns3::NodeContainer nodes, ns3::Time at, std::string args)
{
	RunApp("ip", nodes, at, args, 1 << 16);
}

static void RunGtmpInter(ns3::Ptr<ns3::Node> node, ns3::Time at, std::string args)
{
	RunApp("gmtp-inter", node, at, args, 1 << 16);
}

static void RunGtmpInter(ns3::NodeContainer nodes, ns3::Time at, std::string args)
{
	RunApp("gmtp-inter", nodes, at, args, 1 << 16);
}

static void RunAppMulti(std::string appname, ns3::NodeContainer nodes,
		double start, std::string args, uint32_t stackSize,
		uint32_t factor)
{
	ns3::DceApplicationHelper process;
	ns3::ApplicationContainer apps;
	process.SetBinary(appname);
	process.SetStackSize(stackSize);
	process.ResetArguments();
	process.ParseArguments(args);

	int i = 0;
	int j = 1;
	int k = nodes.GetN() / 30;
	double t = start;
	double step = 0.5;
	std::cout << "k = " << k << std::endl;
	for(; j < k; ++j, t += step) {
		std::cout << "i = " << i << ", j = " << j << ", t = " << t << std::endl;
		for(; i < (j * nodes.GetN() / k); ++i) {
			apps = process.Install(nodes.Get(i));
			apps.Start(ns3::Seconds(t));
		}
	}

	std::cout << "i = " << i << ", j = " << j << ", t = " << t << std::endl;
	t += step;
	for(; i < nodes.GetN(); ++i) {
		apps = process.Install(nodes.Get(i));
		apps.Start(ns3::Seconds(t));
	}
	std::cout << "i = " << i << ", j = " << j << ", t = " << t << std::endl;
}


#endif /* DCE_GMTP_H_ */
