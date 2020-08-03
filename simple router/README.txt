Group:
  Wenkai Zheng: wenkaizheng@email.arizona.edu
  Zehua Zhang: zehuazhang@email.arizona.edu

Description:
	Our router can work with testing of project description, and we follow the working principle of the router when processing frames.
	ARP protocol implementation has two parts, one is ARP request, the other is reply ARP. For ARP request, send ARP broadcast request message, ask MAC address. For ARP reply, ARP unicast reply message with own MAC address.Using ARP cache table to save IP and corresponding MAC.When communicating with other hosts, first determine whether they are on the same network segment as themselves. If they are on the same network segment, send an ARP broadcast to seek the MAC address of the target IP address. If not in the same network segment, send ARP broadcast to seek the MAC address of the gateway.
  	We using stub code to Identify, encapsulate, and send packets.After the project was completed, we were familiar with the stub code and had a deeper understanding of the basic workflow and principles of the router.