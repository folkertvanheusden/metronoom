#include <stdio.h>
#include <string_view>
#include <thread>
#include <unistd.h>
#include <rtpmidid/iobytes.hpp>
#include <rtpmidid/poller.hpp>
#include <rtpmidid/rtpserver.hpp>

#include "mdns_rtpmidi.hpp"


rtpmidid::rtpserver *am = nullptr;

rtpmidid::mdns_rtpmidi mdns_rtpmidi;

void send(rtpmidid::rtpserver *const am)
{
	uint8_t msg[] { 0x99, 0x34, 127 };
	rtpmidid::io_bytes b(msg, 5);

	am->send_midi_to_all_peers(rtpmidid::io_bytes_reader(b));
}

uint64_t get_us()
{
        struct timespec tp { 0 };

        if (clock_gettime(CLOCK_MONOTONIC, &tp) == -1) {
                perror("clock_gettime");
                return 0;
        }

        return tp.tv_sec * 1000l * 1000l + tp.tv_nsec / 1000;
}

void poller_thread()
{
	while (rtpmidid::poller.is_open())
		rtpmidid::poller.wait();
}

void usage()
{
	printf("-p x   port to listen on (hopefully no need to configure this)\n");
	printf("-b x   BPM to beat on\n");
	printf("-V     show version\n");
	printf("-h     this help\n");
}

int main(int argc, char *argv[])
{
	std::string port = "15115", name = "metronoom";
	double BPM = 116;

	int c = -1;
	while((c = getopt(argc, argv, "p:b:Vh")) != -1) {
		if (c == 'p')
			port = optarg;
		else if (c == 'b')
			BPM = atof(optarg);
		else if (c == 'V') {
			printf("metronoom (C) 2021 by Folkert van Heusden\n");
			return 0;
		}
		else if (c == 'h') {
			usage();
			return 0;
		}
		else {
			usage();
			return 1;
		}
	}

	am = new rtpmidid::rtpserver(name, port);

	mdns_rtpmidi.announce_rtpmidi(name, atoi(port.c_str()));

	am->connected_event.connect([port](std::shared_ptr<::rtpmidid::rtppeer> peer) {
		INFO("Remote client connects to local server at port {}. Name: {}", atoi(port.c_str()), peer->remote_name);
	});

	std::thread poller(poller_thread);

	int64_t prev = get_us();
	const int64_t interval = 60000000 / BPM;

	for(;;) {
		int64_t now = get_us();
		int64_t slp = interval - (now % interval);

		if (slp)
			usleep(slp);

		send(am);

		int64_t now_after = get_us();
		int64_t delta = now_after - prev;
		printf("%ld (%ld | %8.4f%%) BEAT\n", delta, interval, double(delta - interval) / interval);
		prev = now_after;
	}

	return 0;
}
