#include <stdio.h>
#include <signal.h>		//same to up
#include <netpacket/packet.h>
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <nmp_mcuioservice.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <linux/if.h>


static FILE* wifi_fp = NULL;
static char wifi_states[32];
static char usb_path[32];
static bool b_force_test = false;
static bool b_enable_voice = false;
#define MAC_LEN (6)
static unsigned char eth_mac[MAC_LEN];
static unsigned char wifi_mac[MAC_LEN];
#define FACTORY_SSID "ALI_Factory_Test"
#define FACTORY_PWD "12345678"
#define KEY_USB "/mnt/usb"
#define FLAG_LOCAL_PATH "/usr/mnt_app/var"
#define USB_TEST_FILE "factory_usb_test"
#define TEST_FLAG "ALI_Factory_Test"


#define DEBUG 1

#define DBG_PRT(fmt, args...)   \
        printf("factory test "fmt"\n", ##args);    \
        printf("-------------------------------------------------->\n")



void prompt(char *str) {
	char cmd[128];

	DBG_PRT("prompt:%s\r\n", str);

	strcpy(cmd, "/usr/mnt_app/var/cache/forked-daapd/prompt.sh ");
	strcat(cmd, str);

	sleep(1);
	if (b_enable_voice) {
		system(cmd);
	}
	sleep(1);
}

int get_mac(const char *dev, char *buf)
{
	int sock, i, ret = 0;
	struct sockaddr_in *sin;	
	struct ifreq ifr_ip;	   

	if ((dev == NULL) || (buf == NULL)) {
		return -1;
	}

	if ((sock=socket(AF_INET, SOCK_STREAM, 0)) == -1) {  
		 return -1;  
	}  

	memset(&ifr_ip, 0, sizeof(ifr_ip)); 	
	strncpy(ifr_ip.ifr_name, dev, sizeof(ifr_ip.ifr_name) - 1);	   
	if(ioctl(sock, SIOCGIFHWADDR, &ifr_ip) >= 0) {	  
		memcpy(buf, ifr_ip.ifr_hwaddr.sa_data, MAC_LEN);
		/*
		for(i = 0; i < MAC_LEN; i++)  
			printf("%s ==> %2x:", dev, buf[i]); */
	}

	close(sock);
	return ret;
}


void factory_test_prepare()
{
	system("kill -9 $(pidof iw_gpioinit.sh)");
	system("kill -9 $(pidof volume_init)");
	system("kill -9 $(pidof forked-daapd)");
	system("kill -9 $(pidof mcuioservice)");
}

int
wifi_shell_connect(char *ssid, char *pwd)
{
	char cmd[128];

	if (check_ethernet_ready() == 1) {
		DBG_PRT("shutdown ethernet ...\r\n");
		system("/usr/mnt_app/var/cache/forked-daapd/network_switch.sh wifi");
		sleep(2);

		while(get_netlink_status() == 1) {
			usleep(1000);
		}
	}

	system("/usr/mnt_app/var/cache/forked-daapd/iw_clean.sh ");
	strcpy(cmd, "/usr/mnt_app/var/cache/forked-daapd/iw_connect.sh hint ");
	if (ssid!=NULL) {
		strcat(cmd, ssid);
	}
	if (pwd!=NULL) {
		strcat(cmd, " ");
		strcat(cmd, pwd);
	}
	wifi_fp = popen(cmd, "r");
	if(wifi_fp == NULL) {
		return -1;
	}
	strcpy(wifi_states, "");
	//system("touch /usr/mnt_app/var/cache/forked-daapd/lib/wifi.wificonfig");

	return 0;
}

int 
wifi_shell_cmdstat(void)
{
  char buf[128];
  int pwd_wrong=0, ssid_wrong=0, success=0; 
  int ret;

  if (wifi_fp == NULL) {
	return -1;
  }

  /* output the message */
  while (fgets(buf, sizeof(buf), wifi_fp) != NULL) {
	  DBG_PRT("wificonf status: %s", buf);
	  if (strstr(buf, "SCANNING")) {
	  	ssid_wrong++;
	  } else if (strstr(buf, "COMPLETED")) {
		success++;
		break;
	  } else if (strstr(buf, "4WAY_HANDSHAKE")) {
	  	pwd_wrong++;
	  } else {
	  }
  }

  if (success > 0) {
	  strcpy(wifi_states, "wifi connected");
	  system("/usr/mnt_app/var/cache/forked-daapd/prompt.sh wifi_connected");
	  get_mac("ra0", wifi_mac);
	  ret = 0;
  } else if (pwd_wrong >= 3) {
	  strcpy(wifi_states, "wrong password");
	  system("/usr/mnt_app/var/cache/forked-daapd/prompt.sh wrong_password");
	  ret = -1;
  } else if (ssid_wrong >= 3) {
	  strcpy(wifi_states, "no such ssid");
	  system("/usr/mnt_app/var/cache/forked-daapd/prompt.sh connect_fail");
	  ret = -1;
  } else {
	  strcpy(wifi_states, "unknown failure");
	  system("/usr/mnt_app/var/cache/forked-daapd/prompt.sh wrong_password");
	  ret = -1;
  }
  DBG_PRT("final wificonf status: %s", wifi_states);
   
  pclose(wifi_fp);
  wifi_fp = NULL;
 
  return ret;
}

int wifi_test(void) {
	int ret = -1;
	bool save = (access("/usr/mnt_app/var/wifi_wpa/wpa_supplicant.conf", F_OK) == 0);

	//start wifi test
	prompt("start_wifi_test");

	if (save)
		system("cp /usr/mnt_app/var/wifi_wpa/wpa_supplicant.conf /tmp/wpa_supplicant.conf");
	system("cp /usr/mnt_app/var/cache/forked-daapd/reset/wpa_supplicant.conf /usr/mnt_app/var/wifi_wpa/wpa_supplicant.conf");

	ret = wifi_shell_connect(FACTORY_SSID, FACTORY_PWD);
	if (ret != 0) {
		goto RETURN;
	}

	ret = wifi_shell_cmdstat();
	if (ret != 0) {
		goto RETURN;
	}

	prompt("wifi_test_pass");

	ret = 0;

RETURN:
	remove("/usr/mnt_app/var/wifi_wpa/wpa_supplicant.conf");
	if (save)
		system("cp /tmp/wpa_supplicant.conf /usr/mnt_app/var/wifi_wpa/wpa_supplicant.conf");
	while(ret != 0) {
		prompt("wifi_test_fail");
		sleep(1);
	}

	return ret;
}

PLM_MODE mmp_get_plm()
{
    PLM_MODE plm;
    plm = mcuio_get_plmmode();
    return plm;
}

int mmp_boot()
{
	int ret;
    int mmp_vol;
    int mmp_plm;
    int mmp_mcuversion;

	ret =mcuio_boot();
	if(ret < 0) {
		return -1;
	}

    mmp_vol = mcuio_get_vol();
	if(mmp_vol < 0) {
		return -1;
	}

    mmp_plm = mmp_get_plm();
	if(mmp_plm < 0) {
		return -1;
	}

    mmp_mcuversion = mcuio_get_mcuversion();
	if(mmp_mcuversion < 0) {
		return -1;
	}

    return 0;
}


int mmp_get_mcuversion()
{
    int ret = 0;
    ret = mcuio_get_mcuversion();
    return ret;
}

static int _mmp_service_start()
{
    char cmd[128];

    DBG_PRT("Start mmp by cmd\n");
    sprintf(cmd, "mcuioservice &");
    system(cmd);
    return 0;
}

int mmp_open()
{
    int ret;
   
    _mmp_service_start();
    
    //mcuioServiceCallbackFuncReg(&cb_mmpservice);
    ret =mcuioServiceOpen();
	if(ret < 0) {
		return -1;
	}

    //mmp_info_init();

    ret =mmp_boot();
	if(ret < 0) {
		return -1;
	}

    return 0;
}


int uart_test(void) {
	prompt("start_uart_test");
	if (mmp_open() < 0) {
		goto UART_TEST_FAILTURE;
	}

	prompt("uart_test_pass");
	return 0;

UART_TEST_FAILTURE:
	while(1) {
		prompt("uart_test_fail");
		sleep(1);
	}

	return -1;
}	

int check_mount(void) {
	char cmd[128];
	char buf[1024];
	char *ptr; 
	FILE* fp = NULL;

	strcpy(cmd, "mount");
	fp = popen(cmd, "r");
	if(fp == NULL) {
		goto CHECK_MOUNT_FAILURE;
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		//DBG_PRT("mount %s", buf);
		if (strstr(buf, KEY_USB) != NULL) {
			ptr = strtok(buf, " ");
			while(ptr != NULL) {
				//DBG_PRT("ptr=%s\n", ptr);  
				if (strstr(ptr, KEY_USB) != NULL) {
					DBG_PRT(">>>>>>>>>> KEY_USB %s", ptr);
					strcpy(usb_path, ptr);
					goto CHECK_MOUNT_SUCCESS;
				}
				ptr = strtok(NULL, " "); 
			}
		}
	}

CHECK_MOUNT_FAILURE:
	return -1;

CHECK_MOUNT_SUCCESS:
	return 0;
}

char* get_mount(void) {
	return usb_path;
}

int usb_test(void) {
	FILE *fp=NULL;
	char ch;
	char path[128];
	
	//start ethernet test
	prompt("start_usb_test");

	while(check_mount() != 0) {
		prompt("pleas_insert_udisk");
		sleep(2);
	}

	sleep(2);
	strcpy(path, get_mount());
	strcat(path, "/");
	strcat(path, USB_TEST_FILE);

	while((fp = fopen(path,"w+")) == NULL) {
		prompt("writing_udisk");
		sleep(1);
	}

	fputs("factory test",fp); 
	fclose(fp);
	remove(fp);

	prompt("usb_test_pass");

    return 0;
}


// if_name like "ath0", "eth0". Notice: call this function
// need root privilege.
// return value:
// -1 -- error , details can check errno
// 1 -- interface link up
// 0 -- interface link down.
int get_netlink_status(const char *if_name)
{
    int skfd,ret;
    struct ifreq ifr;
    struct ethtool_value edata;
    edata.cmd = ETHTOOL_GLINK;
    edata.data = 0;

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name) - 1);
    ifr.ifr_data = (char *) &edata;
    if (( skfd = socket( AF_INET, SOCK_DGRAM, 0 )) < 0)
    {
     	printf("volume_init get_netlink_status error to do socket****\n");
        return -1;
    }

    if(ioctl( skfd, SIOCETHTOOL, &ifr ) < 0)
    {
     	ret = printf("volume_init get_netlink_status error to do socket ioctl SIOCETHTOOL****\n");
        close(skfd);
        return ret;
    }
    close(skfd);
    return edata.data;
}

int check_ethernet_ready(void)
{
	int sock_get_ip, i, ret = 0;  
	char ipaddr[50], dev[16]={"eth0"};  
	struct sockaddr_in *sin;	
	struct ifreq ifr_ip;	   

	if ((sock_get_ip=socket(AF_INET, SOCK_STREAM, 0)) == -1) {  
		 //printf("socket create failse...GetLocalIp!\n");  
		 return -1;  
	}  

	memset(&ifr_ip, 0, sizeof(ifr_ip)); 	
	strncpy(ifr_ip.ifr_name, dev, sizeof(ifr_ip.ifr_name) - 1);	   
	if( ioctl( sock_get_ip, SIOCGIFADDR, &ifr_ip) >= 0 ) {	  
		sin = (struct sockaddr_in *)&ifr_ip.ifr_addr;	  
		strcpy(ipaddr,inet_ntoa(sin->sin_addr));		 
		  
		//printf("local ip for %s: %s \n",dev, ipaddr);	  
		ret = 1;
	}		

	get_mac(dev, eth_mac);

	close(sock_get_ip);

	return ret;
}

int eth_shell_connect(void) {
	char cmd[128];

	system("kill -9 $(pidof wpa_supplicant)");
	system("/usr/mnt_app/var/cache/forked-daapd/network_switch.sh eth &");
	sleep(2);
	if (get_netlink_status <= 0) {
		return -1;
	}

	return 0;
}


int ethernet_test(void) {
	int ret = -1;

	//start wifi test
	prompt("start_ethernet_test");

	ret = eth_shell_connect();
	if (ret != 0) {
		goto ETH_TEST_FAIL;
	}

	while(check_ethernet_ready() == 0) {
		prompt("please_connect_ethernet");
		sleep(1);
	}

	prompt("ethernet_test_pass");

	return 0;

ETH_TEST_FAIL:
	while(1) {
		prompt("ethernet_configuration_fail");
		sleep(1);
	}

	return -1;

}

void set_factory_flag(void) {
	FILE *fp=NULL;
	char path[128];

	strcpy(path, FLAG_LOCAL_PATH);
	strcat(path, "/");
	strcat(path, TEST_FLAG);

	fp = fopen(path,"w+");
	fputs("factory test pass",fp); 
	fclose(fp);
	
	while(1) {
		prompt("factory_test_pass");
		printf("Ethernet MAC => %02x:%02x:%02x:%02x:%02x:%02x \r\n",\
			eth_mac[0], eth_mac[1], eth_mac[2], eth_mac[3], eth_mac[4], eth_mac[5]);
		printf("Wifi MAC => %02x:%02x:%02x:%02x:%02x:%02x \r\n",\
			wifi_mac[0], wifi_mac[1], wifi_mac[2], wifi_mac[3], wifi_mac[4], wifi_mac[5]);
		sleep(3);
	}

}

void Factory_Test() {
	int ret = 0;

	sleep(1);
	prompt("start_factory_test");

	//prepare
	factory_test_prepare();

	//start wifi test
	ret = wifi_test();
	if (ret != 0) {
		goto TEST_FAILURE;
	}

	//start uart test
	ret = uart_test();
	if (ret != 0) {
		goto TEST_FAILURE;
	}

	//start usb test
	check_mount();
	ret = usb_test();
	if (ret != 0) {
		goto TEST_FAILURE;
	}

	//start ethernet test
	ret = ethernet_test();
	if (ret != 0) {
		goto TEST_FAILURE;
	}

TEST_SUCCESS:
	//set factory test flag
	set_factory_flag();

	//reboot
	//reboot();

	return;

TEST_FAILURE:
	while(1) {
		prompt("factory_test_fail");
		sleep(1);
	}
	return;
}

bool check_local_flag(void) {
	char path[128];

	strcpy(path, FLAG_LOCAL_PATH);
	strcat(path, "/");
	strcat(path, TEST_FLAG);

	if (access(path,0) != 0) {
		return true;
	}

	return false;
}

bool check_param(void) {
	return b_force_test;
}

bool check_usb_flag(void) {
	char path[128];

	if (check_mount() != 0)
		return false;

	strcpy(path, get_mount());
	strcat(path, "/");
	strcat(path, TEST_FLAG);

	if (access(path,0) == 0) {
		return true;
	}

	return false;
}

int main(int argc, char **argv)
{
	for(int i=0; i<argc; i++) {
		if (strstr(argv[i], "-F")) {
			b_force_test = true;
			printf("force factory test\r\n");
		}
		if (strstr(argv[i], "-V")) {
			b_enable_voice = true;
			printf("enable voice\r\n");
		}
	}

	if (check_local_flag() || check_param() || check_usb_flag()) {
		Factory_Test();
	}else {
		DBG_PRT("skip factory test\r\n");
	}

	return 0;
}

