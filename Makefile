all:
	g++ simple_transfer/simple_sender.cpp -o simple_transfer/simple_sender
	g++ simple_transfer/simple_receiver.cpp -o simple_transfer/simple_receiver
	g++ selective_repeat_transfer/sr_sender.cpp -o selective_repeat_transfer/sr_sender
	g++ selective_repeat_transfer/sr_receiver.cpp -o selective_repeat_transfer/sr_receiver
clean:
	rm -f simple_transfer/simple_sender simple_transfer/simple_receiver
	rm -f selective_repeat_transfer/sr_sender selective_repeat_transfer/sr_receiver
build_net_env:
	sudo dnctl pipe 1 config plr 0.2 delay 200 bw 200Kbit/s
	sudo dnctl show
	echo "dummynet in proto udp from any to any pipe 1" > pf.conf
	echo "dummynet out proto udp from any to any pipe 1" >> pf.conf
	sudo pfctl -e
	sudo pfctl -f pf.conf
close_net_env:
	sudo dnctl -q flush
	sudo pfctl -f /etc/pf.conf 
	sudo pfctl -d
