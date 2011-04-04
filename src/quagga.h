/*
 * quagga.h
 *
 *  Created on: Jun 16, 2010
 *      Author: Thomás Alimena Del Grande (tgrande@pd3.com.br)
 */

#ifndef QUAGGA_H_
#define QUAGGA_H_

/* UNIX domain socket path. */
#define ZEBRA_PID "/var/quagga/zebra.pid"
#define ZEBRA_PATH "/var/quagga/zebra.vty"
#define RIP_PATH "/var/quagga/ripd.vty"
#define OSPF_PATH "/var/quagga/ospfd.vty"
#define BGP_PATH "/var/quagga/bgpd.vty"

#define ZEBRA_CONF "/etc/quagga/zebra.conf"
#define RIPD_CONF "/etc/quagga/ripd.conf"
#define OSPFD_CONF "/etc/quagga/ospfd.conf"
#define BGPD_CONF "/etc/quagga/bgpd.conf"

#define RIPD_RO_CONF "/etc.ro/quagga/ripd.conf"
#define OSPFD_RO_CONF "/etc.ro/quagga/ospfd.conf"
#define BGPD_RO_CONF "/etc.ro/quagga/bgpd.conf"

#define QUAGGA_OWNER_VALUE 	500
#define QUAGGA_GROUP_VALUE	500

int librouter_quagga_connect_daemon(char *);
int librouter_quagga_execute_client(char *, FILE *, char *, int);
void librouter_quagga_close_daemon(void);
FILE *librouter_quagga_zebra_show_cmd(const char *);
FILE *librouter_quagga_ospf_show_cmd(const char *);
FILE *librouter_quagga_rip_show_cmd(const char *);
FILE *librouter_quagga_bgp_show_cmd(const char *);
int librouter_quagga_cidr_to_classic(char *, char *);
int librouter_quagga_classic_to_cidr(char *, char *, char *);
int librouter_quagga_netmask_to_cidr(char *);
int librouter_quagga_cidr_to_netmask(unsigned int, char *);
char* librouter_quagga_uint_to_octets(unsigned int, char*);
int librouter_quagga_octets_to_uint(unsigned int *, char *);
unsigned int librouter_quagga_bitfill_from_left(unsigned int);
int librouter_quagga_bitcount(unsigned int);
int librouter_quagga_validate_mask(unsigned int);
int librouter_quagga_validate_ip(char *ip);
int add_route_dev(char *target, char *netmask, char *device);
int librouter_quagga_ripd_is_running(void);
int librouter_quagga_ripd_exec(int on_noff);
int librouter_quagga_ospfd_is_running(void);
int librouter_quagga_ospfd_exec(int on_noff);
int librouter_quagga_bgpd_is_running(void);
int librouter_quagga_bgpd_exec(int on_noff);
int librouter_quagga_zebra_hup(void);
int librouter_quagga_floating_route(void);
int is_network_up(void);

FILE * librouter_quagga_get_conf(char *filename, char *key);
FILE * librouter_quagga_bgp_get_conf(int main_nip);
int librouter_quagga_bgp_get_asn(void);
FILE *librouter_quagga_zebra_get_conf(int main_ninterf, char *intf);
void librouter_quagga_zebra_dump_static_routes(FILE *out);
FILE *librouter_quagga_rip_get_conf(int main_ninterf, char *intf);
FILE *librouter_quagga_ospf_get_conf(int main_ninterf, char *intf);

/* Higher level of route abstration */
struct routes_t {
	char *network;
	char *mask;
	char *interface;
	char *gateway;
	int metric;
	char *hash;
	struct routes_t *next;
};

#define CGI_TMP_FILE 	"/tmp/cgi_tmp"

int librouter_quagga_add_route(struct routes_t *route);
void librouter_quagga_free_routes(struct routes_t *route);
int librouter_quagga_del_route(char *hash);
struct routes_t * librouter_quagga_get_routes(void);

#endif /* QUAGGA_H_ */
