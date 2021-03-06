/*
 * dev.c
 *
 *  Created on: Jun 23, 2010
 */

#include "dev.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <syslog.h>

#include <linux/if.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/sockios.h>
#include <linux/hdlc.h>
#include <linux/sockios.h>
#include <linux/netlink.h>

#include "libnetlink/libnetlink.h"
/*#include <ll_map.h>*/
#include <fnmatch.h>

#include "options.h"
#include "typedefs.h"
#include "defines.h"
#include "ip.h"
#include "error.h"
#include "device.h"

#ifdef OPTION_MODEM3G
#include "modem3G.h"
#endif

#ifdef OPTION_EFM
#include "efm.h"
#endif

static int _librouter_dev_get_ctrlfd(void)
{
	int s_errno;
	int fd;

	fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd >= 0)
		return fd;

	s_errno = errno;

	fd = socket(PF_PACKET, SOCK_DGRAM, 0);
	if (fd >= 0)
		return fd;

	fd = socket(PF_INET6, SOCK_DGRAM, 0);
	if (fd >= 0)
		return fd;

	errno = s_errno;
	perror("Cannot create control socket");

	return -1;
}

static int _librouter_dev_chflags(char *dev, __u32 flags, __u32 mask)
{
	struct ifreq ifr;
	int fd;
	int err;

	strcpy(ifr.ifr_name, dev);
	fd = _librouter_dev_get_ctrlfd();
	if (fd < 0)
		return -1;

	err = ioctl(fd, SIOCGIFFLAGS, &ifr);
	if (err) {
		librouter_pr_error(1, "do_chflags %s SIOCGIFFLAGS",
		                ifr.ifr_name);
		close(fd);
		return -1;
	}

	if ((ifr.ifr_flags ^ flags) & mask) {
		ifr.ifr_flags &= ~mask;
		ifr.ifr_flags |= mask & flags;
		err = ioctl(fd, SIOCSIFFLAGS, &ifr);
		if (err)
			perror("SIOCSIFFLAGS");
	}

	close(fd);

	return err;
}

int librouter_dev_get_flags(char *dev, __u32 *flags)
{
	struct ifreq ifr;
	int fd;
	int err;

	strcpy(ifr.ifr_name, dev);
	fd = _librouter_dev_get_ctrlfd();

	if (fd < 0)
		return -1;

	err = ioctl(fd, SIOCGIFFLAGS, &ifr);
	if (err) {
		//pr_error(1, "dev_get_flags %s SIOCGIFFLAGS", ifr.ifr_name);
		close(fd);
		return -1;
	}

	*flags = ifr.ifr_flags;
	close(fd);

	return err;
}

//------------------------------------------------------------------------------

int librouter_dev_set_qlen(char *dev, int qlen)
{
	struct ifreq ifr;
	int s;

	s = _librouter_dev_get_ctrlfd();
	if (s < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, dev);
	ifr.ifr_qlen = qlen;

	if (ioctl(s, SIOCSIFTXQLEN, &ifr) < 0) {
		perror("SIOCSIFXQLEN");
		close(s);
		return -1;
	}

	close(s);

	return 0;
}

int librouter_dev_get_qlen(char *dev)
{
	struct ifreq ifr;
	int s;

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return 0;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, dev);

	if (ioctl(s, SIOCGIFTXQLEN, &ifr) < 0) {
		//printf("dev_get_qlen: %s ", dev); /* !!! */
		//perror("SIOCGIFXQLEN");
		close(s);
		return 0;
	}

	close(s);

	return ifr.ifr_qlen;
}

int librouter_dev_set_mtu(char *dev, int mtu)
{
	struct ifreq ifr;
	int s;

	if (librouter_dev_get_link(dev)) {
		printf("%% Interface must be shutdown first\n");
		syslog(LOG_ERR, "Must shutdown interface to configure MTU\n");
		return -1;
	}

	s = _librouter_dev_get_ctrlfd();
	if (s < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, dev);
	ifr.ifr_mtu = mtu;

	if (ioctl(s, SIOCSIFMTU, &ifr) < 0) {
		perror("SIOCSIFMTU");
		close(s);
		return -1;
	}

	close(s);

	return 0;
}

int librouter_dev_get_mtu(char *dev)
{
	struct ifreq ifr;
	int s;

	s = _librouter_dev_get_ctrlfd();
	if (s < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, dev);

	if (ioctl(s, SIOCGIFMTU, &ifr) < 0) {
		/* error("SIOCGIFMTU"); */
		close(s);
		return 1500; /* Return default value in error */
	}

	close(s);

	return ifr.ifr_mtu;
}

int librouter_dev_set_link_down(char *dev)
{
	int ret = 0;
	dev_family *fam = librouter_device_get_family_by_name(dev, str_linux);

	switch (fam->type) {
#ifdef OPTION_MODEM3G
	case ppp:
		librouter_ppp_backupd_set_param_infile(dev,SHUTD_STR,"yes");
		ret = librouter_ppp_reload_backupd();
		break;
#endif
	default:
		ret = _librouter_dev_chflags(dev, 0, IFF_UP);
		break;
	}

	return ret;

}

int librouter_dev_set_link_up(char *dev)
{
	int ret = 0;
	dev_family *fam = librouter_device_get_family_by_name(dev, str_linux);

	switch (fam->type) {
#ifdef OPTION_MODEM3G
	case ppp:
		librouter_ppp_backupd_set_param_infile(dev,SHUTD_STR,"no");
		ret = librouter_ppp_reload_backupd();
		break;
#endif
	default:
		ret = _librouter_dev_chflags(dev, IFF_UP, IFF_UP);
		break;
	}

	return ret;
}

int librouter_dev_get_link(char *dev)
{
	u32 flags;

	if (librouter_dev_get_flags(dev, &flags) < 0)
		return (-1);

	return (flags & IFF_UP);
}

int librouter_dev_get_link_running(char *dev)
{
	u32 flags;

	if (librouter_dev_get_flags(dev, &flags) < 0)
		return (-1);

	return (flags & IFF_RUNNING);
}


int librouter_dev_get_hwaddr(char *dev, unsigned char *hwaddr)
{
	struct ifreq ifr;
	int skfd;

	if (hwaddr == NULL)
		return 0;

	/* Create a socket to the INET kernel. */
	if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		librouter_pr_error(1, "socket");
		return (-1);
	}

	strcpy(ifr.ifr_name, dev);
	ifr.ifr_addr.sa_family = AF_INET;

	if (ioctl(skfd, SIOCGIFHWADDR, &ifr) < 0)
		memset(hwaddr, 0, 6);
	else
		memcpy(hwaddr, ifr.ifr_hwaddr.sa_data, 6);

	close(skfd);

	return 0;
}

int librouter_dev_change_name(char *ifname, char *new_ifname)
{
	struct ifreq ifr;
	int fd;

	/* Create a socket to the INET kernel. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		librouter_pr_error(1, "socket");
		return (-1);
	}

	strcpy(ifr.ifr_name, ifname);
	strcpy(ifr.ifr_newname, new_ifname);

	if (ioctl(fd, SIOCSIFNAME, &ifr)) {
		librouter_pr_error(1, "%s: SIOCSIFNAME", ifname);
		close(fd);
		return (-1);
	}

	close(fd);

	return 0;
}

int librouter_dev_exists(char *dev)
{
	struct ifreq ifr;
	int fd, found;

	/* Create a socket to the INET kernel. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		librouter_pr_error(1, "socket");
		return (0);
	}

	strcpy(ifr.ifr_name, dev);

	found = (ioctl(fd, SIOCGIFFLAGS, &ifr) >= 0);

	close(fd);

	return found;
}

int librouter_dev_clear_interface_counters(char *dev)
{
	struct ifreq ifr;
	int fd, err;

	dev_dbg("Clearing counters for %s\n", dev);

	if ((fd=socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		librouter_pr_error(1, "socket");
		return (0);
	}

	strcpy(ifr.ifr_name, dev);
	err=ioctl(fd, SIOCCLRIFSTATS, &ifr);

	if ((err < 0) && (errno != ENODEV)) {
		librouter_pr_error(1, "SIOCCLRIFSTATS");
	}

	close(fd);

	return err;
}

char *librouter_dev_get_description(char *dev)
{
	FILE *f;
	char file[240];
	long len;
	char *description;

	sprintf(file, DESCRIPTION_FILE, dev);

	f = fopen(file, "rt");
	if (!f)
		return NULL;

	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (len == 0) {
		fclose(f);
		return NULL;
	}

	description = malloc(len + 1);

	if (description == 0) {
		fclose(f);
		return NULL;
	}

	fgets(description, len + 1, f);
	fclose(f);

	return description;
}

int librouter_dev_add_description(char *dev, char *description)
{
	FILE *f;
	char file[50];

	sprintf(file, DESCRIPTION_FILE, dev);
	f = fopen(file, "wt");

	if (!f)
		return -1;

	fputs(description, f);
	fclose(f);

	return 0;
}

int librouter_dev_del_description(char *dev)
{
	char file[50];

	sprintf(file, DESCRIPTION_FILE, dev);
	unlink(file);

	return 0;
}

/* arp */
/*
 arp -s x.x.x.x Y:Y:Y:Y:Y:Y:Y:Y

 ip neigh add x.x.x.x lladdr H.H.H dev ifname
 ip neigh show
 iproute2/ip/ipneigh.c: ipneigh_modify()
 net-tools/arp.c

 arp 1.1.1.1 ?
 H.H.H 48-bit hardware address of ARP entry
 */

/* Delete an entry from the ARP cache. */
/* <ipaddress> */
int librouter_arp_del(char *host)
{
	struct arpreq req;
	struct sockaddr sa;
	struct sockaddr_in *sin;
	int flags = 0;
	int err;
	int sockfd = 0;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		exit(-1);
	}

	memset((char *) &req, 0, sizeof(req));
	sin = (struct sockaddr_in *) &sa;
	sin->sin_family = AF_INET;
	sin->sin_port = 0;

	if (inet_aton(host, &sin->sin_addr) == 0) {
		return (-1);
	}

	memcpy((char *) &req.arp_pa, (char *) &sa, sizeof(struct sockaddr));
	req.arp_flags = ATF_PERM;

	if (flags == 0)
		flags = 3;
	strcpy(req.arp_dev, "");
	err = -1;

	/* Call the kernel. */
	if (flags & 2) {
		if ((err = ioctl(sockfd, SIOCDARP, &req) < 0)) {
			if (errno == ENXIO) {
				if (flags & 1)
					goto nopub;
				printf("%% no ARP entry for %s\n", host);
				return (-1);
			}

			perror("%% error"); /* SIOCDARP(priv) */
			return (-1);
		}
	}

	if ((flags & 1) && (err)) {
		nopub: req.arp_flags |= ATF_PUBL;
		if (ioctl(sockfd, SIOCDARP, &req) < 0) {
			if (errno == ENXIO) {
				printf("%% no ARP entry for %s\n", host);
				return (-1);
			}
			printf("%% no ARP entry for %s\n", host);
#if 0
			perror("SIOCDARP(pub)");
#endif
			return (-1);
		}
	}

	return (0);
}

/* Input an Ethernet address and convert to binary. */
static int _librouter_arp_in_ether(char *bufp, struct sockaddr *sap)
{
	unsigned char *ptr;
	char c, *orig;
	int i;
	unsigned val;

	sap->sa_family = ARPHRD_ETHER;
	ptr = (unsigned char *) sap->sa_data;

	i = 0;
	orig = bufp;
	while ((*bufp != '\0') && (i < ETH_ALEN)) {
		val = 0;
		c = *bufp++;

		if (isdigit(c)) {
			val = c - '0';
		} else if (c >= 'a' && c <= 'f') {
			val = c - 'a' + 10;
		} else if (c >= 'A' && c <= 'F') {
			val = c - 'A' + 10;
		} else {
			errno = EINVAL;
			return (-1);
		}

		val <<= 4;
		c = *bufp;

		if (isdigit(c)) {
			val |= c - '0';
		} else if (c >= 'a' && c <= 'f') {
			val |= c - 'a' + 10;
		} else if (c >= 'A' && c <= 'F') {
			val |= c - 'A' + 10;
		} else if (c == ':' || c == 0) {
			val >>= 4;
		} else {
			errno = EINVAL;
			return (-1);
		}

		if (c != 0)
			bufp++;

		*ptr++ = (unsigned char) (val & 0377);
		i++;

		/* We might get a semicolon here - not required. */
		if (*bufp == ':') {
			if (i == ETH_ALEN) {
				; /* nothing */
			}
			bufp++;
		}
	}

	/* That's it.  Any trailing junk? */
	if ((i == ETH_ALEN) && (*bufp != '\0')) {
		errno = EINVAL;
		return (-1);
	}

	return (0);
}

/* Set an entry in the ARP cache. */
/* <ipaddress> <mac> */
int librouter_arp_add(char *host, char *mac)
{
	struct arpreq req;
	struct sockaddr sa;
	struct sockaddr_in *sin;
	int flags;
	int sockfd = 0;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		exit(-1);
	}

	memset((char *) &req, 0, sizeof(req));
	sin = (struct sockaddr_in *) &sa;
	sin->sin_family = AF_INET;
	sin->sin_port = 0;

	if (inet_aton(host, &sin->sin_addr) == 0) {
		return (-1);
	}

	memcpy((char *) &req.arp_pa, (char *) &sa, sizeof(struct sockaddr));

	if (_librouter_arp_in_ether(mac, &req.arp_ha) < 0) {
		printf("%% invalid hardware address\n");
		return (-1);
	}

	/* Check out any modifiers. */
	flags = ATF_PERM | ATF_COM;

	/* Fill in the remainder of the request. */
	req.arp_flags = flags;
	strcpy(req.arp_dev, "");

	/* Call the kernel. */
	if (ioctl(sockfd, SIOCSARP, &req) < 0) {
		perror("%% error");
		return (-1);
	}

	return (0);
}


/**
 * _dev_save_subiface_flags
 *
 * Save sub-interfaces flags for when this interface is re-enabled,
 * they can restore to the same state.
 *
 * @param dev
 */
static void _dev_save_subiface_flags(char *dev)
{
	struct intf_info info;
	char *intf_list[MAX_NUM_LINKS];
	int num_of_ifaces = 0;
	int i;

	librouter_ip_get_if_list(&info);
	for (i = 0; i < MAX_NUM_LINKS; i++) {
		intf_list[i] = info.link[i].ifname;
	}

	/* Get number of interfaces and sort them by name */
	for (i = 0; intf_list[i][0] != '\0'; i++)
		num_of_ifaces++;

	for (i = 0; i < num_of_ifaces; i++) {
		char filename[64];
		sprintf(filename, "/var/run/%s.linkup", intf_list[i]);

		if (strstr(intf_list[i],".") == NULL)
			continue;

		if (strncmp(dev, intf_list[i], strlen(dev)))
			continue;

		/*
		 * If sub-interface link is up, create a file
		 * so we can restore its state afterwards. If down,
		 * make sure the file is does not exist.
		 */
		if (librouter_dev_get_link(intf_list[i])) {
			FILE *f;
			f = fopen(filename, "w");
			fclose(f);
		} else {
			unlink(filename);
		}
	}
}

/**
 * _dev_restore_subiface_flags
 *
 * Restore sub-interfaces flags when re-enabling main interface
 *
 * @param dev
 */
static void _dev_restore_subiface_flags(char *dev)
{
	struct intf_info info;
	char *intf_list[MAX_NUM_LINKS];
	int num_of_ifaces = 0;
	int i;

	librouter_ip_get_if_list(&info);
	for (i = 0; i < MAX_NUM_LINKS; i++) {
		intf_list[i] = info.link[i].ifname;
	}

	/* Get number of interfaces and sort them by name */
	for (i = 0; intf_list[i][0] != '\0'; i++)
		num_of_ifaces++;

	for (i = 0; i < num_of_ifaces; i++) {
		FILE *f;
		char filename[64];

		if (strstr(intf_list[i],".") == NULL)
			continue;

		if (strncmp(dev, intf_list[i], strlen(dev)))
			continue;

		sprintf(filename, "/var/run/%s.linkup", intf_list[i]);
		f = fopen(filename, "r");
		if (f != NULL) {
			/* Is interface already up? */
			if (librouter_dev_get_link(intf_list[i]) != IFF_UP)
				librouter_dev_set_link_up(intf_list[i]);
			fclose(f);
		}

	}
}

/**
 * _dhcp_server_reload	Reload DHCP server if interface matches
 *
 * @param dev	Device to be tested
 */
static void _dhcp_server_reload(char *dev)
{
	char *dhcp_dev = NULL;

	/* Reload DHCP server if active */
	librouter_dhcp_server_get_iface(&dhcp_dev);
	if (dhcp_dev) {
		if (!strcmp(dhcp_dev, dev))
			librouter_dhcp_reload_udhcpd();
		free(dhcp_dev);
	}
}

/**
 * librouter_dev_shutdown	Disable a network interface
 *
 * @param dev	Device name (on system)
 * @param fam
 * @return 0 if success, -1 if error
 */
int librouter_dev_shutdown(char *dev, dev_family *fam)
{
	int major = librouter_device_get_major(dev, str_linux);
	int minor = librouter_device_get_minor(dev, str_linux);
	char * dhcpd_intf = NULL;

	dev_dbg("Interface %s, major %d minor %d\n", dev, major, minor);

	if (fam == NULL) {
		printf("%s -- > ERROR: device family not found for dev %s\n", __FUNCTION__, dev);
		return -1;
	}

	/* Is interface already down? */
	if (!librouter_dev_get_link(dev))
		return 0;

#ifdef OPTION_QOS
	librouter_qos_tc_remove_all(dev);
#endif

	/*Verify DHCP Server on intf and shut it down*/
	librouter_dhcp_server_get_iface(&dhcpd_intf);
	if (dhcpd_intf){
		if (!strcmp(dev, dhcpd_intf))
			librouter_dhcp_server_set_status(0);
		free (dhcpd_intf);
	}

	/* Save sub-interfaces states when physical interface goes down */
	if (minor < 0)
		_dev_save_subiface_flags(dev);

	librouter_dev_set_link_down(dev);

	switch (fam->type) {
#ifdef OPTION_EFM
	case efm:
		/* Ignore if sub-interface */
		if (minor > 0)
			break;

		librouter_efm_enable(0);
		break;
#endif
	case eth:
		break;

#ifdef OPTION_MODEM3G
	case ppp:
		break;
#endif
	case wlan:
#ifdef OPTION_HOSTAP
		if (librouter_wifi_hostapd_enable_set(0) < 0)
			syslog(LOG_WARNING, "\n%% Warning: Problems occurred shutting down WLAN - [HOSTAPD].\n\n");
#endif
		sleep(3); /* Time for wlan interface to settle */
		librouter_dev_set_link_down(dev); /* FIXME hostapd brought it up again ???*/
		break;
	default:
		break;
	}

#ifdef OPTION_VRRP
	if (librouter_vrrp_is_iface_tracked(dev))
		librouter_vrrp_reload();
#endif

	return 0;
}

/**
 * librouter_dev_noshutdown	Enable a network interface
 *
 * @param dev
 * @param fam
 * @return
 */
int librouter_dev_noshutdown(char *dev, dev_family *fam)
{
	int major = librouter_device_get_major(dev, str_linux);
	int minor = librouter_device_get_minor(dev, str_linux);
	char *dhcp_dev;

	dev_dbg("Interface %s, major %d minor %d\n", dev, major, minor);

	if (fam == NULL) {
		printf("%s -- > ERROR: device family not found for dev %s\n", __FUNCTION__, dev);
		return -1;
	}

	/* Is interface already up? */
	if (librouter_dev_get_link(dev) == IFF_UP)
		return 0;

	if (librouter_dev_set_link_up(dev) < 0)
		return -1;

	/* Restore sub-interfaces states */
	if (minor < 0)
		_dev_restore_subiface_flags(dev);

	switch (fam->type) {
	case eth:
		break;
#ifdef OPTION_EFM
	case efm:
		/* Ignore if sub-interface */
		if (minor > 0)
			break;

		librouter_efm_enable(1);
		break;
#endif
#ifdef OPTION_MODEM3G
	case ppp:
	{
		int p = librouter_usb_get_realport_by_aliasport(major);
		if (librouter_usb_device_is_modem(p) < 0) {
			printf("\n%% Warning: The interface is not connected or is not a modem\n\n");
			syslog(LOG_WARNING, "\n%% Warning: The interface is not connected or is not a modem\n\n");
		}

	}
	break;
#endif
	case wlan:
#ifdef OPTION_HOSTAP
		if (librouter_wifi_hostapd_enable_set(1) < 0)
			syslog(LOG_WARNING, "\n%% Warning: Problems occurred turning on WLAN - [HOSTAPD].\n\n");
#endif
		sleep(3); /* Wait for wireless interface to settle */
		break;
	default:
		break;
	}


	/* Reload DHCP Server if active on interface */
	_dhcp_server_reload(dev);

#ifdef OPTION_QOS
	/* Reactivate QOS rules */
	librouter_qos_tc_insert_all(dev);
#endif

#ifdef OPTION_SMCROUTE
	librouter_smc_route_hup();
#endif

#ifdef OPTION_VRRP
	if (librouter_vrrp_is_iface_tracked(dev))
		librouter_vrrp_reload();
#endif

	return 0;
}

int notify_driver_about_shutdown(char *dev)
{
#if 0
	int sock;
	struct ifreq ifr;

	if( !dev || !strlen(dev) )
		return -1;

	strcpy(ifr.ifr_name, dev);

	/* Create a channel to the NET kernel. */
	if( (sock=socket(PF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0 ) {
		librouter_pr_error(1, "if_shutdown_alert: socket");
		return (-1);
	}

	ifr.ifr_settings.type = IF_SHUTDOWN_ALERT;

	/* use linux/drivers/net/wan/hdlc_sppp.c */
	if( ioctl(sock, SIOCWANDEV, &ifr) ) {
		librouter_pr_error(1, "if_shutdown_alert: ioctl");
		return (-1);
	}

	close(sock);

	return 0;
#else
	return 0;
#endif
}

#ifdef CONFIG_DEVELOPMENT
#ifdef CONFIG_SERIAL
int librouter_dev_set_rxring(char *dev, int size)
{
	int s;
	struct ifreq ifr;

	if ((s = _librouter_dev_get_ctrlfd()) < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, dev);

	if (ioctl(s, SIOCGIFRXRING, &ifr) < 0) {
		close(s);
		perror("% SIOCGIFRXRING");
		return -1;
	}

	if (ifr.ifr_ifru.ifru_ivalue != size) {
		ifr.ifr_ifru.ifru_ivalue = size;
		if (ioctl(s, SIOCSIFRXRING, &ifr) < 0) {
			close(s);
			//perror("% SIOCSIFRXRING");
			printf("%% shutdown interface first\n");
			return -1;
		}
	}

	close(s);

	return 0;
}

int librouter_dev_get_rxring(char *dev)
{
	int s;
	struct ifreq ifr;

	if ((s=socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return 0;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, dev);

	if (ioctl(s, SIOCGIFRXRING, &ifr) < 0) {
		close(s);
		perror("% SIOCGIFRXRING");
		return -1;
	}

	close(s);

	return ifr.ifr_ifru.ifru_ivalue;
}

int librouter_dev_set_txring(char *dev, int size)
{
	int s;
	struct ifreq ifr;

	if ((s = _librouter_dev_get_ctrlfd()) < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, dev);

	if (ioctl(s, SIOCGIFTXRING, &ifr) < 0) {
		close(s);
		perror("% SIOCGIFRXRING");
		return -1;
	}

	if (ifr.ifr_ifru.ifru_ivalue != size) {
		ifr.ifr_ifru.ifru_ivalue = size;
		if (ioctl(s, SIOCSIFTXRING, &ifr) < 0) {
			close(s);
			//perror("% SIOCSIFTXRING");
			printf("%% shutdown interface first\n");
			return -1;
		}
	}

	close(s);

	return 0;
}

int librouter_dev_get_txring(char *dev)
{
	int s;
	struct ifreq ifr;

	if ((s=socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return 0;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, dev);

	if (ioctl(s, SIOCGIFTXRING, &ifr) < 0) {
		close(s);
		perror("% SIOCGIFTXRING");
		return -1;
	}

	close(s);

	return ifr.ifr_ifru.ifru_ivalue;
}

int librouter_dev_set_weight(char *dev, int size)
{
	int s;
	struct ifreq ifr;

	if ((s = _librouter_dev_get_ctrlfd()) < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, dev);
	ifr.ifr_ifru.ifru_ivalue = size;

	if (ioctl(s, SIOCSIFWEIGHT, &ifr) < 0) {
		close(s);
		perror("% SIOCSIFWEIGHT");
		return -1;
	}

	close(s);

	return 0;
}

int librouter_dev_get_weight(char *dev)
{
	int s;
	struct ifreq ifr;

	if ((s=socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return 0;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, dev);

	if (ioctl(s, SIOCGIFWEIGHT, &ifr) < 0) {
		close(s);
		perror("% SIOCGIFWEIGHT");
		return -1;
	}

	close(s);

	return ifr.ifr_ifru.ifru_ivalue;
}
#endif
#endif /* CONFIG_DEVELOPMENT */

