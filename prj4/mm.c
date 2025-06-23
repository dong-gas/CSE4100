/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include "mm.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your information in the following struct.
 ********************************************************/
team_t team = {
    /* Your student ID */
    "20211507",
    /* Your full name*/
    "Donggeon Kim",
    /* Your email address */
    "eastgun@sogang.ac.kr",
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

/*
allocated block: [header / payload / footer]
free block:      [header / prev 포인터 / next 포인터 / ... / footer]
header/footer:   0번째비트가 allocate 여부 / 3번째(0 based기준) 비트부터 사이즈 나타냄
*/

// csapp fiqure 9.43 매크로
#define WSIZE 4              // 워드, 헤더, 푸터 사이즈
#define DSIZE 8              // 더블 워드 사이즈
#define SSIZE 3              // 헤더/푸터에서 status 나타내는 비트 범위 [0, 3)
#define MAX_PW 32            // segregated 범위 2^MAX_PW
#define CHUNKSIZE (1 << 12)  // 힙을 이 양만큼 확장 (4096)

#define MAX(x, y) ((x) > (y) ? (x) : y)  // max 함수

#define PACK(size, alloc) ((size << SSIZE) | alloc)  // 사이즈랑 할당된비트를 한 워드로 묶는 거

// 주소 p에 있는 거 읽기, 쓰기
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

// size랑 allocate 정보 가져오기
#define GET_SIZE(p) ((GET(p) & ~0x7) >> SSIZE)  // ~0x7 : 111..111000. 페이로드 사이즈
#define GET_ALLOC(p) (GET(p) & 0x1)             // 0x1: 000...000001

// 헤더 포인터로 푸터 주소 계산 (payload + 헤더 사이즈 더하면 나옴)
#define FTRP(bp) ((char **)(bp) + GET_SIZE(bp) + 1)

#define NEXT_BLKP(bp) (FTRP(bp) + 1)                                     // 다음 블록 헤더 주소
#define PREV_BLKP(bp) ((char **)(bp) - GET_SIZE((char **)(bp) - 1) - 2)  // 앞 블럭 헤더 주소 (2만큼 더 빼야함.)

#define PREV_NODE(bp) (*((char **)(bp + 1)))  // 이전 노드 포인터
#define NEXT_NODE(bp) (*((char **)(bp) + 2))  // 다음 노드 포인터

static void insert_node(char **ptr);
static void remove_node(char **ptr);
static void place(void *ptr, size_t words);
static void *merge(void *ptr);
static void mark_block(char **ptr, size_t size, int alloc);
static int get_index(size_t size);

static char **segregated_free_list;  // 다중 free list 관리용 포인터
int prev_size;                       // realloc 전 사이즈 저장용. 메모리 realloc 할 때마다

// free, alloc 표시 하는 거. size만큼
static void mark_block(char **ptr, size_t size, int alloc) { PUT(ptr, PACK(size, alloc)), PUT(ptr + size + 1, PACK(size, alloc)); }

/*
 * mm_init - initialize the malloc package.
 * 오류: -1, else: 0 리턴
 */
int mm_init(void) {
    prev_size = 0;

    char **bp = mem_sbrk(MAX_PW * sizeof(char *));

    // segregated free list 개수 만큼 할당
    if ((segregated_free_list = bp) == (void *)-1) return -1;

    // NULL로 초기화
    for (int i = 0; i < MAX_PW; i++) segregated_free_list[i] = NULL;

    // alignment 용.
    // 오히려 없어야 할 거 같은데.. 빼면 오류가 남.. ㅠ
    bp = mem_sbrk(WSIZE);
    if (bp == (void *)-1) return -1;

    // heap 할당 4개 (더미 head,foot 각 2개씩)
    bp = mem_sbrk(4 * WSIZE);
    if (bp == (void *)-1) return -1;

    // heap 시작 끝에 각각 size 0인 더미 블록(head/foot이고 사이즈 0인) 둬서 표시
    for (int cnt = 0; cnt < 2; cnt++, bp += DSIZE) mark_block(bp, 0, 1);

    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    if (!size) return NULL;
    size_t rsz = size;

    if (size <= CHUNKSIZE) {
        rsz = 1;
        while (rsz < size) rsz <<= 1;  // 나 이상 첫 2 제곱 수
    }

    // align으로 8바이트 맞추고, 워드로 나누기. 필요한 워드 수
    size_t need = (ALIGN(rsz)) / WSIZE;

    char **bp = NULL;
    int idx = get_index(need);
    while (idx < MAX_PW) {
        char **free_list_node = segregated_free_list[idx++];
        if (free_list_node == NULL || GET_SIZE(free_list_node) < need) continue;                                                      // 비어있거나 젤 큰 것도 안 맞으면 다음 거..
        while (NEXT_NODE(free_list_node) && GET_SIZE(NEXT_NODE(free_list_node)) >= need) free_list_node = NEXT_NODE(free_list_node);  // 해당 리스트 순회 쭉..
        bp = free_list_node, idx = MAX_PW;                                                                                            // break
    }

    if (!bp) {
        size_t cnt = MAX(need, CHUNKSIZE / WSIZE);  // 늘릴 워드 개수
        bp = mem_sbrk((cnt + 2) * WSIZE);           // 헤더 푸터 포함 2개 더
        if (bp == (void *)-1) return NULL;

        // 새로 만든 거 넣고, 맨 끝 더미 밀기
        // 기존 끝 더미 노드 2칸 빼야 함..
        // alloc은 여기서 안 함. 0으로

        // payload
        mark_block(bp - 2, cnt, 0);

        // 아래 2개가 힙 끝 더미
        mark_block(bp + cnt, 0, 1);

        bp = bp - 2;
    }
    else remove_node(bp);  // 찾았으면 그 블럭은 seg list에서 삭제

    // 넣고, 남은 거 다시 할당
    place(bp, need);
    return bp + 1;  // payload라 1 더해서
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) {
    if (!ptr) return;
    ptr -= WSIZE;  // payload 시작 주소에서 헤더 주소로 변경

    size_t pay_load = GET_SIZE(ptr);  // 페이로드 크기 (헤더, 푸터 뺀 거)
    mark_block(ptr, pay_load, 0);

    // 합친 다음에 segregated free list에 free 노드추가
    insert_node(merge(ptr));
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
    if (ptr == NULL) return mm_malloc(size);  // 없으면 malloc
    if (size == 0) {                          // 0 이면 free
        mm_free(ptr);
        return NULL;
    }
    // 여기까지 명세서 언급된 예외 1, 2 처리

    int dif = ((int)(size - prev_size) > 0 ? (int)(size - prev_size) : (int)(prev_size - size));
    prev_size = size;

    int buf_size = ((size + 500) / 1000) * 1000;  // 1000 반올림..
    size_t rdif = 1;
    if (dif <= CHUNKSIZE) {  // 근데 작으면 2제곱수 가깝게,,
        while (rdif < dif) rdif <<= 1;
        if (dif != rdif) buf_size = rdif;
    }

    char **original_ptr = (char **)ptr, **bp = original_ptr - 1;  // 원래 거

    size_t cur = ALIGN(size) / WSIZE;     // 버퍼 뺀 필요한 거
    size_t need = cur + buf_size;         // 필요한 거
    size_t original_size = GET_SIZE(bp);  // 원래 사이즈

    if (original_size >= cur) return ptr;  // 이미 여유 있으면 걍 그거. 굳이 버퍼 ㄴ..

    char **pre_ptr = PREV_BLKP(bp), **suf_ptr = NEXT_BLKP(bp);
    size_t pre_size = GET_SIZE(pre_ptr), suf_size = GET_SIZE(suf_ptr);
    int pre_alloc = GET_ALLOC(pre_ptr), suf_alloc = GET_ALLOC(suf_ptr);

    if (pre_alloc && !suf_alloc && suf_size + original_size + 2 >= need) {  // 뒤에 합치기
        mark_block(bp, original_size, 0);
        bp = merge(bp);
        place(bp, need);
    }
    else if (!pre_alloc && suf_alloc && pre_size + original_size + 2 >= need) {  // 앞에 합치기
        mark_block(bp, original_size, 0);
        bp = merge(bp);
        memmove(bp + 1, original_ptr, original_size * WSIZE);
        place(bp, need);
    }
    else if (!pre_alloc && !suf_alloc && pre_size + suf_size + original_size + 2 >= need) {  // 하나씩은 안되는데, 둘 다 합쳐서 될 때. 헤더, 푸터 한 개로 가능,,
        mark_block(bp, original_size, 0);
        bp = merge(bp);
        memmove(bp + 1, original_ptr, original_size * WSIZE);
        place(bp, need);
    }
    else {  // 새로 할당 받기
        char **new_bp_header = (char **)mm_malloc((need * WSIZE) + WSIZE) - 1;
        if (!new_bp_header) return NULL;
        memcpy(new_bp_header + 1, original_ptr, original_size * WSIZE);
        mm_free(original_ptr);
        return new_bp_header + 1;
    }
    return bp + 1;
}

// 인덱스 찾기용
static int get_index(size_t size) {
    int idx = 0, tmp = 1;
    while ((tmp << 1) <= size) idx++, tmp <<= 1;
    return idx;
}

// free list에 추가하기
static void insert_node(char **ptr) {
    if (!ptr) return;
    size_t size = GET_SIZE(ptr);
    if (size == 0) return;  // 이거 없으면 더미헤더땜에 문제 생김... 하...

    int idx = get_index(size);
    PREV_NODE(ptr) = NULL, NEXT_NODE(ptr) = NULL;  // 앞 뒤 다 NULL로 초기화

    if (!segregated_free_list[idx]) segregated_free_list[idx] = ptr;  // 비어 있으면 바로 넣고 끝.
    else if (size >= GET_SIZE(segregated_free_list[idx])) {           // 맨 앞..
        NEXT_NODE(ptr) = segregated_free_list[idx], PREV_NODE((char **)segregated_free_list[idx]) = ptr;
        segregated_free_list[idx] = ptr;
    }
    else {
        char **pre_ptr = segregated_free_list[idx];
        while (NEXT_NODE(pre_ptr) != NULL && GET_SIZE(NEXT_NODE(pre_ptr)) > size) pre_ptr = NEXT_NODE(pre_ptr);
        char **suf_ptr = NEXT_NODE(pre_ptr);
        NEXT_NODE(pre_ptr) = ptr, PREV_NODE(ptr) = pre_ptr;               // 앞 노드에 내 정보 추가, 내 거에 앞 정보 추가
        if (suf_ptr) NEXT_NODE(ptr) = suf_ptr, PREV_NODE(suf_ptr) = ptr;  // 내 거에 뒤 정보 추가, 뒤에 내 정보 추가
    }
}

// ptr에 payload 할당
// 호출 때 가능하게 잘 해야 함... (assert 참고)
static void place(void *ptr, size_t payload) {
    size_t want_size = payload + 2, original_size = GET_SIZE(ptr) + 2;
    assert(original_size >= want_size);  // 할당 가능한 경우에만 호출했는데, 혹시 모르니 assert로 확인,,

    if (original_size - want_size < 2) mark_block(ptr, original_size - 2, 1);      // 2도 안남으면 free 역할 못함.
    else {                                                                         // 2넘게 남으면 쪼개서 추가.
        mark_block(ptr, payload, 1);                                               // 새로 할 거 1로 마킹
        mark_block((char **)ptr + want_size, original_size - (want_size + 2), 0);  // 남는 거 0으로 마킹
        insert_node((char **)ptr + want_size);                                     // 남는 거 free list에 추가
    }
}

// segregated_free_list에서 ptr이 가리키는 노드 삭제
static void remove_node(char **ptr) {
    if (ptr == NULL) return;

    size_t size = GET_SIZE(ptr);
    if (size == 0) return;  // 이거 없으면 더미헤더땜에 문제 생김... 하...

    char **pre_ptr = PREV_NODE(ptr), **suf_ptr = NEXT_NODE(ptr);

    // 앞
    if (pre_ptr) NEXT_NODE(pre_ptr) = suf_ptr;             // pre의 뒤 노드는 지금의 Suf
    else segregated_free_list[get_index(size)] = suf_ptr;  // 맨앞인 경우 걍 뒤에거를 앞에 붙이기만

    // 뒤
    if (suf_ptr) PREV_NODE(suf_ptr) = pre_ptr;  // suf 노드의 앞 노드는 지금의 pre
}

// free 앞 뒤 보고 merge 하기
static void *merge(void *ptr) {
    char **pre_ptr = PREV_BLKP(ptr), **suf_ptr = NEXT_BLKP(ptr);
    size_t pre_alloc = GET_ALLOC(pre_ptr), suf_alloc = GET_ALLOC(suf_ptr);
    size_t size = GET_SIZE(ptr);

    if (pre_alloc && suf_alloc) return ptr;  // 둘 다 차있으면 못 합침
    if (pre_alloc && !suf_alloc) {           // 뒤랑 합치기
        remove_node(suf_ptr);
        size += GET_SIZE(suf_ptr) + 2;
    }
    else if (!pre_alloc && suf_alloc) {  // 앞이랑 합치기
        remove_node(pre_ptr);
        size += GET_SIZE(pre_ptr) + 2;
        ptr = pre_ptr;
    }
    else {                                           // 앞 뒤로 합치기
        remove_node(pre_ptr), remove_node(suf_ptr);  // 삭제
        size += GET_SIZE(pre_ptr) + GET_SIZE(suf_ptr) + 4;
        ptr = pre_ptr;
    }
    mark_block(ptr, size, 0);
    return ptr;
}
