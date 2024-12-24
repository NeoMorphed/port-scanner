
#include "pch.h"
#ifdef __WINDOWS__
#endif

#ifdef __LINUX__
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>

#define DEFAULT_PORT "27015"

typedef uint32_t u32;

// @Cleanup There are many indefs which clutter up the code. We should figure out a way to implement windows and linux
// in a cleaner fashion.

struct Port_Scan_Thread_Info
{
	int* valid_ports;
	const char* ip_to_scan;
	const char* info_name;
	int port_start;
	int port_end;
	int valid_port_count;
};
void scan_ports(Port_Scan_Thread_Info* info);
#ifdef __WINDOWS__
DWORD WINAPI thread_function( LPVOID lpParam )
{
	scan_ports((Port_Scan_Thread_Info*)lpParam);
	return 0;
}
#endif
#ifdef __LINUX__
sem_t semaphore;
void* thread_function(void* param)
{
	scan_ports((Port_Scan_Thread_Info*)param);
	sem_post(&semaphore);
	return NULL;
}
#endif

void log_error(char* fmt, ...)
{
#ifdef __WINDOWS__
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, 12);
#endif
#ifdef __LINUX__
	printf("\033[31m");
#endif
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
#ifdef __WINDOWS__
	SetConsoleTextAttribute(hConsole, 7);
#endif
#ifdef __LINUX__
	printf("\033[0m");
#endif
}
inline void ensure_arg_val_exists(char** args, int argc, int cur_arg)
{
	if ((cur_arg + 1) >= argc) {
		log_error("ERROR: No value specified for %s.\n", args[cur_arg]);
		exit(1);
	}
}
inline bool ensure_first_char_is_digit(const char* str, const char* cur_arg)
{
	if (!isdigit(str[0])) {
		log_error("ERROR: Value \"%s\" for argument \"%s\" is not a number.\n", str, cur_arg);
		exit(1);
	}
	return true;
}
int main(int argc, char** argv)
{
	//assert(false && "Testing message on assert!!!!");
	int thread_count = 1;
	int start_port = 1;
	int end_port = 10000;
	int port_count = 10000;

	// Argument parsing.
	for(int i = 0; i < argc; i++) {
		if (i == 0) continue;
		if (strcmp(argv[i], "-") == 0) {
			log_error("ERROR: no argument specified for \"%s\"\n.", argv[i]);
			return 1;
		}
		// Thread count argument
		if (strcmp(argv[i], "-t") == 0) {
			ensure_arg_val_exists(argv, argc, i);
			char* arg_val = argv[i + 1];
			size_t len = strlen(arg_val);
			for (u32 j = 0; j < len; j++) {
				if (!isdigit(arg_val[j])) {
					log_error("ERROR: Value \"%s\" for argument \"%s\" is not a number.\n", arg_val, argv[i]);
					return 1;
				}
			}
			ensure_first_char_is_digit(argv[i + 1], argv[i]);
			int t_count = atoi(argv[i + 1]);
			if (t_count > 8) {
				printf("Argument %s value greater than max of 8. Thread count set to 8.\n", argv[i]);
				t_count = 8;
			}
			else if (t_count == 0) {
				printf("Argument %s value less than minimum. Thread count set to 1.\n", argv[i]);
				t_count = 1;
			}
			thread_count = t_count;
			i++;
			continue;
		}
		// Port range argument.
		else if (strcmp(argv[i], "-p") == 0) {
			ensure_arg_val_exists(argv, argc, i);
			ensure_first_char_is_digit(argv[i + 1], argv[i]);
			char* arg_val = argv[i + 1];
			size_t len = strlen(arg_val);

			// 11 is the max size of the argument.
			// 10000-65535 -> MAX PORT VALUE 65535 -> 5 digits 
			if (len > 11) {
				log_error("ERROR: Value \"%s\" is greater than max length for argument \"%s\"\n.", arg_val, argv[i]);
				return 0;
			}
			char p_start[6];
			memset(p_start, 0, 6);
			char p_end[6];
			memset(p_start, 0, 6);
			bool end_start = false;
			u32 p_end_index = 0;
			for (u32 j = 0; j < len; j++) {
				bool is_digit = isdigit(arg_val[j]);
				if (is_digit || arg_val[j] == '-') {
					if (!end_start && is_digit) {
						if (j > 4) {
							log_error("ERROR: Port start value \"%s\" is greater than max of 5 digits for argument \"%s\".\n", arg_val, argv[i]);
							return 0;
						}
						p_start[j] = arg_val[j];
						continue;
					}
					if (arg_val[j] == '-'){
						end_start = true;
						continue;
					}
					if (end_start && is_digit){
						if (p_end_index > 4) {
							log_error("ERROR: Port end value \"%s\" is greater than max of 5 digits for argument \"%s\".\n", arg_val, argv[i]);
							return 0;
						}
						p_end[p_end_index] = arg_val[j];
						p_end_index++;
					}
				}
				else {
					log_error("ERROR: Value \"%s\" is not valid for argument \"%s\"\n", arg_val, argv[i]);
					return 1;	
				}
			}
			start_port = atoi(p_start);
			end_port = atoi(p_end);
			if (end_port < start_port) {
				log_error("ERROR: Port start greater than port end for argument \"%s\"\n", argv[i]);
				return 0;
			} 
			port_count = end_port - start_port;
		} 
	}

	//const char* LOCAL_HOST = "127.0.0.1";
	const char* LOCAL_HOST = "127.0.0.1";
#ifdef __WINDOWS__
	WSADATA wsaData;
	//Initialize Winsock
	int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed: %d\n", iResult);
		return 1;
	}
#endif

	in_addr in_ip;
	inet_pton(AF_INET, LOCAL_HOST, &in_ip);

#ifdef __WINDOWS__
	HANDLE icmp_handle;
	char* req_data = "Hello";
	void* reply_buf = malloc(sizeof(ICMP_ECHO_REPLY) + 6 + 8);
	memset(reply_buf, 0, sizeof(ICMP_ECHO_REPLY) + 6 + 8);

	//For Ipv4 Address
	icmp_handle = IcmpCreateFile();
	iResult = IcmpSendEcho(icmp_handle, in_ip.S_un.S_addr, req_data, 6, NULL, reply_buf, sizeof(ICMP_ECHO_REPLY) + 6 + 8, 1000);
	if (iResult == 0) {
		printf("error at IcmpSendEcho: %lu\n", GetLastError());
		printf("Can't reach host: %s\n", LOCAL_HOST);
		return 0;
	}
	else printf("Host: %s is up\n", LOCAL_HOST);
	free(reply_buf);
#endif

	const int MAX_PORT = 65535;
	const int MAX_THREAD_COUNT = 8;

	// for(int i = 0; i < thread_count; i++) {
	// 	HANDLE t_handle = CreateThread(NULL, CREATE_SUSPENDED, thread_function, &scan_infos[i], 0, NULL);
	// 	if (t_handle == NULL) printf("Failed to create thread for info%d\n", i);
	// 	thread_handles[i] = t_handle;
	// }
	// port_count is 1 less than the actual port count. We should probably fix this so everything works with port_count
	// equal to the actual port count instead of port_count + 1
	int remainder = port_count % thread_count;
	int split = port_count / thread_count;

	Port_Scan_Thread_Info scan_infos[MAX_THREAD_COUNT]; 

	int next_value = 0;

	// @Bug End port of last thread is over by 1.
	for (int i = 0; i < thread_count; i++) {
		scan_infos[i].valid_port_count = 0;
		// @Bug We will need to fix this when we fix the remainder bug.
		scan_infos[i].valid_ports = (int*)malloc(sizeof(int) * (split + 1));
		scan_infos[i].ip_to_scan = LOCAL_HOST;
		scan_infos[i].info_name = "info";
		if (i == 0) {
			scan_infos[i].port_start = start_port;
			scan_infos[i].port_end = start_port + split;
			next_value = start_port + split;
			continue;
		}
		scan_infos[i].port_start = next_value + 1;
		// @Bug this will break if the remainder is larger than the amount of threads we have.
		if (remainder > 0) {
			next_value += 1;
			remainder--;
		}
		next_value += split;
		scan_infos[i].port_end = next_value;
	}
	if (scan_infos[thread_count - 1].port_end > MAX_PORT) {
		scan_infos[thread_count - 1].port_end = MAX_PORT;
	}
	// for (int i = 0; i < thread_count; i++) {
	// 	printf("start %d\n", scan_infos[i].port_start);
	// 	printf("end %d\n", scan_infos[i].port_end);
	// }

#ifdef __WINDOWS__	
	HANDLE thread_handles[MAX_THREAD_COUNT];
	// @Cleanup If a thread fails to create than we won't scan all the ports we want. We should create the threads first in
	// standby then activate them here.
	for(int i = 0; i < thread_count; i++) {
		HANDLE t_handle = CreateThread(NULL, 0, thread_function, &scan_infos[i], 0, NULL);
		if (t_handle == NULL) printf("Failed to create thread for info%d\n", i);
		thread_handles[i] = t_handle;
	}
	WaitForMultipleObjects(thread_count, thread_handles, true, INFINITE);
#endif
#ifdef __LINUX__
	bool success = false;
	pthread_t threads[MAX_THREAD_COUNT];
	for(int i = 0; i < thread_count; i++) {
		success = pthread_create(&threads[i], NULL, thread_function, &scan_infos[i]);
		if (success != 0) printf("Failed to create thread for info%d\n", i);
	}
	if(sem_wait(&semaphore) < 0) {
		log_error("sem_wait failed: %s\n", strerror(errno));
		return 1;
	}
	for (int i = 0; i < MAX_THREAD_COUNT; i++) {
		pthread_join(threads[i], NULL);
	}
#endif
	for(int i = 0; i < thread_count; i++) {
		for(int j = 0; j < scan_infos[i].valid_port_count; j++) {
			printf("%d\n", scan_infos[i].valid_ports[j]);
		}
	}
}

void scan_ports(Port_Scan_Thread_Info* info)
{
	int success = 0;
	for (int i = info->port_start; i <= info->port_end; i++) {
		// struct sockaddr_in sa;
		// sa.sin_family = AF_INET;
		addrinfo* result = NULL;
		//addrinfo* ptr = NULL;
		addrinfo hints;

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		char num_buf[6];
		//Resolve the server address and port
		snprintf(num_buf, 6, "%d", i);
		success = getaddrinfo(info->ip_to_scan, num_buf, &hints, &result);
		if (success != 0) {
			printf("getaddrinfo failed: %d\n", success);
			return;
		}
#ifdef __WINDOWS__
		u_long ioctl_sock_mode = 1;
		SOCKET sock = INVALID_SOCKET;
#endif
#ifdef __LINUX__
		int sock = 0;
#endif
		//ptr = result;
		sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

#ifdef __WINDOWS__
		if (sock == INVALID_SOCKET) {
			printf("error at socket(): %ld\n", WSAGetLastError());
			return;
		}
		success = ioctlsocket(sock, FIONBIO, &ioctl_sock_mode);
		if (success != NO_ERROR)
			printf("ioctlsocket failed with error: %ld\n", success);
#endif

#ifdef __LINUX__
		if (sock == -1) {
			printf("Failed to create socket for port \033[31m%d\033[0m. Error code: %d\n", i, errno);
			return;
		}
		int flags = fcntl(sock, F_GETFL, 0);
		if (flags == -1) { 
			log_error("flags error\n"); 
			exit(1);
		}
		flags = flags | O_NONBLOCK;
		fcntl(sock, F_SETFL, flags);
#endif
		// Connect to server
		success = connect(sock, result->ai_addr, (int)result->ai_addrlen);
#ifdef __WINDOWS__
		if (success == SOCKET_ERROR) {
			if (WSAGetLastError() != WSAEWOULDBLOCK) {
				printf("%s: %d\n", info->info_name, i);
				printf("Error at connect: error code %ld\n", WSAGetLastError());
				closesocket(sock);
				sock = INVALID_SOCKET;				
			}
			else {
				fd_set set_writable; 
				fd_set set_readable;
				FD_ZERO(&set_writable);
				FD_SET(sock, &set_writable);
				FD_ZERO(&set_readable);
				FD_SET(sock, &set_readable);
				const TIMEVAL timeVal = {0, (long)0.1};

				success = select(0, &set_readable, &set_writable, NULL, &timeVal);
				if (success > 0) {
					info->valid_ports[info->valid_port_count] = i;
					info->valid_port_count++;
				}
			}
		}
		else {
			info->valid_ports[info->valid_port_count] = i;
			info->valid_port_count++;
		}
		closesocket(sock);	
#endif
#ifdef __LINUX__
		if (success == -1) {
			if (errno != EINPROGRESS) {
				printf("%d: %d\n", info->port_start, i);
				log_error("ERROR: Socket connect error code %ld\n", errno);
				close(sock);
				sock = -1;
			} else {
				fd_set set_writable;
				fd_set set_readable;
				FD_ZERO(&set_writable);
				FD_SET(sock, &set_writable);
				FD_ZERO(&set_readable);
				FD_SET(sock, &set_readable);
				timeval time_val = {0, (long)0.1};

				success = select(0, &set_readable, &set_writable, NULL, &time_val);
				if (success > 0) {
					info->valid_ports[info->valid_port_count] = i;
					info->valid_port_count++;
				}
			}
		} else {
			info->valid_ports[info->valid_port_count] = i;
			info->valid_port_count++;
			close(sock);
		}
#endif
		freeaddrinfo(result);

	}
}




