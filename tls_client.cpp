#include <openssl/ssl.h>
#include <openssl/err.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>

// #include <oqs/oqs.h>

struct timeval s, e;
struct timeval send_time, recv_time;

static SSL_CTX *create_context(){
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if(!ctx) {
	printf("ctx is NULL\n"); 
	ERR_print_errors_fp(stderr);
    }
    SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);
    return ctx;
}

int main() {
    // OpenSSL 라이브러리 초기화
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    // TLS 클라이언트 메서드 사용
    SSL_CTX *ctx = create_context();
    printf("a\n");
    SSL_CTX_load_verify_locations(ctx, "./dil2_crt.pem", NULL);
    printf("b\n");
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL); // SSL_VERIFY_NONE
    printf("c\n");
	SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
    printf("d\n");


    
    // 포스트 퀀텀 알고리즘 설정
    // SSL_CTX_set_cipher_list(ctx, "OQS-dilithium-2-SHA256"); // Kyber-512 사용
    // SSL_CTX_set1_groups_list(ctx, "dilithium2");

    // 클라이언트 소켓 생성 및 서버에 연결
    int client_sock = socket(AF_INET, SOCK_STREAM, 0);
    struck sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(4433);
    inet_pton(AF_INET, "10.41.12.52", &addr.sin_addr);
    // inet_pton(AF_INET, "192.168.0.2", &addr.sin_addr);
    printf("Attempting to connect to TLS server...\n");

    if (connect(client_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Connect failed");
    } else {
        printf("Connected to server\n");
    }

    // SSL 객체 생성 및 연결 설정 (인증서 검증)
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, client_sock);
    // 세션키 생성 및 송신
        
    // tls 핸드쉑 시작
    gettimeofday(&s, NULL);
    SSL_connect(ssl);
    // 핸드셰이크 종료
    gettimeofday(&e, NULL);
    long seconds = e.tv_sec - s.tv_sec;
    long microseconds = e.tv_usec - s.tv_usec;
    long elapsed = seconds * 1000000 + microseconds;
    printf("Handshake time: %ld microseconds\n", elapsed);

    // 서버에게 메시지 수신
    printf("--------------------\n");
    char buffer[16384];
    // char buffer[8192];
    int bytes_read = SSL_read(ssl, buffer, sizeof(buffer));
    buffer[bytes_read] = '\0';
    printf("%s\n", buffer);
    printf("--------------------\n");
    printf("Enter the number of bytes to request (or 'exit' to quit)\n");
    
    // 메시지 송수신을 위한 루프
    while (1) {
        // 1. 서버에 원하는 바이트의 크기 n 요청
        printf(">> ");
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = '\0';  // 개행 문자 제거
        printf("a"); 
        // 종료 조건
        if (strcmp(buffer, "exit") == 0) {
            SSL_write(ssl, buffer, strlen(buffer));
            break;
        }
        printf("a"); 
        int n = atoi(buffer); // n을 정수로 변환
        if (n <= 0) {
            printf("Please enter a valid number.\n");
            continue; // 잘못된 입력일 경우 루프를 계속
        }
        printf("a"); 
        // 2. 서버로 n 전송
        SSL_write(ssl, buffer, strlen(buffer));

        // 3. 서버로부터 n 바이트 랜덤 데이터 수신
        double total_time = 0.00;
        double avg_time = 0.00;
        long elapsed_times[100];
        int data_bytes_read = 0;  // 일반 데이터 수신 용도
        int time_bytes_read = 0;  // 시간 데이터 수신 용도

        for (int i = 0; i < 100; i++) {
            // printf("[%dth receive]\n", i+1);
            memset(buffer, 0, sizeof(buffer));  // 버퍼 초기화
            data_bytes_read = 0;  // 일반 데이터 수신 용도
            time_bytes_read = 0;  // 시간 데이터 수신 용도
            int rcvd = 0;

            // 일반 데이터 수신
            do {
                rcvd = SSL_read(ssl, buffer, n);
                if (rcvd > 0) {
                    data_bytes_read += rcvd;
                    printf("%d\n", data_bytes_read);
                }
            } while (data_bytes_read < n);

            if (data_bytes_read > 0) {
                // 데이터 수신 완료 ACK 전송
                SSL_write(ssl, "ACK", 3);
                // printf("a, %d\n", data_bytes_read);

                // 4. send_time 수신
                memset(&send_time, 0, sizeof(struct timeval));
                time_bytes_read = SSL_read(ssl, (char*)&send_time, sizeof(struct timeval));  // 시간 데이터 수신
                // printf("Time data size: %d\n", time_bytes_read);  // 시간 데이터 크기 출력

                // 5. rev_time 측정
                gettimeofday(&recv_time, NULL);
                // printf("%ld\n", recv_time.tv_sec);
                long seconds = recv_time.tv_sec - send_time.tv_sec;
                long microseconds = recv_time   .tv_usec - send_time.tv_usec;
                long elapsed = seconds * 1000000 + microseconds;

                // printf("\nReceived %d bytes from server\n", data_bytes_read);  // 수신된 바이트 수 출력
                // printf("Latency time: %ld microseconds\n", elapsed);
                elapsed_times[i] = elapsed;  // 각 수신 시간 기록
                total_time += elapsed;  // 총 시간을 누적
                // printf("b, %d\n", data_bytes_read);
            } else {
                int ssl_error = SSL_get_error(ssl, data_bytes_read);
                printf("Error reading from SSL: %d\n", ssl_error);
                break;
            }
            // printf("c, %d\n", data_bytes_read);
            // printf("--------------------\n");
        }
        avg_time = total_time / 100.00;
        printf("\nReceived %d bytes from TLS server\n", data_bytes_read);  // 수신된 바이트 수 출력
        printf("100 times average Latency: %f microseconds\n", avg_time);
        printf("--------------------\n");
    }

    // SSL 종료 및 리소스 해제 
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_sock);
    SSL_CTX_free(ctx);
}
