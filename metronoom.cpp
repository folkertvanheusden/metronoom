#include <atomic>
#include <stdio.h>
#include <string_view>
#include <thread>
#include <time.h>
#include <unistd.h>
#include <rtpmidid/iobytes.hpp>
#include <rtpmidid/mdns_rtpmidi.hpp>
#include <rtpmidid/poller.hpp>
#include <rtpmidid/rtpserver.hpp>


rtpmidid::rtpserver *am = nullptr;

rtpmidid::mdns_rtpmidi mdns_rtpmidi;

void send(rtpmidid::rtpserver *const am, const uint8_t instrument)
{
	uint8_t msg[] { 0x99, instrument, 127 };
	rtpmidid::io_bytes b(msg, sizeof msg);

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

uint64_t get_us_rt()
{
        struct timespec tp { 0 };

        if (clock_gettime(CLOCK_REALTIME, &tp) == -1) {
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
	printf("-i x   (percussion) instrument to use (1-based numbering!)\n");
	printf("-V     show version\n");
	printf("-h     this help\n");
}

int main(int argc, char *argv[])
{
	std::string port = "15115", name = "metronoom_";
	double BPM = 116;
	int instrument = 0x34;

	int c = -1;
	while((c = getopt(argc, argv, "p:b:i:Vh")) != -1) {
		if (c == 'p')
			port = optarg;
		else if (c == 'b')
			BPM = atof(optarg);
		else if (c == 'i')
			instrument = atoi(optarg) - 1;
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

	char hostname[128];
	gethostname(hostname, sizeof hostname);
	name += hostname;

	am = new rtpmidid::rtpserver(name, port);

	mdns_rtpmidi.announce_rtpmidi(name, atoi(port.c_str()));

	am->connected_event.connect([port](std::shared_ptr<::rtpmidid::rtppeer> peer) {
		INFO("Remote client connects to local server at port {}. Name: {}", atoi(port.c_str()), peer->remote_name);
	});

	std::atomic_bool playing { true };

	am->midi_event.connect([&playing](rtpmidid::io_bytes_reader buffer) {
			size_t len = buffer.size();

			if (len) {
				uint8_t msg = buffer.read_uint8();

				if (msg == 0xfa)
					playing = true;

				else if (msg == 0xfc)
					playing = false;
			}
		});

	std::thread poller(poller_thread);

	int64_t prev = get_us();
	const int64_t interval = 60000000 / BPM;

	for(;;) {
		int64_t now = get_us();
		int64_t slp = interval - (now % interval);
		int64_t then = now + slp;

		struct timespec ts = { then / 1000000, (then % 1000000) * 1000l };
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, nullptr);

		if (playing)
			send(am, instrument);

		int64_t now_after = get_us();

		int64_t delta = now_after - prev;

		prev = now_after;

		uint64_t now_rt = get_us_rt();
		time_t t = now_rt / 1000000;
		struct tm *tm = localtime(&t);

		int64_t delta_err = delta - interval;

		printf("%lu (%04d:%02d:%02d %02d:%02d:%02d.%06lu) %ldus (%ldus | %5ldus | %8.4f%%) BEAT\n",
				now_rt, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, now_rt % 1000000,
				delta, interval, delta_err, delta_err * 100.0 / interval);
	}

	return 0;
}
