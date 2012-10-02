main:
	linked list of link_t's x = parses links of file
	node = init_node(x)
	
	node_start(node)
	

node.c in change of constructing packet to send

node:
	forwarding_table_t forwarding_table; // why not just a hashmap?
	routing_table_t routing_table;
	
	//for taking sfd from select() and telling interface to read
	sfd_interface_map<sfd, link_interface_t> sock_interfaceMap (maps sockets to interface structs) // for convenience with select()
	// for getting forward vip from forwarding table and getting interface to send packet
	ip_interface_map<ip, link_interface_t> nextMap (maps ip addresses to interface structs)

	int num_interfaces
	// for query_interfaces iteration
	link_interface links[num_interfaces] -- array of link_interfaces
	

functions:

	node_init(linked list of link_t's)
	
		int num_interfaces = : iterate through list to get num_interfaces
	
		create empty hashmap ip_interface_map;
		create empty hashmap sfd_interface_map;
		create link_interface_factory
		
			//populates hashmap and passes back array

node_start(node_t node){
	
		while(1){
			select
			
				handle_stdin()
				query_interfaces()  // checks if each interface up/down
					// handles updating_routing_table if necessary 
					//-- which then handles updating about new info: update_all_interfaces
			
				handle_selected_sfd()
			
			if time elapsed > 5 s:
				update_all_interfaces()
				
		
}
def query_interfaces(node_t node){
	for(i = 0; i<num_interfaces; i++):
		check if each interface up/down
			if status has changed:
				update_routing_table();
}

def update_all_interfaces():
	for interface in interfaces:
		interface_send(interface, this.routingTable)
		
def handle_selected(interface):
	ip_packet = interface_read_packet(interface)
	if (!ip_packet):
		error("null pakcet received")
	elif ip_packet.destination != this.destination:
		forward_packet(ip_packet)
	else:
		if ip_packet.type == RIP:
			update_routing_table(ip_packet.data)
		else:
			print ip_packet
			
def forward_packet(node_t node, ip_packet):
	next = forwarding_table.get(ip_packet.dest)
	((interface_t)nextMap.get(next)).send(ip_packet.data)

			
def update_forwarding_table(forwarding_table_t forward, line):
	forward.put(line.dest, line.nextHop)
	
// forwarding_table.h

struct forwarding_table{


// routing_table.c

def update_routing_table(routing_table_t table, forwarding_table_t forward, routing_info):
	for line in routing_info:
		if table.get(line.dest) == NULL or table.get(line.dest).distance > line.distance + 1:
			table.put(line.dest, line.nextHop, line.distance + 1)
			update_forwarding_table(forward, line)
		else:
			pass // do nothing

where routing_info in form:
uint16_t command;
uint16_t num_entries;
struct {
uint32_t cost;
uint32_t address;
} entries[num_entries];
	

//ALEX TODO:
	make link_interface.c
	make link_interface.h

	write handle selected
	write wrap_ip_packet  // don't have to deal with fragmentation
	write unwrap_ip_packet
	
	questions:
		all in ipv4 right???? when do i need to make a distinction??
	
	git add  -- only needs to happen once per file
	git commit
	git pull
	git push
	
//NEIL TODO
	write node.h file
	write routing_table.c, .h
	write forwarding_table.c, .h
