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

static void RunIp(ns3::Ptr<ns3::Node> node, ns3::Time at, std::string str)
{
	ns3::DceApplicationHelper process;
	ns3::ApplicationContainer apps;
	process.SetBinary("ip");
	process.SetStackSize(1 << 16);
	process.ResetArguments();
	process.ParseArguments(str.c_str());
	apps = process.Install(node);
	apps.Start(at);
}

static void RunGtmpInter(ns3::Ptr<ns3::Node> node, ns3::Time at, std::string str)
{
	ns3::DceApplicationHelper process;
	ns3::ApplicationContainer apps;
	process.SetBinary("gmtp-inter");
	process.SetStackSize(1 << 16);
	process.ResetArguments();
	process.ParseArguments(str.c_str());
	apps = process.Install(node);
	apps.Start(at);
}

#endif /* DCE_GMTP_H_ */
