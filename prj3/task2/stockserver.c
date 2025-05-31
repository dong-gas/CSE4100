/*
 * echoserveri.c - An iterative echo server
 */
/* $begin echoserverimain */
#include "csapp.h"

#define MAX_STOCK_NUM 1000
#define SBUFSIZE 200
#define NTHREADS 200

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

void echo(int connfd);
void sigint_handler(int sig) {
    save_stock(), exit(0);
    return;
}

// ---------------------------------------
// sbuf (강의자료 참고)
typedef struct {
    int* buf;    //buffer array
    int n;       // 최대 슬롯
    int front;   // buf[(front + 1) % n]: first
    int rear;    // buf[rear % n] : last
    sem_t mutex; // buf 접근 protect
    sem_t slots; // 슬롯
    sem_t items; // 아이템
} sbuf_t;
sbuf_t sbuf;

int byte_cnt = 0;   // byte counter
int client_cnt = 0; // client counter
static sem_t mutex; // byte_cnt, client_cnt mutex

void sbuf_init(sbuf_t * sp, int n) { // 초기화
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n;
    sp->front = sp->rear = 0;
    Sem_init(&sp->mutex, 0, 1); // 바이너리 세마포어 (lock용)
    Sem_init(&sp->slots, 0, n); // n개의 빈 슬롯
    Sem_init(&sp->items, 0, 0); // 첨에 0개
}

void sbuf_deinit(sbuf_t *sp) {
    Free(sp->buf);
    return;
}

void sbuf_insert(sbuf_t *sp, int item) { // subf 맨 뒤에 item 삽입
    P(&sp->slots);
    P(&sp->mutex);
    sp->buf[(++sp->rear) % (sp->n)] = item;
    V(&sp->mutex);
    V(&sp->items);
}

int sbuf_remove(sbuf_t *sp) { // 첫 원소 삭제 후 리턴
    int item;
    P(&sp->items);
    P(&sp->mutex);
    item = sp->buf[(++sp->front) % (sp->n)];
    V(&sp->mutex);
    V(&sp->slots);
    return item;
}

static void init_echo_cnt(void) {
    Sem_init(&mutex, 0, 1);
    byte_cnt = 0;
}

void echo_cnt(int connfd) {
    int n;
    char buf[MAXLINE];
    rio_t rio;
    static pthread_once_t once = PTHREAD_ONCE_INIT;

    Pthread_once(&once, init_echo_cnt);
    Rio_readinitb(&rio, connfd);

    // client count
    P(&mutex);
    client_count++;
    V(&mutex); 

    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        P(&mutex);     // byte_cnt용 mutex
        byte_cnt += n; 
        printf("server received %d bytes\n", n);

        char tmp[MAXLINE], ret[MAXLINE] = {'\0'};
        int ID, cnt, exit = 0;
        
        sscanf(buf, "%s", tmp); // buf에 있는 거 tmp로 읽기
        if (!strcmp(tmp, "show")) show_stock(ret), Rio_writen(connfd, ret, MAXLINE);
        else if(!strcmp(tmp, "buy")) {
            sscanf(buf + 3, "%d %d", &ID, &cnt);
            strcpy(ret, (update_stock(root, ID, -cnt)? "[buy] success\n" : "Not enough left stocks\n"));
            Rio_writen(connfd, ret, MAXLINE);
        }
        else if (!strcmp(tmp, "sell")) {
            sscanf(buf + 4, "%d %d", &ID, &cnt);
            update_stock(root, ID, cnt);
            strcpy(ret, "[sell] success\n");
            Rio_writen(connfd, ret, MAXLINE);
        }
        else if (!strcmp(tmp, "exit")) exit = 1;
        
        V(&mutex);
        if (exit) break;
        // Rio_writen(connfd, buf, n);
    }
    
    P(&mutex);
    if (--client_count == 0) save_stock();
    V(&mutex); 
}

void *thread(void *vargp) { // thread
    Pthread_detach(pthread_self());
    while (1) {
        int connfd = sbuf_remove(&sbuf); // buf에서 connfd 삭제
        echo_cnt(connfd); // service client..
        Close(connfd);
    }
}

int main(int argc, char** argv) {
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr; /* Enough space for any address */  // line:netp:echoserveri:sockaddrstorage
    pthread_t tid;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    Signal(SIGINT, sigint_handler);  // sigint 등록

    listenfd = Open_listenfd(argv[1]);
    load_stock();

    // 강의자료 참고
    sbuf_init(&sbuf, SBUFSIZE);
    for (int i = 0; i < NTHREADS; i++) Pthread_create(&tid, NULL, thread, NULL);


    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        sbuf_insert(&sbuf, connfd);
    }

    exit(0);
}
/* $end echoserverimain */