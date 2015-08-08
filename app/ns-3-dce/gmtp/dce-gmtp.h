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

static void RunApp(std::string appname,
		ns3::Ptr<ns3::Node> node, ns3::Time at,
		std::string args, uint32_t stackSize)
{
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

#endif /* DCE_GMTP_H_ */
