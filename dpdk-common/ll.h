#include <stdlib.h>

struct ll {
	struct ll* next;
	void* elt;
};

struct ll* new_ll() {
	struct ll* newll = (struct ll*) malloc(sizeof(struct ll));
	newll->next = NULL;
	newll->elt = NULL;
	return newll;
}

void add_ll(struct ll* ll, void* elt) {
	while (ll->next != NULL) ll = ll->next;
	ll->elt = elt;
	ll->next = new_ll();
}

void* pop_ll(struct ll** ll) {
	void* elt = (*ll)->elt;
	struct ll* ll_old = *ll; 
	*ll = (*ll)->next;
	free(ll_old);
	return elt;
}