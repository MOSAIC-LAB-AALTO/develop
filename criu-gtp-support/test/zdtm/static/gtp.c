
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <linux/limits.h>
#include <signal.h>
#include <arpa/inet.h>
#include <net/if.h>
#include "zdtmtst.h"

const char *test_doc	= "check gtp devices";
const char *test_author	= "Ibrahim Afolabi <ibrahim1.afolabi@gmail.com";

#define IF_NAME "gtp0"
#define MS_ADDR	"10.10.10.2"
#define SGSN_ADDR	"172.16.10.22"
#define I_TEI   "1234"
#define O_TEI   "5678"

int main(int argc, char **argv)
{
	int ret = 1;

	test_init(argc, argv);

	if (system("gtp-link add "IF_NAME" v1 "I_TEI " "O_TEI " "MS_ADDR " "SGSN_ADDR " &")) {
        	printf("Can't make gtp link device\n");
        	return ret;
    	}	

       	if (system("gtp-tunnel add "IF_NAME" v1 "I_TEI " "O_TEI " "MS_ADDR " "SGSN_ADDR " &")) {
        	printf("Can't make gtp tunnel device\n");
        	return ret;
    	}	
	
	//if (system ("ip addr add 10.10.10.1/24 dev gtp0")){
	//	printf("Can't add address to the GTP device\n");
		//return ret;
	//}

        sleep(10);

        if (system("ip -details addr list dev "IF_NAME" > gtp.dump.test")) {
		fail("Can't get net config dumping phase");
                goto out;
        }


	test_daemon();
	test_waitsig();

	if (system("ip -details addr list dev "IF_NAME" > gtp.rst.test")) {
		fail("Can't get net config restoring phase");
		goto out;
	}

	if (system("diff gtp.rst.test gtp.dump.test")) {
		fail("Net config differs after restore");
		goto out;
	}
		

	pass();
	ret = 0;

out:
	return ret;
}


/***
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <net/if.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <libmnl/libmnl.h>
#include <libgtpnl/gtp.h>
#include <libgtpnl/gtpnl.h>
#include <errno.h>
#include <time.h>
#include "zdtmtst.h"

#define GTPV1U_UDP_PORT 2152
//#define gtp_dev_mtu		1440
#define OK        0
#define ERROR     -1
#define GTP_DEVNAME "gtp0"

static struct {
  int                 genl_id;
  struct mnl_socket  *nl;
  bool                is_enabled;
} gtp_nl;


//------------------------------------------------------------------------------
int gtp_mod_kernel_init(int *fd0, int *fd1u, struct in_addr *ue_net, int mask, int gtp_dev_mtu)
{
  // we don't need GTP v0, but interface with kernel requires 2 file descriptors
  *fd0 = socket(AF_INET, SOCK_DGRAM, 0);
  *fd1u = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in sockaddr_fd0 = {
      .sin_family = AF_INET,
      .sin_port = htons(3386),
      .sin_addr = {
          .s_addr   = INADDR_ANY,
      },
  };
  struct sockaddr_in sockaddr_fd1 = {
      .sin_family = AF_INET,
      .sin_port = htons(GTPV1U_UDP_PORT),
      .sin_addr = {
          .s_addr   = INADDR_ANY,
      },
  };

  if (bind(*fd0, (struct sockaddr *) &sockaddr_fd0,
      sizeof(sockaddr_fd0)) < 0) {
    printf("bind GTPv0 port\n");
    return ERROR;
  }
  if (bind(*fd1u, (struct sockaddr *) &sockaddr_fd1,
      sizeof(sockaddr_fd1)) < 0) {
    printf("bind s1U port\n");
    return ERROR;
  }

  if (gtp_dev_create(-1, GTP_DEVNAME, *fd0, *fd1u) < 0) {
    printf("Cannot create GTP tunnel device: %s\n", strerror(errno));
    return ERROR;
  }
  gtp_nl.is_enabled = true;

  gtp_nl.nl = genl_socket_open();
  if (gtp_nl.nl == NULL) {
    printf("Cannot create genetlink socket\n");
    return ERROR;
  }
  gtp_nl.genl_id = genl_lookup_family(gtp_nl.nl, "gtp");
  if (gtp_nl.genl_id < 0) {
    printf("Cannot lookup GTP genetlink ID\n");
    return ERROR;
  }
  printf("Using the GTP kernel mode (genl ID is %d)\n", gtp_nl.genl_id);
  
  
  int ret = system("ip link set dev " GTP_DEVNAME " mtu 1440");
  printf("GTP device setup successfully!\n");
  if (ret) {
    printf("ERROR in system command 'ip link set dev ...': ret = %d at line 97\n", ret);
    return ERROR;
  }

  //struct in_addr ue_gw;
  //ue_gw.s_addr = ue_net->s_addr | htonl(1);
  ret = system ("ip addr add 10.10.10.1/24 dev " GTP_DEVNAME);
  	printf("GTP ip address added successfully!\n");
  if (ret) {
    printf("ERROR in system command 'ip link set dev ...': ret = %d at line 110\n", ret);
    return ERROR;
  }else{
	printf("GTP ip address added successfully!\n");
  }

  printf("Setting route to reach UE network %s via %s\n", inet_ntoa(*ue_net), GTP_DEVNAME);
  if (gtp_dev_config(GTP_DEVNAME, ue_net, mask) < 0) {
    printf("Cannot add route to reach network\n");
    return ERROR;
  }
  printf("GTP kernel configured\n");

  return OK;
}


//------------------------------------------------------------------------------
int gtp_mod_kernel_tunnel_add(struct in_addr ue, struct in_addr enb, uint32_t i_tei, uint32_t o_tei)
{
  struct gtp_tunnel *t;
  int ret;

  if (!gtp_nl.is_enabled)
    return OK;

  t = gtp_tunnel_alloc();
  if (t == NULL)
    return ERROR;


  gtp_tunnel_set_ifidx(t, if_nametoindex(GTP_DEVNAME));
  gtp_tunnel_set_version(t, 1);
  gtp_tunnel_set_ms_ip4(t, &ue);
  gtp_tunnel_set_sgsn_ip4(t, &enb);
  gtp_tunnel_set_i_tei(t, i_tei);
  gtp_tunnel_set_o_tei(t, o_tei);

  ret = gtp_add_tunnel(gtp_nl.genl_id, gtp_nl.nl, t);
  gtp_tunnel_free(t);

  return ret;
}


//------------------------------------------------------------------------------
int main(int argc,  char **argv)
{
	
	int fdo,
	    fdiu,
	    mask = 24,
	    mtu  = 1440,
	    itei = 5577,
	    otei = 4422;
	struct in_addr  ue_addr, eNB_addr, ue_net;

	int ret = 1;

	test_init(argc, argv);
	
	inet_pton(AF_INET, "10.10.10.0",  &ue_net);
	inet_pton(AF_INET, "10.10.10.2",  &ue_addr);
	inet_pton(AF_INET, "20.20.20.2",  &eNB_addr);
	//eNB_addr = (struct in_addr )inet_network("20.20.20.2");	    
	
	ret = gtp_mod_kernel_init(&fdo, &fdiu, &ue_net, mask, mtu);
	printf("GTP KERNEL MODULE INITIALIZED SUCCESSFULLY! ret = %d\n", ret);
	
	ret = gtp_mod_kernel_tunnel_add(ue_addr, eNB_addr, itei, otei);
    	printf("GTP TUNNEL ADDED SUCCESSFULLY! ret = %d\n", ret);
    

	sleep(10);

        if (system("ip -details addr list dev "GTP_DEVNAME" > gtp.dump.test")) {
		fail("Can't get net config dumping phase");
                goto out;
        }


	test_daemon();
	test_waitsig();

	if (system("ip -details addr list dev "GTP_DEVNAME" > gtp.rst.test")) {
		fail("Can't get net config restoring phase");
		goto out;
	}

	if (system("diff gtp.rst.test gtp.dump.test")) {
		fail("Net config differs after restore");
		goto out;
	}
		

	pass();
	ret = 0;

out:
	return ret;

}

***/




