#/*
echo "$0 is library file -> skip"
exit
*/

#include "inetfunc.h"

#include <regex.h>

static NetList* InitNetListElement() {
	NetList* a = (NetList*)malloc(sizeof(NetList));
	if (a == NULL) ABORT_NO_MEMORY;
	NetDevice* e = &(a->dev);
	e->ipv4 = NULL;
	e->metric = UINT32_MAX;
	e->isDefault = false;
	e->IPV4cidr = 0;
	e->device = NULL;
	e->linkspeed = NULL;
	e->routedNet = NULL;
	return a;
}

static NetList* InsertIntoNetListSorted(NetList* head, const char* device, const char* ipv4, int metric, bool isDefault, int IPV4cidr, const char* linkspeed, const char* routedNet) {
	if (head != NULL && head->dev.isDefault == false && isDefault == true) {
		abortMessage("assumption on the order of routes incorrect (assume default routes are listed first)");
	}
	if (head == NULL) {
		//Create Initial Element (List didn't exist previously)
		NetList* n = InitNetListElement();
		n->next = NULL;
		n->prev = NULL;
		//IP and device are the minimum set necessary (this was the original assumption, since reduced to just device)
		//originally I asserted device AND IP must be present, but on my linode VM in the entry denoting the default route there is no IP -> that assertion does not hold
		assert(device != NULL);
		if (asprintf(&(n->dev.device), "%s", device) == -1) ABORT_NO_MEMORY;
		if (ipv4 != NULL) {
			if (n->dev.ipv4 != NULL) free(n->dev.ipv4);
			if (asprintf(&(n->dev.ipv4), "%s", ipv4) == -1) ABORT_NO_MEMORY;
		}
		n->dev.metric = metric;
		n->dev.isDefault = isDefault;
		if (IPV4cidr != 0) n->dev.IPV4cidr = IPV4cidr;
		if (linkspeed != NULL) {
			if (n->dev.linkspeed != NULL) free(n->dev.linkspeed);
			if (asprintf(&(n->dev.linkspeed), "%s", linkspeed) == -1) ABORT_NO_MEMORY;
		}
		if (routedNet != NULL) {
			if (n->dev.routedNet != NULL) free(n->dev.routedNet);
			if (asprintf(&(n->dev.routedNet), "%s", routedNet) == -1) ABORT_NO_MEMORY;
		}

		return n;
	} else if (Compare(head->dev.device, device)) {
		//device name matches, this is just additional info
		head->dev.metric = metric;
		if (IPV4cidr != 0) head->dev.IPV4cidr = IPV4cidr;
		head->dev.isDefault = head->dev.isDefault || isDefault;
		if (linkspeed != NULL) {
			if (head->dev.linkspeed != NULL) free(head->dev.linkspeed);
			if (asprintf(&(head->dev.linkspeed), "%s", linkspeed) == -1) ABORT_NO_MEMORY;
		}
		if (routedNet != NULL) {
			if (head->dev.routedNet != NULL) free(head->dev.routedNet);
			if (asprintf(&(head->dev.routedNet), "%s", routedNet) == -1) ABORT_NO_MEMORY;
		}
		if (ipv4 != NULL) {
			if (head->dev.ipv4 != NULL) free(head->dev.ipv4);
			if (asprintf(&(head->dev.ipv4), "%s", ipv4) == -1) ABORT_NO_MEMORY;
		}
		return head;
	} else if (metric < head->dev.metric) {
		//The new Element has a lower metric -> insert before myself
		NetList* n = InitNetListElement();
		n->next = head;
		n->prev = head->prev;
		if (head->prev != NULL) head->prev->next = n;
		head->prev = n;
		//IP and device are the minimum set necessary (this was the original assumption, since reduced to just device)
		//originally I asserted device AND IP must be present, but on my linode VM in the entry denoting the default route there is no IP -> that assertion does not hold
		assert(device != NULL);
		if (asprintf(&(n->dev.device), "%s", device) == -1) ABORT_NO_MEMORY;
		if (ipv4 != NULL) {
			if (n->dev.ipv4 != NULL) free(n->dev.ipv4);
			if (asprintf(&(n->dev.ipv4), "%s", ipv4) == -1) ABORT_NO_MEMORY;
		}
		n->dev.metric = metric;
		n->dev.isDefault = isDefault;
		if (IPV4cidr != 0) n->dev.IPV4cidr = IPV4cidr;
		if (linkspeed != NULL) {
			if (n->dev.linkspeed != NULL) free(n->dev.linkspeed);
			if (asprintf(&(head->dev.linkspeed), "%s", linkspeed) == -1) ABORT_NO_MEMORY;
		}
		if (routedNet != NULL) {
			if (n->dev.routedNet != NULL) free(n->dev.routedNet);
			if (asprintf(&(n->dev.routedNet), "%s", routedNet) == -1) ABORT_NO_MEMORY;
		}
		return n;
	} else if (head->next == NULL) {
		//The New Element is NOT Equal to myself and is NOT alphabetically before myself (as per the earlier checks)
		//on top of that, I AM the LAST element, so I can simply create a new last element
		NetList* n = InitNetListElement();
		n->next = NULL;
		n->prev = head;

		head->next = n;
		//IP and device are the minimum set necessary (this was the original assumption, since reduced to just device)
		//originally I asserted device AND IP must be present, but on my linode VM in the entry denoting the default route there is no IP -> that assertion does not hold
		assert(device != NULL);
		if (asprintf(&(n->dev.device), "%s", device) == -1) ABORT_NO_MEMORY;
		if (ipv4 != NULL) {
			if (n->dev.ipv4 != NULL) free(n->dev.ipv4);
			if (asprintf(&(n->dev.ipv4), "%s", ipv4) == -1) ABORT_NO_MEMORY;
		}
		n->dev.metric = metric;
		n->dev.isDefault = isDefault;
		if (IPV4cidr != 0) n->dev.IPV4cidr = IPV4cidr;
		if (linkspeed != NULL) {
			if (n->dev.linkspeed != NULL) free(n->dev.linkspeed);
			if (asprintf(&(head->dev.linkspeed), "%s", linkspeed) == -1) ABORT_NO_MEMORY;
		}
		if (routedNet != NULL) {
			if (n->dev.routedNet != NULL) free(n->dev.routedNet);
			if (asprintf(&(n->dev.routedNet), "%s", routedNet) == -1) ABORT_NO_MEMORY;
		}

		return head;
	} else {
		//The New Element is NOT Equal to myself and is NOT alphabetically before myself (as per the earlier checks)
		//This time there IS another element after myself -> defer to it.
		head->next = InsertIntoNetListSorted(head->next, device, ipv4, metric, isDefault, IPV4cidr, linkspeed, routedNet);
		return head;
	}
}

static bool DecodeIfaceSpeed(char* result, char** ret) {
	TerminateStrOn(result, DEFAULT_TERMINATORS);
	if (Compare(result, "")) {
		return false;
	}

	if (Compare(result, "1000M")) {
		if (asprintf(ret, "1G") == -1) ABORT_NO_MEMORY;
		return true;
	} else if (Compare(result, "2500M")) {
		if (asprintf(ret, "2.5G") == -1) ABORT_NO_MEMORY;
		return true;
	} else if (Compare(result, "5000M")) {
		if (asprintf(ret, "5G") == -1) ABORT_NO_MEMORY;
		return true;
	} else if (Compare(result, "10000M")) {
		if (asprintf(ret, "10G") == -1) ABORT_NO_MEMORY;
		return true;
	} else if (Compare(result, "unknown")) {
		return false;
	} else {
		if (asprintf(ret, "%s", result) == -1) ABORT_NO_MEMORY;
		return true;
	}
}

static char* GetIfaceSpeed_int(const char* iface, bool IsBackup) {
	char* ret = NULL;
	int size = 32;
	char* result = (char*)malloc(sizeof(char) * size);
	if (result == NULL) ABORT_NO_MEMORY;
	char* command;
	if (IsBackup) {
		if (asprintf(&command, "ethtool %s 2>/dev/null | sed -nE 's~\tSpeed: ([0-9]+)([kMG])b/s~\\1\\2~p'", iface) == -1) ABORT_NO_MEMORY;
	} else {
		if (asprintf(&command, "nmcli -g CAPABILITIES.SPEED device show %s 2>/dev/null | sed -E 's~([0-9]+) (.).+~\\1\\2~'", iface) == -1) ABORT_NO_MEMORY;
	}
	FILE* fp = popen(command, "r");
	if (fp == NULL) {
		fprintf(stderr, "failed running process %s\n", command);
	} else {
		while (fgets(result, size - 1, fp) != NULL) {
			if (DecodeIfaceSpeed(result, &ret)) {
				break;
			}
		}
		pclose(fp);
		fp = NULL;
	}
	free(command);
	command = NULL;
	free(result);
	result = NULL;
	return ret;
}

static char* GetIfaceSpeed(const char* iface) {
	char* val = GetIfaceSpeed_int(iface, false);
	if (val == NULL) {
		val = GetIfaceSpeed_int(iface, true);
	}
	return val;
}

IpTransportStruct GetBaseIPString() {
	IpTransportStruct ret;
	ret.BasicIPInfo = NULL;
	ret.AdditionalIPInfo = NULL;
	ret.RouteInfo = NULL;
	if (!CONFIG_PROMPT_NETWORK) {
		//network is completely configured out -> do not do anything
		ret.BasicIPInfo = malloc(sizeof(char) * 1);
		if (ret.BasicIPInfo == NULL) ABORT_NO_MEMORY;
		ret.AdditionalIPInfo = malloc(sizeof(char) * 1);
		if (ret.AdditionalIPInfo == NULL) ABORT_NO_MEMORY;
		ret.RouteInfo = malloc(sizeof(char) * 1);
		if (ret.RouteInfo == NULL) ABORT_NO_MEMORY;
		ret.BasicIPInfo[0] = 0x00;
		ret.AdditionalIPInfo[0] = 0x00;
		ret.RouteInfo[0] = 0x00;
		return ret;
	}
	uint8_t RouteRegexGroupCount = 16;
	regmatch_t RouteRegexGroups[RouteRegexGroupCount];
	regex_t RouteRegex;
	const char* RouteRegexString = "^(((default) via ([0-9.]+))|(([0-9.]+)/([0-9]+))) dev ([^ ]+)( proto [^ ]+)?( scope link)?( src ([0-9.]+))?( metric ([0-9]+))?( linkdown)? *$";
	//the following was the originally used regex. it doesn't work for some network configurations (such as linode/akamai VPS).
	//const char* RouteRegexString="^(((default) via ([0-9.]+))|(([0-9.]+)/([0-9]+))).*?dev ([^ ]+).*?src ([0-9.]+)( metric ([0-9]+))?( linkdown)?.*$";
	//root cause is problems with lazy matching, which C's regex engine can't do.
	//I had copilot help in creating a "fixed" regex. the output was this:
	//^((default[[:space:]]+via[[:space:]]+([0-9.]+))|(([0-9.]+)/([0-9]+)))[[:space:]]+dev[[:space:]]+([^[:space:]]+)(?:[[:space:]]+proto[[:space:]]+[^[:space:]]+)?(?:[[:space:]]+scope[[:space:]]+link)?(?:[[:space:]]+src[[:space:]]+([0-9.]+))?(?:[[:space:]]+metric[[:space:]]+([0-9]+))?(?:[[:space:]]+linkdown)?[[:space:]]*$
	//I have then refined it into a much smaller and more easily readable version which is in use currently
#define RouteIsDefaultIndex 3
#define RouteNextHopIndex	4
#define RouteRoutedNetIndex 6
#define RouteCidrIndex		7
#define RouteDeviceIndex	8
#define RouteIpIndex		12
#define RouteMetricIndex	14
#define RouteLinkDownIndex	15
	int IpRegexReturnCode;
	IpRegexReturnCode = regcomp(&RouteRegex, RouteRegexString, REG_EXTENDED | REG_NEWLINE);
	if (IpRegexReturnCode) {
		char* regErrorBuf = (char*)malloc(sizeof(char) * 1024);
		if (regErrorBuf == NULL) ABORT_NO_MEMORY;
		regerror(IpRegexReturnCode, &RouteRegex, regErrorBuf, 1024);
		printf("Could not compile regular expression '%s'. [%i(%s)]\n", RouteRegexString, IpRegexReturnCode, regErrorBuf);
		fflush(stdout);
		free(regErrorBuf);
		exit(1);
	};

	NetList* head = NULL;

	int size = 1024;
	char* result = (char*)malloc(sizeof(char) * size);
	if (result == NULL) ABORT_NO_MEMORY;
	const char* command = "ip route show";
	FILE* fp = popen(command, "r");
	if (fp == NULL) {
		fprintf(stderr, "failed running process %s\n", command);
		fflush(stderr);
	} else {
		while (fgets(result, size - 1, fp) != NULL) {
			TerminateStrOn(result, DEFAULT_TERMINATORS);
			//fprintf(stderr, "\n\nresult: %s\n", result);
			IpRegexReturnCode = regexec(&RouteRegex, result, RouteRegexGroupCount, RouteRegexGroups, 0);
			//man regex (3): regexec() returns zero for a successful match or REG_NOMATCH for failure.
			if (IpRegexReturnCode == 0) {
				if (RouteRegexGroups[RouteLinkDownIndex].rm_eo - RouteRegexGroups[RouteLinkDownIndex].rm_so != 0) {
					continue; //linkdown matched, therefore ignore this interface
				}
				char* device = NULL;
				char* ipv4 = NULL;
				uint32_t metric = UINT32_MAX;
				bool isDefault = false;
				int IPV4cidr = 0;
				char* linkspeed = NULL;
				char* routednet = NULL;
				int len;
				len = RouteRegexGroups[RouteDeviceIndex].rm_eo - RouteRegexGroups[RouteDeviceIndex].rm_so;
				if (len > 0) {
					device = malloc(sizeof(char) * (len + 1));
					if (device == NULL) ABORT_NO_MEMORY;
					strncpy(device, result + RouteRegexGroups[RouteDeviceIndex].rm_so, len);
					device[len] = 0x00;
				}
				if (RouteRegexGroups[RouteIsDefaultIndex].rm_eo - RouteRegexGroups[RouteIsDefaultIndex].rm_so != 0) {
					isDefault = true;
				} else if (CONFIG_PROMPT_NET_LINKSPEED) {
					linkspeed = GetIfaceSpeed(device);
				}
				len = RouteRegexGroups[RouteIpIndex].rm_eo - RouteRegexGroups[RouteIpIndex].rm_so;
				if (len > 0) {
					ipv4 = malloc(sizeof(char) * (len + 1));
					if (ipv4 == NULL) ABORT_NO_MEMORY;
					strncpy(ipv4, result + RouteRegexGroups[RouteIpIndex].rm_so, len);
					ipv4[len] = 0x00;
				}
				len = RouteRegexGroups[RouteMetricIndex].rm_eo - RouteRegexGroups[RouteMetricIndex].rm_so;
				if (len > 0) {
					char* temp = malloc(sizeof(char) * (len + 1));
					if (temp == NULL) ABORT_NO_MEMORY;
					strncpy(temp, result + RouteRegexGroups[RouteMetricIndex].rm_so, len);
					temp[len] = 0x00;
					metric = atoi(temp);
					free(temp);
					temp = NULL;
				}
				len = RouteRegexGroups[RouteCidrIndex].rm_eo - RouteRegexGroups[RouteCidrIndex].rm_so;
				if (len > 0) {
					char* temp = malloc(sizeof(char) * (len + 1));
					if (temp == NULL) ABORT_NO_MEMORY;
					strncpy(temp, result + RouteRegexGroups[RouteCidrIndex].rm_so, len);
					temp[len] = 0x00;
					IPV4cidr = atoi(temp);
					free(temp);
					temp = NULL;
				}
				len = RouteRegexGroups[RouteRoutedNetIndex].rm_eo - RouteRegexGroups[RouteRoutedNetIndex].rm_so;
				if (len > 0) {
					routednet = malloc(sizeof(char) * (len + 1));
					if (routednet == NULL) ABORT_NO_MEMORY;
					strncpy(routednet, result + RouteRegexGroups[RouteRoutedNetIndex].rm_so, len);
					routednet[len] = 0x00;
				}
				head = InsertIntoNetListSorted(head, device, ipv4, metric, isDefault, IPV4cidr, linkspeed, routednet);
				if (device != NULL) free(device);
				if (ipv4 != NULL) free(ipv4);
				if (linkspeed != NULL) free(linkspeed);
				if (routednet != NULL) free(routednet);
			} else if (DIPFALSCHEISSER_WARNINGS) {
				printf("ERROR: IP regex match returned %i -> IP info is likely incomplete\n", IpRegexReturnCode);
				fflush(stdout);
			}
		}
		pclose(fp);
	}
	free(result);

	//res (the standard IP line should get (numDefaultRoutes+1)*(1+4+8+4+1+(4*(3+1))+4+4+4+4+4+16+4) bytes (space+CSI+device+CSI+:IP+CSI+CIDR+CSI+CSI+CSI+metric+CSI) bytes
	//the above calculation comes out to 74 bytes, so if I assign 80 bytes per default route I should be fine.
	//for the third line (the actual route display) I think 6+(10*numNonDefaultRoutes)+30 should be enough

	NetList* current = head;
	int numDefaultRoutes = 0;
	int numNonDefaultRoutes = 0;
	uint32_t lowestMetric = UINT32_MAX;
	while (current != NULL) {
		if (current->dev.isDefault) {
			numDefaultRoutes++;
		} else {
			numNonDefaultRoutes++;
		}
		if (current->dev.metric < lowestMetric) {
			lowestMetric = current->dev.metric;
		}
		//fprintf(stderr, "%s:%s/%i@%i %i for %s\n", current->dev.device, current->dev.ipv4, current->dev.IPV4cidr, current->dev.metric, current->dev.isDefault, current->dev.routedNet);
		current = current->next;
	}
	//this works under the assumption the interface names aren't longer than 10-ish characters (true for enpXsY or eth1 or wlpXsY) however at some point my system decided to use wlx18d6c713d4ed as default device and completely ignore the actually fast wifi chip

	int basicIPStringLen = (CONFIG_PROMPT_NET_IFACE ? (13 + (112 * numDefaultRoutes)) : 1);
	ret.BasicIPInfo = malloc(sizeof(char) * basicIPStringLen);
	if (ret.BasicIPInfo == NULL) ABORT_NO_MEMORY;
	int nondefaultIpStringLen = (CONFIG_PROMPT_NET_ADDITIONAL ? (2 + (numNonDefaultRoutes * 96)) : 1);
	ret.AdditionalIPInfo = malloc(sizeof(char) * nondefaultIpStringLen);
	if (ret.AdditionalIPInfo == NULL) ABORT_NO_MEMORY;
	int rounteinfoLen = (CONFIG_PROMPT_NET_ROUTE ? (48 + (16 * numDefaultRoutes)) : 1);
	ret.RouteInfo = malloc(sizeof(char) * rounteinfoLen);
	if (ret.RouteInfo == NULL) ABORT_NO_MEMORY;

	int baseIPlenUsed = 0;
	int nondefaultLenUsed = 0;
	int routeinfoLenUsed = 0;

	current = head;
	if (CONFIG_PROMPT_NET_IFACE && numDefaultRoutes == 0) {
		baseIPlenUsed += snprintf(ret.BasicIPInfo + baseIPlenUsed, basicIPStringLen - (baseIPlenUsed + 1), " \e[4mNC\e[0m");
	}
	for (int i = 0; i < numDefaultRoutes; i++) {
		if (CONFIG_PROMPT_NET_IFACE) {
			baseIPlenUsed += snprintf(ret.BasicIPInfo + baseIPlenUsed, basicIPStringLen - (baseIPlenUsed + 1), " %s%s\e[0m:%s", (current->dev.metric == lowestMetric && numDefaultRoutes > 1 ? "\e[4m" : ""), current->dev.device, current->dev.ipv4);
			if (current->dev.IPV4cidr > 0) {
				baseIPlenUsed += snprintf(ret.BasicIPInfo + baseIPlenUsed, basicIPStringLen - (baseIPlenUsed + 1), "\e[38;5;244m/%i\e[0m", current->dev.IPV4cidr);
			}
			if (numDefaultRoutes > 1) {
				baseIPlenUsed += snprintf(ret.BasicIPInfo + baseIPlenUsed, basicIPStringLen - (baseIPlenUsed + 1), "\e[38;5;240m\e[2m\e[3m~%u\e[0m", current->dev.metric);
			}
			if (current->dev.linkspeed != NULL) {
				baseIPlenUsed += snprintf(ret.BasicIPInfo + baseIPlenUsed, basicIPStringLen - (baseIPlenUsed + 1), "\e[38;5;238m\e[2m@%s\e[0m", current->dev.linkspeed);
			}
		}
		current = current->next;
	}

	//I intentionally didn't reset current
	if (numNonDefaultRoutes > 0 && CONFIG_PROMPT_NET_ADDITIONAL) {
		for (int i = 0; i < numNonDefaultRoutes; i++) {
			nondefaultLenUsed += snprintf(ret.AdditionalIPInfo + nondefaultLenUsed, nondefaultIpStringLen - (nondefaultLenUsed + 1), " \e[38;5;244m\e[3m%s:%s\e[0m", current->dev.device, current->dev.ipv4);
			if (current->dev.IPV4cidr > 0) {
				nondefaultLenUsed += snprintf(ret.AdditionalIPInfo + nondefaultLenUsed, nondefaultIpStringLen - (nondefaultLenUsed + 1), "\e[38;5;240m\e[2m\e[3m/%i\e[0m", current->dev.IPV4cidr);
			}
			current = current->next;
		}
	}

	if (numNonDefaultRoutes > 0 && CONFIG_PROMPT_NET_ROUTE) {
		current = head;
		routeinfoLenUsed += snprintf(ret.RouteInfo + routeinfoLenUsed, rounteinfoLen - (routeinfoLenUsed + 1), " \e[38;5;244m\e[2m*->");
		if (numDefaultRoutes > 1) {
			routeinfoLenUsed += snprintf(ret.RouteInfo + routeinfoLenUsed, rounteinfoLen - (routeinfoLenUsed + 1), "{");
		}
		routeinfoLenUsed += snprintf(ret.RouteInfo + routeinfoLenUsed, rounteinfoLen - (routeinfoLenUsed + 1), "%s", current->dev.device);
		current = current->next;
		for (int i = 1; i < numDefaultRoutes; i++) {
			routeinfoLenUsed += snprintf(ret.RouteInfo + routeinfoLenUsed, rounteinfoLen - (routeinfoLenUsed + 1), ",%s", current->dev.device);
			current = current->next;
		}
		if (numDefaultRoutes > 1) {
			routeinfoLenUsed += snprintf(ret.RouteInfo + routeinfoLenUsed, rounteinfoLen - (routeinfoLenUsed + 1), "}");
		}
		if (numNonDefaultRoutes > 1) {
			routeinfoLenUsed += snprintf(ret.RouteInfo + routeinfoLenUsed, rounteinfoLen - (routeinfoLenUsed + 1), "  %i additional routes\e[0m", numNonDefaultRoutes);
		} else {
			routeinfoLenUsed += snprintf(ret.RouteInfo + routeinfoLenUsed, rounteinfoLen - (routeinfoLenUsed + 1), " %s/%i->%s\e[0m", current->dev.routedNet, current->dev.IPV4cidr, current->dev.device);
		}
	}
	assert(baseIPlenUsed < basicIPStringLen);
	assert(nondefaultLenUsed < nondefaultIpStringLen);
	assert(routeinfoLenUsed < rounteinfoLen);
	ret.BasicIPInfo[baseIPlenUsed] = 0x00;
	ret.AdditionalIPInfo[nondefaultLenUsed] = 0x00;
	ret.RouteInfo[routeinfoLenUsed] = 0x00;
#ifdef DEBUG
	fprintf(stderr, ">%s< (%i/%i)\n>%s< (%i/%i)\n>%s< (%i/%i)\n", ret.BasicIPInfo, baseIPlenUsed, basicIPStringLen, ret.AdditionalIPInfo, nondefaultLenUsed, nondefaultIpStringLen, ret.RouteInfo, routeinfoLenUsed, rounteinfoLen);
#endif

	regfree(&RouteRegex);
	return ret;
}
