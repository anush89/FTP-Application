FTP-Application
===============

Compile the server "gcc server.c -lpthread -o output_file_name".

Compile the client "gcc client.c -o another_output_file_name".

Run the server first "./output_file_name udp_port_no tcp_port_no" (Ex:./myServer 3333 4444) default is set as net01.utdallas.edu, udp port=3333, tcp port=4444.

Run the client "./another_output_file_name" (Ex:./myClient)

Note: The hostname, tcp_port_no and udp_port_no should be provided in the client.c file. If the server is running on any machine other than net01.utdallas.edu then it should be set.
