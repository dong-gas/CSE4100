#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bitmap.h"
#include "debug.h"
#include "hash.h"
#include "hex_dump.h"
#include "limits.h"
#include "list.h"
#include "round.h"

#define CMD_MAX 20
#define NAME_LEN_MAX 20
#define MAX_CNT 100
// 명세서: The number of list, hash table and bitmap is less than or equal to 10.
// 삭제 후, 더 만들 수도 있으니 여유있게 100으로 잡음.

struct list List[MAX_CNT];
struct hash Hash[MAX_CNT];
// struct bitmap Bitmap[MAX_CNT];

char type[MAX_CNT][NAME_LEN_MAX];
char name[MAX_CNT][NAME_LEN_MAX];
int idx;                // 작업하는 인덱스
int structure_cnt = 0;  // 현재까지 자료구조 개수 (list, bitmap, hash 다 합쳐서)

int get_idx() {
    char structure_name[NAME_LEN_MAX];
    scanf("%s", structure_name);
    for (int i = 0; i < structure_cnt; i++) {
        if (!strcmp(name[i], structure_name)) return i;
    }
    // ASSERT(1); // 이런 경우 없어야 하긴 함
    return -1;
}

void Create() {
    scanf("%s %s", type[structure_cnt], name[structure_cnt]);
    if (!strcmp(type[structure_cnt], "list")) list_init(&List[structure_cnt]);
    else if(!strcmp(type[structure_cnt], "hashtable")) hash_init(&Hash[structure_cnt], hashing_function, hash_less, NULL);
    // else if(!strcmp(type[structure_cnt], "bitmap")) list_init(&List[sz]);
    structure_cnt++;
}

void Delete() {
    for (int i = 0; i < NAME_LEN_MAX; i++) name[idx][i] = '!';  // 쓰레기 값으로 바꾸기

    // List, hash만 구현
    if (!strcmp(type[idx], "list")) {
        size_t sz = list_size(&List[idx]);
        while (sz--) {
            struct list_item* item = list_entry(list_front(&List[idx]), struct list_item, elem);
            list_pop_front(&List[idx]);
            free(item);
        }
    }
    else if (!strcmp(type[idx], "hashtable")) hash_destroy(&Hash[idx], Hash_Destructor);
}

void Dumpdata() {
    // List, hash만 구현
    if (!strcmp(type[idx], "list")) {
        for (struct list_elem* i = list_begin(&List[idx]); i != list_end(&List[idx]); i = list_next(i)) {
            printf("%d ", list_entry(i, struct list_item, elem)->data);
        }
        printf("\n");
    }
    else if (!strcmp(type[idx], "hashtable")) Hash_Print(&Hash[idx]);
}

int main(void) {
    while (1) {
        char cmd[CMD_MAX];
        scanf("%s", cmd);
        if (!strcmp(cmd, "quit")) break;
        else if (!strcmp(cmd, "create")) Create();
        else {
            idx = get_idx();

            if (!strcmp(cmd, "delete")) Delete();
            else if (!strcmp(cmd, "dumpdata")) Dumpdata();

            // ------------- List Start -------------
            /* List Insertion */
            else if (!strcmp(cmd, "list_insert")) List_Insert(&List[idx]);
            else if (!strcmp(cmd, "list_splice")) {
                int insert_idx, l, r;
                scanf("%d", &insert_idx);
                int idx2 = get_idx();
                scanf("%d %d", &l, &r);
                List_Splice(&List[idx], &List[idx2], l, r, insert_idx);
            }
            else if (!strcmp(cmd, "list_push_front")) List_Push_Front(&List[idx]);
            else if (!strcmp(cmd, "list_push_back")) List_Push_Back(&List[idx]);

            // /* List Removal */
            else if (!strcmp(cmd, "list_remove")) List_Remove(&List[idx]);
            else if (!strcmp(cmd, "list_pop_front")) List_Pop_Front(&List[idx]);
            else if (!strcmp(cmd, "list_pop_back")) List_Pop_Back(&List[idx]);

            // /* List Elements */
            else if (!strcmp(cmd, "list_front")) List_Front(&List[idx]);
            else if (!strcmp(cmd, "list_back")) List_Back(&List[idx]);

            // /* List Properties */
            else if (!strcmp(cmd, "list_size")) List_Size(&List[idx]);
            else if (!strcmp(cmd, "list_empty")) List_Empty(&List[idx]);

            // /* Miscellaneous */
            else if (!strcmp(cmd, "list_reverse")) List_Reverse(&List[idx]);
            else if (!strcmp(cmd, "list_sort")) List_Sort(&List[idx]);
            else if (!strcmp(cmd, "list_insert_ordered")) List_Insert_Ordered(&List[idx]);
            else if (!strcmp(cmd, "list_unique")) {
                struct list* duplicates = NULL;
                if (getchar() != '\n') duplicates = &List[get_idx()];
                List_Unique(&List[idx], duplicates);
            }
            // /* Max And Min */
            else if (!strcmp(cmd, "list_max")) List_Max(&List[idx]);
            else if (!strcmp(cmd, "list_min")) List_Min(&List[idx]);
            else if (!strcmp(cmd, "list_swap")) List_Swap(&List[idx]);
            else if (!strcmp(cmd, "list_shuffle")) List_Shuffle(&List[idx]);  // -> 랜덤 + Swap
            // ------------- List End -------------

            // ------------- Hash Start -------------
            /* Basic Life Cycle */
            else if (!strcmp(cmd, "hash_clear")) Hash_Clear(&Hash[idx]);

            /* Search, Insertion, Deletion */
            else if (!strcmp(cmd, "hash_insert")) Hash_Insert(&Hash[idx]);
            else if (!strcmp(cmd, "hash_replace")) Hash_Replace(&Hash[idx]);
            else if (!strcmp(cmd, "hash_find")) Hash_Find(&Hash[idx]);
            else if (!strcmp(cmd, "hash_delete")) Hash_Delete(&Hash[idx]);

            /* Iteration. */
            else if (!strcmp(cmd, "hash_apply")) Hash_Apply(&Hash[idx]);

            /* Information. */
            else if (!strcmp(cmd, "hash_size")) Hash_Size(&Hash[idx]);
            else if (!strcmp(cmd, "hash_empty")) Hash_Empty(&Hash[idx]);
            // ------------- Hash End -------------


            else break;
        }
    }
}
