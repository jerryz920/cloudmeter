/******************************************************************************

 @File Name : monitor/acct.c

 @Creation Date : 05-06-2013

 @Created By: Zhai Yan

 @Purpose : This is the module for network information accounting. For accurate
 sniffering all the traffic went through, and with relative high performance, we
 use netfilter.

*******************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netfilter.h>
#include <linux/proc_fs.h>
#include <linux/net.h>
#include <linux/types.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/if_addr.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/errno.h>
#include <linux/udp.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <net/ip.h>
#include <net/net_namespace.h>

MODULE_LICENSE("BSD");

#define ETHER_HEADER_LEN 14

static struct net* acct_net = &init_net;
static __be32 self_addr;

static int fetch_addr(const char* name, struct in_addr* addr)
{
  struct in_device* in_dev;
  struct in_ifaddr** ifap = NULL;
  struct in_ifaddr* ifa = NULL;
  struct net_device* dev;
  char ifname[IFNAMSIZ];
  int ret = 0;

  strncpy(ifname, name, IFNAMSIZ);
  ifname[IFNAMSIZ - 1] = 0;

  dev_load(acct_net, ifname);
  rtnl_lock();

  ret = -ENODEV;

  dev = __dev_get_by_name(acct_net, ifname);
  if (!dev)
    goto done;

  in_dev = __in_dev_get_rtnl(dev);
  if (in_dev) {
    for (ifap = &in_dev->ifa_list; (ifa = *ifap) != NULL;
        ifap = &ifa->ifa_next) {
      if (strcmp(ifname, ifa->ifa_label) == 0) {
        break;
      }
    }
  } 
  ret = -EADDRNOTAVAIL;
  if (!ifa)
    goto done;

  ret = 0;
  addr->s_addr = ifa->ifa_local;

done:
  rtnl_unlock();
  return ret;
}


#if 0
/*
 *  Debug usage
 */
static void print_netif_addresses(void)
{
  struct in_addr addr;
  if (fetch_addr("eth0", &addr) < 0) {
    printk(KERN_INFO "can not fetch address\n");
  } else {
    printk(KERN_INFO "network address %u\n", addr.s_addr);
  }
}
#endif

static __u64 s_target_flow = 0;
static __u64 s_target_flow_bytes = 0;
static __u64 s_inner_ec2 = 0;
static __u64 s_inner_ec2_bytes = 0;
static __u64 s_internet = 0;
static __u64 s_internet_bytes = 0;
static __u64 s_grand_total = 0;
static __u64 s_grand_total_bytes = 0;
static __u64 s_http = 0;
static __u64 s_http_bytes = 0;
static __u64 s_ssh = 0;
static __u64 s_ssh_bytes = 0;
static __u64 s_meta = 0;
static __u64 s_meta_bytes = 0;


static __u64 r_target_flow = 0;
static __u64 r_target_flow_bytes = 0;
static __u64 r_inner_ec2 = 0;
static __u64 r_inner_ec2_bytes = 0;
static __u64 r_internet = 0;
static __u64 r_internet_bytes = 0;
static __u64 r_grand_total = 0;
static __u64 r_grand_total_bytes = 0;
static __u64 r_http = 0;
static __u64 r_http_bytes = 0;
static __u64 r_ssh = 0;
static __u64 r_ssh_bytes = 0;
static __u64 r_meta = 0;
static __u64 r_meta_bytes = 0;

static int ifaddr_is_private(__be32 addr)
{
#define CHECK_SUBNET(addr,subnet) (((addr)&(subnet)) == (subnet))
  if (CHECK_SUBNET(addr, 0xa) || CHECK_SUBNET(addr,0xa8c0) ||
      CHECK_SUBNET(addr, 0x10ac))
    return 1;
#undef CHECK_SUBNET
  return 0;
}

static int ifaddr_is_self(__be32 addr)
{
  return addr == self_addr;
}

static int ifaddr_is_meta(__be32 addr)
{
/*
	169.254.169.254
 */
  return addr == (0xfea9fea9);
}

static void update_stat_recever_pre(const struct sk_buff *skb)
{
  struct iphdr* iph = ip_hdr(skb);
/*
    transport layer header is not yet set!
 */
  struct tcphdr* tcph = (struct tcphdr*) (((char*) iph) + 4 * iph->ihl);
  struct udphdr* udph = (struct udphdr*) tcph;

  uint64_t total_size = skb->len + ETHER_HEADER_LEN;

  r_grand_total++;
  r_grand_total_bytes += total_size;

  /*
   *  For traffic through port 1985, we mark it specially
   */
  if (iph->protocol == IPPROTO_UDP && 
      (udph->dest == htons(1985) || udph->source == htons(1985))) {
    r_target_flow++;
    r_target_flow_bytes += total_size;
  } else if (iph->protocol == IPPROTO_TCP && tcph->source == htons(80)) { 
    if (ifaddr_is_meta(iph->saddr)) {
      r_meta++;
      r_meta_bytes += total_size;
    } else {
      r_http++;
      r_http_bytes += total_size;
    }
  } else if (iph->protocol == IPPROTO_TCP &&
      tcph->dest == htons(22)) {
    r_ssh++;
    r_ssh_bytes += total_size;
  }  else if (ifaddr_is_private(iph->saddr)) {
    r_inner_ec2++;
    r_inner_ec2_bytes += total_size;
  } else {
    r_internet++;
    r_internet_bytes += total_size;
  }
}

//static void update_stat_recever_local(const struct sk_buff *skb)
//{
//}

static void update_stat_sender(const struct sk_buff *skb)
{
  struct iphdr* iph = ip_hdr(skb);
  struct udphdr* udph = udp_hdr(skb);
  struct tcphdr* tcph = tcp_hdr(skb);
  //struct dst_entry* dst = skb_dst(skb);
  uint64_t total_size = skb->len + ETHER_HEADER_LEN;

  //if (dst) {
  //      printk(KERN_INFO"reserve %d %d\n", LL_RESERVED_SPACE(dst->dev), skb_headroom(skb));
  //}
  //printk(KERN_INFO" send mac layer: %p %d %d %d\n", skb, skb->len, ntohs(iph->tot_len), skb->truesize);
  s_grand_total++;
  s_grand_total_bytes += total_size;
  /*
   *  For traffic through port 1985, we mark it specially
   */
  if (ifaddr_is_self(iph->saddr)) { 
    if (iph->protocol == IPPROTO_UDP && 
	(udph->dest == htons(1985) || udph->source == htons(1985))) {
      s_target_flow++;
      s_target_flow_bytes += total_size;
    } else if (iph->protocol == IPPROTO_TCP && tcph->dest == htons(80)) { 
      if (ifaddr_is_meta(iph->daddr)) {
	s_meta++;
	s_meta_bytes += total_size;
      } else {
	s_http++;
	s_http_bytes += total_size;
      }
    } else if (iph->protocol == IPPROTO_TCP &&
	tcph->source == htons(22)) {
      s_ssh++;
      s_ssh_bytes += total_size;
    }  else if (ifaddr_is_private(iph->daddr)) {
      s_inner_ec2++;
      s_inner_ec2_bytes += total_size;
    } else {
      s_internet++;
      s_internet_bytes += total_size;
    }
  }
}


static unsigned int acct_recever_hook(unsigned int hooknum, struct sk_buff *skb,
    const struct net_device *in,
    const struct net_device *out,
    int (*okfn) (struct sk_buff *))
{
  update_stat_recever_pre(skb);
  return NF_ACCEPT;
}

//static unsigned int acct_recever_local_hook(unsigned int hooknum, struct sk_buff *skb,
//    const struct net_device *in,
//    const struct net_device *out,
//    int (*okfn) (struct sk_buff *))
//{
//  update_stat_recever_local(skb);
//  return NF_ACCEPT;
//}


static unsigned int acct_sender_hook(unsigned int hooknum, struct sk_buff *skb,
    const struct net_device *in,
    const struct net_device *out,
    int (*okfn) (struct sk_buff *))
{
  update_stat_sender(skb);
  return NF_ACCEPT;
}


static struct nf_hook_ops ec2_recever_ops __read_mostly = {
  .hook = acct_recever_hook,
  .owner = THIS_MODULE,
  .pf = NFPROTO_IPV4,
  .hooknum = NF_INET_PRE_ROUTING,
  .priority = INT_MIN, /* we want to run as early as possible */
};

static struct nf_hook_ops ec2_sender_ops  __read_mostly = {
  .hook = acct_sender_hook,
  .owner = THIS_MODULE,
  .pf = NFPROTO_IPV4,
  .hooknum = NF_INET_POST_ROUTING,
  .priority = INT_MIN, /* we want to run as early as possible */
};

//static struct nf_hook_ops ec2_recever_local_ops __read_mostly = {
//  .hook = acct_recever_local_hook,
//  .owner = THIS_MODULE,
//  .pf = NFPROTO_IPV4,
//  .hooknum = NF_INET_LOCAL_IN,
//  .priority = INT_MIN, /* we want to run as early as possible */
//};

/*
 *  We use procfs to expose these statistics
 */
static const char* procfs_entry = "ec2_netstat";
static struct proc_dir_entry* net_acct = NULL;

static int read_ec2stat(char *page, char **start,
    off_t off, int count, int *eof, void *data)
{
  int len = sprintf(page, "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu \n",
      s_grand_total, s_grand_total_bytes, s_inner_ec2, s_inner_ec2_bytes, s_internet, s_internet_bytes, s_target_flow, s_target_flow_bytes, s_http, s_http_bytes, s_ssh, s_ssh_bytes, s_meta, s_meta_bytes, r_grand_total, r_grand_total_bytes, r_inner_ec2, r_inner_ec2_bytes, r_internet, r_internet_bytes, r_target_flow, r_target_flow_bytes, r_http, r_http_bytes, r_ssh, r_ssh_bytes, r_meta, r_meta_bytes);

  if (len <= off + count) *eof = 1;
  *start = page + off;

  len -= off;
  if (len > count) len = count;
  if (len < 0) len = 0;
  return len;
}

static int init_ec2stat(void)
{
  net_acct = create_proc_entry(procfs_entry, 0644, NULL);
  if (!net_acct) {
    remove_proc_entry("ec2_netstat", NULL);
    return -ENOMEM;
  }

  net_acct->read_proc = read_ec2stat;
  net_acct->mode = S_IFREG | S_IRUGO;
  net_acct->uid = 0;
  net_acct->gid = 0;
  net_acct->size = 4096;

  return 0;
}

int init_module(void)
{
  struct in_addr tmpaddr;
  int ret = 0;

  printk(KERN_INFO "inserting cloudmeter acct module\n");

  ret = fetch_addr("eth0", &tmpaddr);
  if (ret < 0) {
    printk(KERN_ALERT"Error: can not fetch network address\n");
    goto cleanup;
  }
  self_addr = tmpaddr.s_addr;
  /*
   *  Register the procfs to expose our data
   */
  ret = init_ec2stat();
  if (ret < 0) {
    printk(KERN_ALERT"Error: can not create ec2stat\n");
    goto cleanup;
  }

  /*
   *  Register the nfhook to capture all the packets flow through
   *  the default interface.
   */
  ret = nf_register_hook(&ec2_sender_ops);
  if (ret < 0) {
    printk(KERN_ALERT"Error: can not register nf sender hook\n");
    goto cleanup;
  }

  ret = nf_register_hook(&ec2_recever_ops);
  if (ret < 0) {
    printk(KERN_ALERT"Error: can not register nf recever hook\n");
    goto cleanup;
  }

 // ret = nf_register_hook(&ec2_recever_local_ops);
 // if (ret < 0) {
 //   printk(KERN_ALERT"Error: can not register nf recever local hook\n");
 //   goto cleanup;
 // }
  ret = 0;

done:
  return ret;

cleanup:
  remove_proc_entry(procfs_entry, NULL);
  nf_unregister_hook(&ec2_recever_ops);
//  nf_unregister_hook(&ec2_recever_local_ops);
  nf_unregister_hook(&ec2_sender_ops);
  goto done;
}


void cleanup_module(void)
{
  nf_unregister_hook(&ec2_recever_ops);
//  nf_unregister_hook(&ec2_recever_local_ops);
  nf_unregister_hook(&ec2_sender_ops);
  remove_proc_entry(procfs_entry, NULL);
  printk(KERN_INFO "cloudmeter acct module is cleaned out\n");
}
