main:
	linked list of link_t's x = parses links of file
	node = init_node(x)
	
	node_start(node)
	

node.c in change of constructing packet to send

node:
	forwarding_table_t forwarding_table; // why not just a hashmap?
	routing_table_t routing_table;
	Map<int, link_interface_t> sock_interfaceMap (maps sockets to interface structs) // for convenience with select()
	Map<ip, link_interface_t> nextMap (maps ip addresses to interface structs)

	int num_interfaces
	array: of link_interfaces
	hashmap: <sfd, arrayindex>

functions:

	node_init(linked list of link_t's)
	
		create empty hashmap
		create link_interface_factory
		factory_make(linked list, &hashmap)
			//populates hashmap and passes back array

	node_start(node_t node){
	
		while(1){
			select
				handle_selected_sfd()
			
			if time elapsed > 5 s:
				update_all_interfaces()
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
	make link_interface_factory
	
	git add  -- only needs to happen once per file
	git commit
	git pull
	git push
	
//NEIL TODO
	write node.h file
	write routing_table.c, .h
	write forwarding_table.c, .h
	
	WRITE OUR MAKEFILE PLEASE
	reorganize directories (I suppose this will go with creating makefile)
	