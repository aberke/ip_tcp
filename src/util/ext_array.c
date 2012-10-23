#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "ext_array.h"

#define MINIMUM_RATIO .25
/* MINIMUM_RATIO has to be less than 1/SCALE_FACTOR */

struct ext_array{
	int capacity;
	int left;
	int right;
	void* data;
};

void _scale_up(ext_array_t ar);
void _scale_down(ext_array_t ar);
void _shift(ext_array_t ar);

ext_array_t ext_array_init(int capacity){
	ext_array_t ext_array = (struct ext_array*)malloc(sizeof(struct ext_array));
	ext_array->capacity = capacity;
	ext_array->data = malloc(capacity);

	ext_array->left = ext_array->right = 0;

	return ext_array;
}

/* FREES ITS DATA!!!! */
void ext_array_destroy(ext_array_t* ext_array){
	free((*ext_array)->data);

	free(*(ext_array));
	*ext_array = NULL;
}

void ext_array_put(ext_array_t ext_array, void* data, int length){
	/* if there's no room at the end, but that's due to all the garbage at the beginning, 
	   then get rid of all the garbage */
	if(ext_array->capacity - ext_array->right < length 
		&& (ext_array->capacity - (ext_array->right - ext_array->left) > length)){
		/* shift everything over */
		_shift(ext_array);
	
		memcpy(ext_array->data+ext_array->right, data, length);
		ext_array->right += length;
	}
	else{
		/* otherwise it looks like you need to start scaling up */
		while(ext_array->capacity - ext_array->right < length)	_scale_up(ext_array);

		memcpy(ext_array->data+ext_array->right, data, length);
		ext_array->right += length;
	}
}

/* peels data from the beginning of the array */
memchunk_t ext_array_peel(ext_array_t ext_array, int length){
	int ret_length = length < ext_array->right - ext_array->left ? length : ext_array->right - ext_array->left;
	if(ret_length == 0) 
		return NULL;

	void* data = malloc(ret_length);
	memcpy(data, ext_array->data, ret_length);
	
	/* now peel */
	ext_array->left += ret_length;

	return memchunk_init(data, ret_length);
}

/************************ INTERNAL ***********************/

/* just multiply the capacity by scale factor, init a pointer to
   a chunk of memory of the desired size, and then copy over the current
   data to its new home */
void _scale_up(ext_array_t ar){
	int new_capacity = ar->capacity*SCALE_FACTOR;
	ar->capacity = new_capacity;

	void* new_data = malloc(ar->capacity);
	int new_left = 0, new_right = ar->right - ar->left;
	memcpy(new_data, ar->data, new_right);

	free(ar->data);
	ar->data = new_data;
	ar->left = new_left;
	ar->right = new_right;
}

/* NO CHECKS! Don't scale down if its going to do something bad */
void _scale_down(ext_array_t ar){
	/* the new capacity will be scaled down by SCALE_FACTOR */
	int new_capacity = ar->capacity/SCALE_FACTOR;
	ar->capacity = new_capacity;

	/* now init a pointer to the amount of memory dictated by the new 
	   capacity, and copy all the data over */
	void* new_data = malloc(ar->capacity);
	int new_left = 0, new_right = ar->right - ar->left;
	memcpy(new_data, ar->data, new_right);

	/* free the old data, and point the array to the new one */
	free(ar->data);
	ar->data = new_data;
	ar->left = new_left;
	ar->right = new_right;
}

void _shift(ext_array_t ar){
	int data_size = ar->right - ar->left;

	/* if the ratio of data to capacity is too small, then shrink */
	if(data_size/(float)ar->capacity < MINIMUM_RATIO) ar->capacity /= SCALE_FACTOR;

	void* new_data = malloc(ar->capacity);
	memcpy(new_data, ar->data, data_size);
	
	/* free the old data */
	free(ar->data);

	/* all is right in the world again */
	ar->data = new_data;
	ar->left = 0;
	ar->right = data_size;
}



