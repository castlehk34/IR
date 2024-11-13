#include <openssl/ssl.h>
#include <openssl/err.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>

struct timeval s, e;
struct timeval send_time, recv_time;

int main() {
    // OpenSSL 라이브러리 초기화
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    // DTLS 클라이언트 메서드 사용
    const SSL_METHOD *method = DTLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);

    // 클라이언트 소켓 생성 및 서버에 연결
    int client_sock = socket(AF_INET, SOCK_DGRAM, 0); // SOCK_DGRAM으로 UDP 소켓 생성
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(4433);
    // inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    inet_pton(AF_INET, "10.41.12.52", &addr.sin_addr);



    printf("Attempting to connect to DTLS server...\n");

    // SSL 객체 생성 및 DTLS 연결 설정
    SSL *ssl = SSL_new(ctx);
    BIO *bio = BIO_new_dgram(client_sock, BIO_NOCLOSE); // DTLS용 BIO 설정
    SSL_set_bio(ssl, bio, bio);

    // 서버 주소 설정
    connect(client_sock, (struct sockaddr*)&addr, sizeof(addr));
    BIO_ctrl(bio, BIO_CTRL_DGRAM_SET_CONNECTED, 0, &addr);

    // DTLS 핸드셰이크 시작
    gettimeofday(&s, NULL);
    if (SSL_connect(ssl) <= 0) {
        printf("DTLS connect failed\n");
        ERR_print_errors_fp(stderr);
    } else {
        printf("DTLS connection established\n");
    }
    gettimeofday(&e, NULL);
    long seconds = e.tv_sec - s.tv_sec;
    long microseconds = e.tv_usec - s.tv_usec;
    long elapsed = seconds * 1000000 + microseconds;
    printf("Handshake time: %ld microseconds\n", elapsed);

    // 서버에게 메시지 수신
    printf("--------------------\n");
    char buffer[16384];
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

        // 종료 조건
        if (strcmp(buffer, "exit") == 0) {
            SSL_write(ssl, buffer, strlen(buffer));
            break;
        }

        int n = atoi(buffer); // n을 정수로 변환
        if (n <= 0) {
            printf("Please enter a valid number.\n");
            continue; // 잘못된 입력일 경우 루프를 계속
        }

        // 2. 서버로 n 전송
        SSL_write(ssl, buffer, strlen(buffer));

        // 3. 서버로부터 n 바이트 랜덤 데이터 수신
        double total_time = 0.00;
        double avg_time = 0.00;
        long elapsed_times[100];
        int data_bytes_read = 0;  // 일반 데이터 수신 용도
        int time_bytes_read = 0;  // 시간 데이터 수신 용도

        for (int i = 0; i < 100; i++) {
            memset(buffer, 0, sizeof(buffer));  // 버퍼 초기화
            data_bytes_read = 0;  // 일반 데이터 수신 용도
            time_bytes_read = 0;  // 시간 데이터 수신 용도
            int packet_size = 1024;
            int rcvd = 0;

            // 일반 데이터 수신
            do {
                rcvd = SSL_read(ssl, buffer, 8192);
                if (rcvd > 0) {
                    data_bytes_read += rcvd;
                    printf("%d\n", data_bytes_read);
                }
            } while (data_bytes_read < n);

            if (data_bytes_read > 0) {
                // 데이터 수신 완료 ACK 전송
                SSL_write(ssl, "ACK", 3);

                // 4. send_time 수신
                memset(&send_time, 0, sizeof(struct timeval));
                time_bytes_read = SSL_read(ssl, (char*)&send_time, sizeof(struct timeval));  // 시간 데이터 수신

                // 5. recv_time 측정
                gettimeofday(&recv_time, NULL);
                long seconds = recv_time.tv_sec - send_time.tv_sec;
                long microseconds = recv_time.tv_usec - send_time.tv_usec;
                long elapsed = seconds * 1000000 + microseconds;

                elapsed_times[i] = elapsed;  // 각 수신 시간 기록
                total_time += elapsed;  // 총 시간을 누적
            } else {
                int ssl_error = SSL_get_error(ssl, data_bytes_read);
                printf("Error reading from DTLS: %d\n", ssl_error);
                break;
            }
        }
        printf("send_time   : %ld\n", send_time);
        printf("recv_time   : %ld\n", recv_time);
        avg_time = total_time / 100.00;
        printf("\nReceived %d bytes from DTLS server\n", data_bytes_read);  // 수신된 바이트 수 출력
        printf("100 times average Latency: %f microseconds\n", avg_time);
        printf("--------------------\n");
    }

    // DTLS 종료 및 리소스 해제
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_sock);
    SSL_CTX_free(ctx);
}
