#ifndef __ARRAY2D_H__ 
#define __ARRAY2D_H__

#include <stdio.h>
#include <string.h>

/* ARRAY_DEF will be used BEFORE declaring the struct.
   For example,

	ARRAY_DEF(int);
	
	struct myStruct{
		int myField;	
		ARRAY_TYPE(int) myArray;
	}
*/
#define ARRAY_DEF(type) 			\
struct array {    					\
	int width;         				\
	int height;						\
	type empty_value;				\
	type elems[0];					\
};																			

/* ARRAY_TYPE should be used when actually declaring a field
   of a struct that should be an array (see above example) */
#define ARRAY_TYPE(type) struct array*

/* ARRAY_INIT should be used when you want to init the array.
   For example, 

		myStruct_t myStruct_init(){
			myStruct_t myStruct = malloc(sizeof(struct myStruct));
			myStruct->myField = 0;
			ARRAY_INIT(myStruct->array, int, 4, 5, -1);
		}
*/
#define ARRAY_INIT(tobeset, type, h, w, mt_value)		\
(tobeset) = (struct array*)malloc(sizeof(struct array)+	\
						  sizeof(type)*w*h);			\
(tobeset)->width = w;									\
(tobeset)->height = h;									\
(tobeset)->empty_value = mt_value;						\
do{														\
	int i, j;											\
	for(i=0;i<h;i++){ for(j=0;j<w;j++){					\
		(tobeset)->elems[i*w+j] = mt_value;				\
	}}													\
}														\
while(0);

#define ARRAY_ERROR(msg) printf("Error: %s\n", msg);    \

/* ARRAY_GET and ARRAY_PUT are used like normal getters/setters */
#define ARRAY_GET(ar, i, j)															\
(( i < 0 || i >= (ar)->height || 													\
   j < 0 || j >= (ar)->width ) ? (ar)->empty_value : (ar)->elems[j*(ar)->width + i])

#define ARRAY_PUT(ar, i, j, elem)								\
do{																\
	if( i < 0 || i >= (ar)->height ||							\
		j < 0 || j >= (ar)->width ){							\
		ARRAY_ERROR("Out of bounds");							\
	}															\
	else{														\
		(ar)->elems[j*(ar)->width + i] = elem;					\
	}															\
}																\
while(0)

/* Pass in a pointer to the array in order to destroy it */
#define ARRAY_DESTROY(ar_ptr) 	\
do{								\
	free(*(ar_ptr));				\
	*(ar_ptr) = NULL;				\
}								\
while(0);				

#define ARRAY_DESTROY_TOTAL(ar_ptr, destructor) 			 \
do{															 \
	int i;													 \
	for(i=0;i<((*(ar_ptr))->height)*((*(ar_ptr))->width);i++)\
		destructor(&((*(ar_ptr))->elems[i]));				 \
	free(*(ar_ptr));										 \
	*(ar_ptr) = NULL;										 \
}															 \
while(0);


#endif // __ARRAY2D_H__
