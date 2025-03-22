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
struct bitmap Bitmap[MAX_CNT];

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
    // 새로 자료구조 생성하는 함수
    scanf("%s %s", type[structure_cnt], name[structure_cnt]);
    if (!strcmp(type[structure_cnt], "list")) list_init(&List[structure_cnt]);
    else if(!strcmp(type[structure_cnt], "hashtable")) hash_init(&Hash[structure_cnt], hashing_function, hash_less, NULL);
    else if(!strcmp(type[structure_cnt], "bitmap")) {
        int sz;
        scanf("%d", &sz);
        Bitmap[structure_cnt] = *bitmap_create(sz);
    }
    structure_cnt++;
}

void Delete() {
    // 삭제하는 함수
    for (int i = 0; i < NAME_LEN_MAX; i++) name[idx][i] = '!';  // 이름 쓰레기 값으로 바꾸기

    if (!strcmp(type[idx], "list")) {
        size_t sz = list_size(&List[idx]);
        while (sz--) {
            struct list_item* item = list_entry(list_front(&List[idx]), struct list_item, elem);
            list_pop_front(&List[idx]);
            free(item);
        }
    }
    else if (!strcmp(type[idx], "hashtable")) hash_destroy(&Hash[idx], Hash_Destructor);
    else if (!strcmp(type[idx], "bitmap")) free(Bitmap[idx].bits);
    return;
}

void Dumpdata() {
    // 자료구조 내 데이터 출력하는 함수
    if (!strcmp(type[idx], "list")) List_Print(&List[idx]);
    else if (!strcmp(type[idx], "hashtable")) Hash_Print(&Hash[idx]);
    else if (!strcmp(type[idx], "bitmap")) Bitmap_Print(&Bitmap[idx]);
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


            // ------------- Bitmap Start -------------
            /* Bitmap size. */
            else if (!strcmp(cmd, "bitmap_size")) Bitmap_Size(&Bitmap[idx]);

            /* Setting and testing single bits. */
            else if (!strcmp(cmd, "bitmap_set")) Bitmap_Set(&Bitmap[idx]);
            else if (!strcmp(cmd, "bitmap_mark")) Bitmap_Mark(&Bitmap[idx]);
            else if (!strcmp(cmd, "bitmap_reset")) Bitmap_Reset(&Bitmap[idx]);
            else if (!strcmp(cmd, "bitmap_flip")) Bitmap_Flip(&Bitmap[idx]);
            else if (!strcmp(cmd, "bitmap_test")) Bitmap_Test(&Bitmap[idx]);

            /* Setting and testing multiple bits. */
            else if (!strcmp(cmd, "bitmap_set_all")) Bitmap_Set_All(&Bitmap[idx]);
            else if (!strcmp(cmd, "bitmap_set_multiple")) Bitmap_Set_Multiple(&Bitmap[idx]);
            else if (!strcmp(cmd, "bitmap_count")) Bitmap_Count(&Bitmap[idx]);
            else if (!strcmp(cmd, "bitmap_contains")) Bitmap_Contains(&Bitmap[idx]);
            else if (!strcmp(cmd, "bitmap_any")) Bitmap_Any(&Bitmap[idx]);
            else if (!strcmp(cmd, "bitmap_none")) Bitmap_None(&Bitmap[idx]);
            else if (!strcmp(cmd, "bitmap_all")) Bitmap_All(&Bitmap[idx]);

            /* Finding set or unset bits. */
            else if (!strcmp(cmd, "bitmap_scan")) Bitmap_Scan(&Bitmap[idx]);
            else if (!strcmp(cmd, "bitmap_scan_and_flip")) Bitmap_Scan_And_Flip(&Bitmap[idx]);
            
            /* Debugging. */
            else if (!strcmp(cmd, "bitmap_dump")) Bitmap_Dump(&Bitmap[idx]);

            /* Other */
            else if (!strcmp(cmd, "bitmap_expand")) Bitmap[idx] = *Bitmap_Expand(&Bitmap[idx]);
            // ------------- Bitmap End -------------


            
        }
    }
}
