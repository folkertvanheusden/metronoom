#include <atomic>
#include <stdio.h>
#include <signal.h>
#include <string_view>
#include <thread>
#include <time.h>
#include <unistd.h>
#include <rtpmidid/iobytes.hpp>
#include <rtpmidid/mdns_rtpmidi.hpp>
#include <rtpmidid/poller.hpp>
#include <rtpmidid/rtpserver.hpp>
#include <sys/resource.h>
#include <sys/time.h>


#define BILLION 1000000000ll

int instrument = 0x34;
std::atomic_bool playing { true };

uint64_t start = 0;
uint64_t i = 0;

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

void timer_handler(sigval sv)
{
	if (playing)
		send(am, instrument);

	static int cnt = 0;
	cnt++;

	uint64_t now = get_us_rt();

	printf("%.3f ping %d/%.6f\n", now / 1000.0, cnt, (now - start) / (i / 1000.0));
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

	if (setpriority(PRIO_PROCESS, getpid(), -20) == -1)
		perror("setpriority");

	char hostname[128];
	gethostname(hostname, sizeof hostname);
	name += hostname;

	am = new rtpmidid::rtpserver(name, port);

	mdns_rtpmidi.announce_rtpmidi(name, atoi(port.c_str()));

	am->connected_event.connect([port](std::shared_ptr<::rtpmidid::rtppeer> peer) {
		INFO("Remote client connects to local server at port {}. Name: {}", atoi(port.c_str()), peer->remote_name);
	});

	am->midi_event.connect([](rtpmidid::io_bytes_reader buffer) {
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

	struct sigevent sevp { 0 };
	sevp.sigev_notify = SIGEV_THREAD;
	sevp.sigev_notify_attributes = nullptr;
	sevp.sigev_value.sival_ptr = nullptr;
	sevp.sigev_notify_function = timer_handler;

	timer_t timerid { 0 };
	if (timer_create(CLOCK_MONOTONIC, &sevp, &timerid) == -1)
		perror("timer_create");

	i = BILLION * 60 / BPM;

	struct itimerspec its { 0 };
	its.it_value.tv_sec = its.it_interval.tv_sec = i / BILLION;
	its.it_value.tv_nsec = its.it_interval.tv_nsec = i % BILLION;
	if (timer_settime(timerid, 0, &its, nullptr) == -1)
		perror("timer_settime");

	start = get_us_rt();

	for(;;)
		sleep(86400);

	return 0;
}
