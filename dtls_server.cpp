#include <openssl/ssl.h>
#include <openssl/err.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>

struct timeval start, end;
struct timeval send_time, recv_time;

// 전역 변수를 사용해 ClientHello 수신 시각 저장
struct timeval clienthello_received_time;

// 메시지 콜백 함수
void message_callback(int write_p, int version, int content_type,
                      const void *buf, size_t len, SSL *ssl, void *arg) {
    // Handshake message 중 ClientHello (message type = 1) 확인
    if (content_type == SSL3_RT_HANDSHAKE && len > 0 && ((const unsigned char *)buf)[0] == SSL3_MT_CLIENT_HELLO) {
        gettimeofday(&clienthello_received_time, NULL);
        printf("ClientHello received. Start timing handshake from here.\n");
    }
}

int main() {
    // OpenSSL 라이브러리 초기화
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    // DTLS 서버 메서드 사용
    const SSL_METHOD *method = DTLS_server_method();
    SSL_CTX *ctx = SSL_CTX_new(method);

    SSL_CTX_set_msg_callback(ctx, message_callback);

    // 인증서와 개인 키 파일 로드 및 유효성 검사
    if (SSL_CTX_use_certificate_file(ctx, "server.crt", SSL_FILETYPE_PEM) <= 0) {
        fprintf(stderr, "Error loading certificate file\n");
        ERR_print_errors_fp(stderr);
        return EXIT_FAILURE;
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, "server.key", SSL_FILETYPE_PEM) <= 0) {
        fprintf(stderr, "Error loading private key file\n");
        ERR_print_errors_fp(stderr);
        return EXIT_FAILURE;
    }

    // 인증서와 개인 키가 일치하는지 확인
    if (!SSL_CTX_check_private_key(ctx)) {
        fprintf(stderr, "Private key does not match the certificate public key\n");
        return EXIT_FAILURE;
    }

    // UDP 소켓 생성 및 설정
    int server_sock = socket(AF_INET, SOCK_DGRAM, 0); // SOCK_DGRAM으로 UDP 소켓 생성
    int actual_recv_buf_size = 0;
    socklen_t optlen = sizeof(actual_recv_buf_size); 
    getsockopt(server_sock, SOL_SOCKET, SO_RCVBUF, &actual_recv_buf_size, &optlen);
    printf("Origin buffer size: %d bytes\n", actual_recv_buf_size);

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(4433);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
    printf("DTLS Server started and waiting for connections...\n");
    fflush(stdout);

    // SSL 객체 생성 및 DTLS 연결 설정 (핸드셰이크)
    struct timeval bio_start, bio_end;
    gettimeofday(&bio_start, NULL);
    SSL *ssl = SSL_new(ctx);
    BIO *bio = BIO_new_dgram(server_sock, BIO_NOCLOSE); // DTLS용 BIO 설정
    SSL_set_bio(ssl, bio, bio);
    gettimeofday(&bio_end, NULL);
    printf("aaaa %d\n", bio_end.tv_usec - bio_start.tv_usec);

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int received = recvfrom(server_sock, NULL, 0, MSG_PEEK, (struct sockaddr *)&client_addr, &client_len);
    if (received < 0) {
        perror("Receive failed");
        return EXIT_FAILURE;
    }

    // 클라이언트 주소 설정 및 연결 시작
    BIO_dgram_set_peer(bio, (struct sockaddr *)&client_addr);

    // DTLS 핸드셰이크 시작
    if (SSL_accept(ssl) <= 0) {
        printf("DTLS accept failed\n");
        ERR_print_errors_fp(stderr);
        return -1;
    }

    gettimeofday(&end, NULL);
    printf("ssss %ld, %ld\n", end.tv_sec, clienthello_received_time.tv_sec);
    long elapsed = (end.tv_sec - clienthello_received_time.tv_sec) * 1000000L
                   + (end.tv_usec - clienthello_received_time.tv_usec);

    printf("DTLS Handshake time: %ld microseconds\n", elapsed);


    // 클라이언트에게 환영 메시지 전송
    SSL_write(ssl, "Welcome to DTLS Server!!", 24);

    srand(time(NULL));
    // 메시지 송수신을 위한 루프
    char buffer[16384];
    while (1) {
        // 클라이언트의 요청 수신
        memset(buffer, 0, sizeof(buffer));
        int bytes_read = SSL_read(ssl, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("Client requested: %s\n", buffer);

            // 종료 조건
            if (strcmp(buffer, "exit") == 0) {
                printf("Connection closing...\n");
                break;
            }

            // n 바이트의 무작위 데이터 생성
            int n = atoi(buffer);
            // printf("nnnnnnn : %d\n",n);
            char *random_data = (char *)malloc(n);
            if (random_data) {
                for (int i = 0; i < 100; i++) {
                    printf("%dth send\n", i + 1);
                    for (int j = 0; j < n; j++) {
                        random_data[j] = rand() % 256;
                        // printf("%d : %d\n",j, random_data[j]);
                    }
                    printf("aaaaaaaaaaaaaa\n");
                    // 전송 시간 기록
                    gettimeofday(&send_time, NULL);
                    int bytes_written = 0;
                    while (bytes_written < n) {
                        // 전송할 남은 데이터 중에서 패킷 크기 이하로 잘라 전송
                        // int size_to_send = (n - bytes_written < 16384) ? (n - bytes_written) : 16384;
                        // 각 패킷을 독립적으로 전송
                        int adsfadsf = SSL_write(ssl, random_data + bytes_written, 8192);
                        if (adsfadsf > 0){
                            // printf("%d", adsfadsf);
                            bytes_written += adsfadsf;
                        }
                    }
                    // printf("d");

                    if (bytes_written <= 0) {
                        int ssl_error = SSL_get_error(ssl, bytes_written);
                        printf("Error writing to DTLS: %d\n", ssl_error);
                    }
                    
                    free(random_data);
                    random_data = (char *)malloc(n);

                    // ACK 수신 확인
                    memset(buffer, 0, sizeof(buffer));
                    SSL_read(ssl, buffer, sizeof(buffer));
                    if (strcmp(buffer, "ACK") == 0) {
                        SSL_write(ssl, (char*)&send_time, sizeof(struct timeval));
                    }
                }
            } else {
                SSL_write(ssl, "Error generating random data", 30);
            }
        } else {
            int ssl_error = SSL_get_error(ssl, bytes_read);
            printf("Error reading from DTLS: %d\n", ssl_error);
            break;
        }
    }

    // DTLS 종료 및 리소스 해제
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(server_sock);
    SSL_CTX_free(ctx);
}
