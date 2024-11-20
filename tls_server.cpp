#include <openssl/ssl.h>
#include <openssl/err.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>

// #include <oqs/oqs.h>

struct timeval start, end;
struct timeval send_time, recv_time;

int main() {
    // OpenSSL 라이브러리 초기화
    printf("1\n");
    SSL_library_init();
    printf("2\n");
    OpenSSL_add_all_algorithms();
    printf("3\n");
    SSL_load_error_strings();
    printf("4\n");


    // TLS 서버 메서드 사용
    // const SSL_METHOD *method = TLS_server_method();
    // printf("5\n");
    // SSL_CTX *ctx = SSL_CTX_new(method);
    // printf("6\n");
    SSL_CTX* ctx = SSL_CTX_new(SSLv23_server_method());
    printf("5\n");
    SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);
    printf("6\n");
    printf("SSL_CTX address: %p\n", (void *)ctx);
    
    // 포스트 퀀텀 알고리즘 설정
    // int aa =  SSL_CTX_set_cipher_list(ctx, "OQS-dilithium-2-SHA256"); 
    // printf("7: %d\n", aa);
    // aa = SSL_CTX_set1_groups_list(ctx, "dilithium2");
    // printf("8: %d\n", aa);
    
    

    // 인증서와 개인 키 파일 로드 및 유효성 검사
    if (SSL_CTX_use_certificate_file(ctx, "dil2_crt.pem", SSL_FILETYPE_PEM) <= 0) {
        fprintf(stderr, "Error loading certificate file\n");
        ERR_print_errors_fp(stderr);
        return EXIT_FAILURE;
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, "dil2_privkey.key", SSL_FILETYPE_PEM) <= 0) {
        fprintf(stderr, "Error loading private key file\n");
        ERR_print_errors_fp(stderr);
        return EXIT_FAILURE;
    }
    
    // SSL_CTX_set_keylog_callback(ctx, keylog_callback);
    SSL_CTX_set_num_tickets(ctx, 0);    

    // 인증서와 개인 키가 일치하는지 확인
    if (!SSL_CTX_check_private_key(ctx)) {
        fprintf(stderr, "Private key does not match the certificate public key\n");
        return EXIT_FAILURE;
    }

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    int actual_recv_buf_size = 0;
    int actual_send_buf_size = 0;
    int recv_buf_size = 40000;
    int send_buf_size = 40000; // 원하는 크기 (바이트 단위)
    socklen_t optlen = sizeof(actual_recv_buf_size); 
    getsockopt(server_sock, SOL_SOCKET, SO_RCVBUF, &actual_recv_buf_size, &optlen);
    printf("Origin buffer size: %d bytes\n", actual_recv_buf_size);
    // TCP 수신 버퍼 크기 설정
    setsockopt(server_sock, SOL_SOCKET, SO_RCVBUF, &recv_buf_size, sizeof(recv_buf_size));
    // TCP 송신 버퍼 크기 설정
    setsockopt(server_sock, SOL_SOCKET, SO_SNDBUF, &send_buf_size, sizeof(send_buf_size));

    getsockopt(server_sock, SOL_SOCKET, SO_RCVBUF, &actual_recv_buf_size, &optlen);
    getsockopt(server_sock, SOL_SOCKET, SO_SNDBUF, &actual_send_buf_size, &optlen);
    
    printf("recvice buf size is %d bytes\n", actual_recv_buf_size);
    printf("send buf size is %d bytes\n", actual_send_buf_size);
    
    // 서버 소켓 생성 및 설정
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(4433);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_sock, 1);
    printf("TLS Server started and waiting for connections...\n");
    fflush(stdout);

    // 클라이언트 연결 수락 (세션키 수신)
    // tcp 핸드쉑
    int client_sock = accept(server_sock, NULL, NULL);
    if (client_sock < 0) {
        perror("Accept failed");
        ERR_print_errors_fp(stderr);
    } else {
        printf("Client connected!\n");
    }

    // SSL 객체 생성 및 연결 설정 (핸드셰이크)
    SSL *ssl = SSL_new(ctx);    
    SSL_set_fd(ssl, client_sock);
    // tls 핸드쉑 시작 시간 측정
    gettimeofday(&start, NULL);
    if (SSL_accept(ssl) <= 0) {
        printf("SSL accept failed\n");
    } else {
        printf("SSL connection established\n");
    }
    gettimeofday(&end, NULL);
    printf("asdf %ld, %ld\n", end.tv_sec, start.tv_sec);
    long seconds = end.tv_sec - start.tv_sec;
    long microseconds = end.tv_usec - start.tv_usec;
    long elapsed = seconds * 1000000 + microseconds;
    printf("TLS Handshake time: %ld microseconds\n", elapsed);
    
    SSL_write(ssl, "Welcome to Server!!", 17);
    printf("--------------------\n");
    // 메시지 송수신을 위한 루프
    char buffer[16384];
    // char buffer[8192];
    while (1) {
        // 클라이언트의 요청 수신
        memset(buffer, 0, sizeof(buffer));  // 버퍼 초기화
        int bytes_read = SSL_read(ssl, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';  // null terminator 추가
            printf("Client requested: %s\n", buffer);

            // 종료 조건
            if (strcmp(buffer, "exit") == 0) {
                printf("Connection closing...\n");
                break;  // 종료 조건
            }

            // n 바이트의 무작위 데이터 생성
            int n = atoi(buffer); // 클라이언트로부터 받은 문자열을 정수로 변환
            char *random_data = (char *)malloc(n); // 메모리 할당
            if (random_data) {
                for (int i = 0; i < 100; i++) {
                    printf("%dth send\n", i+1);
                    for (int i = 0; i < n; i++) {
                        random_data[i] = rand() % 256; // 0~255의 랜덤 바이트 생성
                    }
                    // 현재 시간 기록
                    gettimeofday(&send_time, NULL);
                    // printf("%ld\n", send_time.tv_sec);
                    // 랜덤 데이터 클라이언트로 전송
                    int bytes_written = 0;
                    do {
                        bytes_written += SSL_write(ssl, random_data + bytes_written, n - bytes_written);
                        // printf("a, %d\n", bytes_written);
                    } while (bytes_written < n);
                    
                    if (bytes_written <= 0) {
                        int ssl_error = SSL_get_error(ssl, bytes_written);
                        printf("Error writing to SSL: %d\n", ssl_error);
                    }
                    free(random_data); // 메모리 해제
                    random_data = (char *)malloc(n); // 메모리 재할당

                    // ACK 수신 확인
                    memset(buffer, 0, sizeof(buffer));  // 버퍼 초기화
                    SSL_read(ssl, buffer, sizeof(buffer));
                    // printf("%s\n", buffer);
                    if (strcmp(buffer, "ACK") == 0) {
                        // memset(buffer, 0, sizeof(buffer));
                        int a = SSL_write(ssl, (char*)&send_time, sizeof(struct timeval));
                        // printf("%ld\n", send_time.tv_sec);
                        // printf("a, %d\n", a);
                    }
                }

            } else {
                SSL_write(ssl, "Error generating random data", 30); // 오류 메시지 전송
            }


        } else {
            int ssl_error = SSL_get_error(ssl, bytes_read);
            printf("Error reading from SSL: %d\n", ssl_error);
            break;
        }
    }
    
    // SSL 종료 및 리소스 해제
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_sock);
    close(server_sock);
    SSL_CTX_free(ctx);
}

// void keylog_callback(const SSL* ssl, const char *line){
//    printf("==============================================\n");
//    printf("%s\n", line);
// }
