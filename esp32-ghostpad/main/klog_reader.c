#include "klog_reader.h"
#include "esp_log.h"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static const char *TAG = "ghostpad_klog";

static int serve_file_while_connected(const char *path, int server_fd) {
    fd_set output_set;
    fd_set input_set;
    fd_set temp_set;
    int client_fd;
    char buf[255];
    ssize_t len;
    int file_fd;
    int err = 0;

    if ((file_fd = open(path, O_RDONLY)) < 0) {
        ESP_LOGE(TAG, "open(%s): %s", path, strerror(errno));
        return -1;
    }

    FD_ZERO(&input_set);
    FD_ZERO(&output_set);
    FD_SET(server_fd, &input_set);
    FD_SET(file_fd, &input_set);

    size_t nb_connections = 0;

    do {
        struct timeval timeout = { .tv_sec = 0, .tv_usec = 10000 };
        temp_set = input_set;

        int sel = select(FD_SETSIZE, &temp_set, NULL, NULL, &timeout);
        if (sel < 0) {
            ESP_LOGE(TAG, "select: %s", strerror(errno));
            close(file_fd);
            return -1;
        }
        if (sel == 0) continue;

        if (FD_ISSET(server_fd, &temp_set)) {
            if ((client_fd = accept(server_fd, NULL, NULL)) < 0) {
                ESP_LOGE(TAG, "accept: %s", strerror(errno));
                err = -1;
                break;
            }
            FD_SET(client_fd, &output_set);
            nb_connections++;
        }

        if (FD_ISSET(file_fd, &temp_set)) {
            if ((len = read(file_fd, buf, sizeof(buf))) < 1) {
                ESP_LOGE(TAG, "read: %s", strerror(errno));
                err = -1;
                break;
            }

            for (client_fd = 0; client_fd < FD_SETSIZE; client_fd++) {
                if (FD_ISSET(client_fd, &output_set)) {
                    if (write(client_fd, buf, len) != len) {
                        FD_CLR(client_fd, &output_set);
                        close(client_fd);
                        nb_connections--;
                    }
                }
            }
        }
    } while (nb_connections > 0);

    for (client_fd = 0; client_fd < FD_SETSIZE; client_fd++) {
        if (FD_ISSET(client_fd, &output_set)) {
            FD_CLR(client_fd, &output_set);
            close(client_fd);
        }
    }
    close(file_fd);

    return err;
}

int klog_reader_start(const char *path, uint16_t port) {
    struct sockaddr_in sin;
    int sockfd;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        ESP_LOGE(TAG, "socket: %s", strerror(errno));
        return -1;
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        ESP_LOGE(TAG, "bind port %d: %s", port, strerror(errno));
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, 5) < 0) {
        ESP_LOGE(TAG, "listen: %s", strerror(errno));
        close(sockfd);
        return -1;
    }

    ESP_LOGI(TAG, "Klog server listening on port %d", port);

    while (1) {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(sockfd, &set);

        if (select(sockfd + 1, &set, NULL, NULL, NULL) < 0) {
            ESP_LOGE(TAG, "select: %s", strerror(errno));
            close(sockfd);
            return -1;
        }

        if (FD_ISSET(sockfd, &set)) {
            if (serve_file_while_connected(path, sockfd) < 0) {
                close(sockfd);
                return -1;
            }
        }
    }

    return 0;
}
