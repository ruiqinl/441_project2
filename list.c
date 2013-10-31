#include <stdlib.h>
#include <assert.h>
#include "debug.h"
#include "list.h"


struct list_t *enlist(struct list_t *list, void *data) {
    struct list_item_t *t = NULL;

    t = (struct list_item_t *)calloc(1, sizeof(struct list_item_t));
    if (t == NULL) {
	DPRINTF(DEBUG_LIST, "Warning! enlist, calloc return null\n");
	return NULL;
    }
    
    t->data = data;
    
    if (list->end == NULL) {
	list->end = t;
	list->head = list->end;
	list->length = 1;
    } else {
	list->end->next = t;
	list->end = t;
	list->length++;
    }
    return list;
}

void *delist(struct list_t *list) {
    void *data = NULL;
    
    if (list->length == 0) {
	DPRINTF(DEBUG_LIST, "Warning! delist a null list\n");
	return NULL;
    }
    
    data = list->head->data;

    list->head = list->head->next;
    list->length -= 1;

    return data;
}

struct list_t *init_list(struct list_t **list) {
    *list = (struct list_t *)calloc(1, sizeof(struct list_t));
    
    if (*list == NULL) {
	DPRINTF(DEBUG_LIST, "Warning! init_list, calloc returns null\n");
	return NULL;
    }

    (*list)->head = NULL;
    (*list)->end = NULL;
    (*list)->length = 0;

    return *list;
}

struct list_t *cat_list(struct list_t *p, struct list_t *q) {

    assert(p != NULL);
    assert(q != NULL);

    p->end->next = q->head;
    p->end = q->end;

    p->length += q->length;
    
    return p;
}

void dump_list(struct list_t *list, void(*printer)(void *data), char *delim) {
    struct list_item_t *item = NULL;
    
    if (list->length == 0 || list->head == NULL) 
	printf("NULL list");

    item = list->head;
    while ( item != NULL) {
	printer(item->data);
	printf("%s", delim);
	item = item->next;
    }
    printf("\n");
}

struct list_item_t *get_iterator(struct list_t *list) {
    if (list == NULL)
	return NULL;
    return list->head;
}

int has_next(struct list_item_t *iterator) {
    assert(iterator != NULL);
    
    if (iterator->next == NULL)
	return 0;
    return 1;
}

void *next(struct list_item_t **iterator) {
    assert(iterator != NULL);
    assert(*iterator != NULL);

    *iterator = (*iterator)->next;

    assert(*iterator != NULL);
    return (*iterator)->data;
}


#ifdef _TEST_LIST_

void int_printer(void *data) {
    printf("%d", *(int*)data);
}

int main(){
    int i,j,k;
    struct list_t *list = NULL;
    
    i = 42;
    j = 0;
    k = -1;

    init_list(&list);
    dump_list(list, int_printer, " ");

    enlist(list, &i);
    dump_list(list, int_printer, " ");
    enlist(list, &j);
    dump_list(list, int_printer, " ");
    enlist(list, &k);
    dump_list(list, int_printer, " ");

    delist(list);
    dump_list(list, int_printer, " ");
    delist(list);
    dump_list(list, int_printer, " ");
    delist(list);
    dump_list(list, int_printer, " ");

    printf("list->length:%d\n", list->length);
    delist(list);
    
}

#endif
