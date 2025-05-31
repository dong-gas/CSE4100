/*
 * echoserveri.c - An iterative echo server
 */
/* $begin echoserverimain */
#include "csapp.h"

#define MAX_STOCK_NUM 1000

typedef struct stock {
    int id, cnt, price, idx;
    struct stock *left, *right;
} stock;
stock* root;

int client_count = 0;
int stock_cnt = 0;       // 주식 개수
int idx[MAX_STOCK_NUM];  // stock.txt에 저장되어 있는 순서..
// idx[0]가 3이라는 의미: stock.txt 0번째 주식 id가 3이라는 의미..
// 입력된 순서대로 출력하기 위해..

stock* insert_stock(stock* u, stock* item) {  // item을 삽입
    if (!u) return item;
    if (u->id == item->id) u->cnt += item->cnt;  // 이미 있는 경우
    else if (u->id > item->id) u->left = insert_stock(u->left, item);
    else u->right = insert_stock(u->right, item);
    return u;
}

int find_stock_by_id(stock* u, int id, char* buf) {  // 트리에 id가 무조건 있다고 가정
    if (u->id == id) return sprintf(buf, "%d %d %d\n", u->id, u->cnt, u->price);
    else if (u->id > id) return find_stock_by_id(u->left, id, buf);
    else return find_stock_by_id(u->right, id, buf);
}

void show_stock(char* buf) {  // buf에 쓰기, return 값은 쓴 길이
    // 입력된 순서f대로 출력하기 위해..
    for (int i = 0; i < stock_cnt; i++) buf += find_stock_by_id(root, idx[i], buf);
    return;
}

void load_stock() {
    FILE* fp = fopen("stock.txt", "r");
    int id, cnt, price;
    while (fscanf(fp, "%d %d %d", &id, &cnt, &price) != EOF) {
        stock* u = (stock*)malloc(sizeof(stock));
        u->id = id, u->cnt = cnt, u->price = price, u->idx = stock_cnt;
        idx[stock_cnt++] = id;
        u->left = u->right = NULL;
        root = insert_stock(root, u);
    }
    fclose(fp);
    return;
}

void save_stock() {
    FILE* fp = fopen("stock.txt", "w");

    // buf에 저장 후, stock.txt에 write
    char buf[MAXLINE] = {'\0'};
    show_stock(buf), fprintf(fp, "%s", buf);
    fclose(fp);
    return;
}

// stock.txt는 항상 사고 팔 주식이 있는 것으로 가정하고 과제를 진행하시면 됩니다.
int update_stock(stock* u, int id, int cnt) {
    if (u->id == id) {
        if (u->cnt + cnt < 0) return 0;
        u->cnt += cnt;
        return 1;
    }
    else if (u->id > id) return update_stock(u->left, id, cnt);
    else return update_stock(u->right, id, cnt);
}

// pool code: 강의자료/교과서 코드 사용
typedef struct {
    int maxfd;  // 가장 큰 fd 번호
    fd_set read_set, ready_set;
    int nready;  // ready set에서 1 개수
    int maxi;
    int clientfd[FD_SETSIZE];
    rio_t clientrio[FD_SETSIZE];  // set of read buffers
} pool;

int byte_cnt = 0;  // 서버가 받은 총 바이트 수

void init_pool(int listenfd, pool* p) {
    // 첨에 아무 연결 없게 초기화.
    p->maxi = -1;
    memset(p->clientfd, -1, sizeof(p->clientfd));

    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);           // 다 0으로하고,
    FD_SET(listenfd, &p->read_set);  // listenfd만 있으니까 그것만 1로
}

void add_client(int connfd, pool* p) {
    int full = 1;
    p->nready--;                            // 처리할 거니까 1줄이기
    for (int i = 0; i < FD_SETSIZE; i++) {  // 빈 거 찾기
        if (p->clientfd[i] < 0) {           // 비어 있으면
            p->clientfd[i] = connfd;
            Rio_readinitb(&p->clientrio[i], connfd);  // buffer 초기화

            FD_SET(connfd, &p->read_set);  // connfd를 추가

            if (connfd > p->maxfd) p->maxfd = connfd;
            if (i > p->maxi) p->maxi = i;
            full = 0;
            client_count++;
            break;
        }
    }
    if (full) app_error("add_client error: Too many clients");
}

void check_clients(pool* p) {
    char buf[MAXLINE];
    for (int i = 0; i <= p->maxi && p->nready > 0; i++) {  // 준비된 거 모두 한 줄 씩 처리..
        int connfd = p->clientfd[i];
        rio_t rio = p->clientrio[i];

        if (connfd > 0 && FD_ISSET(connfd, &p->ready_set)) {
            p->nready--;

            int n = Rio_readlineb(&rio, buf, MAXLINE);  // 각각 한 줄만
            if (n != 0) {
                byte_cnt += n;
                printf("server received %d bytes\n", n);

                char tmp[MAXLINE], ret[MAXLINE] = {'\0'};
                int ID, cnt;

                sscanf(buf, "%s", tmp);  // buf에 있는 거 tmp(임시)로 %s만큼 읽기
                if (!strcmp(tmp, "show")) show_stock(ret), Rio_writen(connfd, ret, MAXLINE);
                else if (!strcmp(tmp, "buy")) {
                    sscanf(buf + 3, "%d %d", &ID, &cnt);
                    strcpy(ret, (update_stock(root, ID, -cnt) ? "[buy] success\n" : "Not enough left stocks\n"));
                    Rio_writen(connfd, ret, MAXLINE);
                }
                else if (!strcmp(tmp, "sell")) {
                    sscanf(buf + 4, "%d %d", &ID, &cnt);
                    update_stock(root, ID, cnt);
                    strcpy(ret, "[sell] success\n");
                    Rio_writen(connfd, ret, MAXLINE);                    
                }
                else if (!strcmp(tmp, "exit")) {
                    Close(connfd);
                    FD_CLR(connfd, &p->read_set);
                    p->clientfd[i] = -1;
                    if (--client_count == 0) save_stock();
                }
            }
            else {
                Close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;

                if (--client_count == 0) save_stock();
            }
        }
    }
}

void echo(int connfd);
void sigint_handler(int sig) {
    save_stock();
    exit(0);
    return;
}

int main(int argc, char** argv) {
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr; /* Enough space for any address */  // line:netp:echoserveri:sockaddrstorage
    static pool pool;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    Signal(SIGINT, sigint_handler);  // sigint 등록
    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);
    load_stock();

    while (1) {
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd + 1, &pool.ready_set, NULL, NULL, NULL);
        if (FD_ISSET(listenfd, &pool.ready_set)) {
            clientlen = sizeof(struct sockaddr_storage);
            connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
            add_client(connfd, &pool);
        }
        check_clients(&pool);
    }

    exit(0);
}
/* $end echoserverimain */