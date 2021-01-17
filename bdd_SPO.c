/*
 *
 * Implemented by Masoud (Pouya) Salehpour based on BuDDy Package
 * For more information about the licence, see the package information
 * Original  link: www.itu.dk/research/buddy/ (not available; Oct 2020)
 * Available link: http://vlsicad.eecs.umich.edu/BK/Slots/cache/www.itu.dk/research/buddy/ (Oct 2020)
 *
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include <stdbool.h>
#include <bdd.h>
#include <time.h>       // for clock_t, clock(), CLOCKS_PER_SEC
//*************************************************
//can be taken to be inserted in a .h file as well

#define N_DIGITS_P            2 //placeholder for predicates
#define st_size 2500


#define bit_subject_s     0    
#define bit_predicate_s   21  //14//32   //14 //9
#define bit_object_s      28 //20//64   //20 //15


#define bit_subject_e     20 //31//13 //8  
#define bit_predicate_e   27 //63//19 //14
#define bit_object_e      49 //127 //24

bdd kg;
static const int varnum = 50; //128; // 35; //25; //limited BSBM
/*
 *
 * The K/V store is implemented by Masoud (Pouya) Salehpour, based on: https://codereview.stackexchange.com/questions/63493/simple-key-value-store-in-c-take-2
 *Please visit the original webpage for further details
 *
 *
 */


#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <string.h>

#define KVS_SPACE_INCREMENT 8

typedef int KVScompare(const char *a, const char *b);

typedef const void KVSkey;

typedef void KVSvalue;

typedef struct {
    KVSkey *key;
    KVSvalue *value;
} KVSpair;

typedef struct KVSstore KVSstore;

/** Create a new key-value store.

    @param compare
        A function to compare keys. If the store will only contain string keys,
        use strcmp, or use NULL for the default behavior of comparing memory
        addresses, or use a custom function matching the signature of strcmp.

    @return
        A pointer to the store.
*/
KVSstore *kvs_create(KVScompare *compare);

/** Destroy a key-value store.

    @param store
        A pointer to the store.
*/
void kvs_destroy(KVSstore *store);

/** Store a value.

    @param store
        A pointer to the store.

    @param key
        A key used to retrieve the value later. If the key already exists, the
        new value will be stored in place of the old one, unless value is NULL
        in which case the key-value pair will be removed from the store.

    @param value
        A pointer to the data being stored, or NULL to remove an existing value. 
*/
void kvs_put(KVSstore *store, KVSkey *key, KVSvalue *value);

/** Retrieve a value.

    @param store
        A pointer to the store.

    @param key
        A key used to retrieve the value.

    @return
        A pointer to the retrieved value, or NULL if not found.
*/
KVSvalue *kvs_get(KVSstore *store, KVSkey *key);

/** Remove a value from the store.

    @param store
        A pointer to the store.

    @param key
        A key identifying the value to be removed.
*/
void kvs_remove(KVSstore *store, KVSkey *key);

/** Get the number of values in a store.

    @param store
        A pointer to the store.

    @return
        The number of values contained in the store.
*/
size_t kvs_length(KVSstore *store);

/** Get a key-value pair at a given index.

    @param store
        A pointer to the store.

    @param index
        The index of the key-value pair.

    @return
        A pointer to the key-value pair, or NULL if the index is out or range.
*/
KVSpair *kvs_pair(KVSstore *store, size_t index);

#ifdef __cplusplus
}
#endif












struct KVSstore {
    KVSpair *pairs;
    KVScompare *compare;
    size_t length;
    size_t space;
};

static const size_t kvs_pair_size = sizeof(KVSpair);

static const size_t kvs_store_size = sizeof(KVSstore);

static KVSpair *kvs_search(KVSstore *store, KVSkey *key, int exact) {
    KVSpair *pairs = store->pairs;
    size_t lbound = 0;
    size_t rbound = store->length;
    while (lbound < rbound) {
        size_t index = lbound + ((rbound - lbound) >> 1);
        KVSpair *element = pairs + index;
        int result = store->compare(key, element->key);
        if (result < 0) {
            rbound = index;
        } else if (result > 0) {
            lbound = index + 1;
        } else {
            return element;
        }
    }
    return exact ? NULL : pairs + lbound;
}

static KVSpair *kvs_get_pair(KVSstore *store, KVSkey *key) {
    if ((!store) || (!store->pairs)) {
        return NULL;
    }
    return kvs_search(store, key, 1);
}

static void kvs_abort_if_null(void *pointer, const char *message) {
    if (pointer == NULL) {
        fprintf(stderr, "%s\n", message);
        exit(-1);
    }
}

static void kvs_resize_pairs(KVSstore *store, size_t amount) {
    if (!store) {
        return;
    }
    store->length += amount;
    if (store->space > store->length) {
        return;
    }
    store->space += KVS_SPACE_INCREMENT;
    store->pairs = realloc(store->pairs, kvs_pair_size * store->space);
    kvs_abort_if_null(store->pairs, "out of memory");
}

static size_t kvs_get_pair_index(KVSstore *store, KVSpair *pair) {
    if ((!store) || (!pair)) {
        return -1;
    }
    return pair - store->pairs;
}

static size_t kvs_get_bytes_from_pair(KVSstore *store, KVSpair *pair) {
    size_t pair_index;
    if ((!store) || (!pair)) {
        return 0;
    }
    pair_index = kvs_get_pair_index(store, pair);
    return (store->length - pair_index) * kvs_pair_size;
}

static void kvs_create_pair(KVSstore *store, KVSkey *key, KVSvalue *value) {
    KVSpair *pair;
    if (!store) {
        return;
    }
    pair = kvs_search(store, key, 0);
    if (pair < store->pairs + store->length) {
        size_t bytes = kvs_get_bytes_from_pair(store, pair);
        memmove(pair + 1, pair, bytes);
    }
    pair->key = key;
    pair->value = value;
    kvs_resize_pairs(store, +1);
}

static void kvs_remove_pair(KVSstore *store, KVSpair *pair) {
    if ((!store) || (!pair)) {
        return;
    }
    memmove(pair, pair + 1, kvs_get_bytes_from_pair(store, pair + 1));
    kvs_resize_pairs(store, -1);
}

static int kvs_compare_pointers(const char *a, const char *b) {
    return a - b;
}

KVSstore *kvs_create(KVScompare compare) {
    KVSstore *store = malloc(kvs_store_size);
    kvs_abort_if_null(store, "out of memory");
    store->pairs = NULL;
    store->length = 0;
    store->space = 0;
    if (compare) {
        store->compare = compare;
    } else {
        store->compare = kvs_compare_pointers;
    }
    kvs_resize_pairs(store, 0);
    return store;
}

void kvs_destroy(KVSstore *store) {
    if (!store) {
        return;
    }
    if (store->pairs) {
        free(store->pairs);
    }
    free(store);
}

void kvs_put(KVSstore *store, KVSkey *key, void *value) {
    KVSpair *pair = kvs_get_pair(store, key);
    if (pair) {
        if (value) {
            pair->value = value;
        } else {
            kvs_remove_pair(store, pair);
        }
    } else {
        kvs_create_pair(store, key, value);
    }
}

KVSvalue *kvs_get(KVSstore *store, KVSkey *key) {
    KVSpair *pair = kvs_get_pair(store, key);
    return pair ? pair->value : NULL;
}

void kvs_remove(KVSstore *store, KVSkey *key) {
    kvs_put(store, key, NULL);
}

size_t kvs_length(KVSstore *store) {
    if (!store) {
        return 0;
    }
    return store->length;
}

KVSpair *kvs_pair(KVSstore *store, size_t index) {
    if ((!store) || (index < 0) || (index >= store->length)) {
        return NULL;
    }
    return store->pairs + index;
}


/*
 *
 *End of the code for the K/V store
 original webpage: https://codereview.stackexchange.com/questions/63493/simple-key-value-store-in-c-take-2
 *
 */

long s_counter=1, p_counter=1, o_counter=1;
long line_number=0;



char* dec2bin(int b, int n)
{

        // Size of an integer is assumed to be 'b' bits
        //
        char* res = malloc(b+1);
        int pi=0;
        for (int i = b; i >= 0; i--)
        {
                int k = n >> i;
                if (k & 1)
                        res[pi++]='1'; //printf("1");
                else
                        res[pi++]='0';//printf("0");

        }
        res[pi]='\0';

        //printf("\n counter:%d     bin:%s \n",n, res);

        return res;

}

size_t last_mc = 0;
KVSstore *store_SP_bin;
long bin_SP_counter=0;

KVSstore *store_SO_bin;
long bin_SO_counter=0;


void ___bdd_construct2(int _ss_b, int _pp_b, int _oo_b)
{

	char * bin_s = dec2bin(bit_subject_e, _ss_b ); 
	char * bin_p = dec2bin( ( (bit_predicate_e) - (bit_predicate_s)  ), _oo_b );
	char * bin_o = dec2bin( ( (bit_object_e) - (bit_object_s)  ), _pp_b );


	size_t mc;

	printf("s:%s \t p:%s \t o:%s \n", bin_s, bin_p, bin_o);
	//getchar();
	//
	bdd alpha;
        char* test = malloc(varnum+1);
	char* bin_SP = malloc(bit_object_s);
	char * bin_SO= malloc( bit_predicate_s + ( (bit_object_e) - (bit_object_s) +1 ) + 1);
	int SO_counter=1;


        if (bin_s[0] == '0')
                alpha = bdd_nithvar(0);
        else
                alpha = bdd_ithvar(0);
        test[0] = bin_s[0];
	
	bin_SP[0] = bin_s[0];
	bin_SO[0] = bin_s[0];



        for (int i=1; i<= (bit_subject_e);i++)
        {
                if (bin_s[i] == '0')
                        alpha = bdd_addref(bdd_apply( bdd_nithvar(i) , alpha , bddop_and ));
			       	
                else
                        alpha = bdd_addref(bdd_apply( bdd_ithvar(i) , alpha , bddop_and ));
			       	
                test[i] = bin_s[i];

		bin_SP[i] = bin_s[i];
		bin_SO[SO_counter++] = bin_s[i];
		


        }



       	for (int i=(bit_subject_e+1); i<=(bit_predicate_e);i++)
        {
                if (bin_p[i-(bit_subject_e+1)] == '0')
                        alpha = bdd_addref(bdd_apply( bdd_nithvar(i) , alpha , bddop_and ));
				
                else
                        alpha = bdd_addref(bdd_apply(  bdd_ithvar(i) , alpha , bddop_and ));

                test[i] = bin_p[i-(bit_subject_e+1)];

		bin_SP[i] = bin_p[i-(bit_subject_e+1)];

		bin_SO[SO_counter++] = '2';
		
	}




	bin_SP[bit_object_s]='\0';

	char * SP_dup = strdup(bin_SP);
	
	
	//bin_SP[bit_object_s]='\0';
	
	kvs_put(store_SP_bin, ++bin_SP_counter , SP_dup  );
	






        for (int i=(bit_predicate_e+1); i<=(bit_object_e);i++)
        {
                if (bin_o[i-(bit_predicate_e+1)] == '0')
                        alpha = bdd_addref(bdd_apply( bdd_nithvar(i) , alpha , bddop_and ));

                else
                        alpha = bdd_addref(bdd_apply(  bdd_ithvar(i) , alpha , bddop_and ));

                test[i] = bin_o[i-(bit_predicate_e+1)];
		bin_SO[SO_counter++] = bin_o[i-(bit_predicate_e+1)];
        }

	test[(varnum)] = '\0';


	bin_SO[SO_counter] = '\0';
       	char * SO_dup = strdup(bin_SO);
        kvs_put(store_SO_bin, ++bin_SO_counter , SO_dup  );



	kg = bdd_addref(bdd_apply(kg,alpha,bddop_or));
	bdd_delref(alpha);
	printf("\n triple observed (test): %s \n****************************************************\n ", test);
	free(test);

	

	//mc = bdd_satcount(kg);
	//printf("\n mc:\t%zu\n", mc);



	//if (mc <= last_mc)
	//	getchar();
	
	//last_mc = mc;





	


	
	









}





void ___bdd_construct(int _ss_b, int _pp_b, int _oo_b)
{

	char * bin_s = dec2bin(bit_subject_e, _ss_b ); 
	char * bin_p = dec2bin( ( (bit_predicate_e) - (bit_predicate_s)  ), _oo_b );
	char * bin_o = dec2bin( ( (bit_object_e) - (bit_object_s)  ), _pp_b );


	size_t mc;

	printf("s:%s \t p:%s \t o:%s \n", bin_s, bin_p, bin_o);
	//getchar();
	//
	bdd alpha;
        char* test = malloc(varnum+1);
	char* bin_SP = malloc(bit_object_s);
	//char * bin_SO= malloc( bit_predicate_s + ( (bit_object_e) - (bit_object_s) +1 ) + 1);
	//int SO_counter=1;


        if (bin_s[0] == '0')
                alpha = bdd_nithvar(0);
        else
                alpha = bdd_ithvar(0);
        test[0] = bin_s[0];
	
	bin_SP[0] = bin_s[0];
	//bin_SO[0] = bin_s[0];



        for (int i=1; i<= (bit_subject_e);i++)
        {
                if (bin_s[i] == '0')
                        alpha = bdd_apply( bdd_nithvar(i) , alpha , bddop_and );
			       	
                else
                        alpha = bdd_apply( bdd_ithvar(i) , alpha , bddop_and );
			       	
                test[i] = bin_s[i];

		bin_SP[i] = bin_s[i];
		//bin_SO[SO_counter++] = bin_s[i];
		


        }



       	for (int i=(bit_subject_e+1); i<=(bit_predicate_e);i++)
        {
                if (bin_p[i-(bit_subject_e+1)] == '0')
                        alpha = bdd_apply( bdd_nithvar(i) , alpha , bddop_and );
				
                else
                        alpha = bdd_apply(  bdd_ithvar(i) , alpha , bddop_and );

                test[i] = bin_p[i-(bit_subject_e+1)];

		bin_SP[i] = bin_p[i-(bit_subject_e+1)];

		//bin_SO[SO_counter++] = '2';
		
	}




	bin_SP[bit_object_s]='\0';

	char * SP_dup = strdup(bin_SP);
	
	
	//bin_SP[bit_object_s]='\0';
	
	kvs_put(store_SP_bin, ++bin_SP_counter , SP_dup  );
	






        for (int i=(bit_predicate_e+1); i<=(bit_object_e);i++)
        {
                if (bin_o[i-(bit_predicate_e+1)] == '0')
                        alpha = bdd_apply( bdd_nithvar(i) , alpha , bddop_and );

                else
                        alpha = bdd_apply(  bdd_ithvar(i) , alpha , bddop_and );

                test[i] = bin_o[i-(bit_predicate_e+1)];
		//bin_SO[SO_counter++] = bin_o[i-(bit_predicate_e+1)];
        }

	test[(varnum)] = '\0';


	//bin_SO[SO_counter] = '\0';
       	//char * SO_dup = strdup(bin_SO);
        //kvs_put(store_SO_bin, ++bin_SO_counter , SO_dup  );



	kg = bdd_addref(bdd_apply(kg,alpha,bddop_or));
	
	printf("\n triple observed (test): %s \n****************************************************\n ", test);
	free(test);

	

	//mc = bdd_satcount(kg);
	//printf("\n mc:\t%zu\n", mc);



	//if (mc <= last_mc)
	//	getchar();
	
	//last_mc = mc;





	


	
	









}





KVSstore *s_store_str_id;
KVSstore *s_store_id_str;

KVSstore *p_store_str_id;
KVSstore *p_store_id_str;

KVSstore *o_store_str_id;
KVSstore *o_store_id_str;



void triple_construct(char* _ss, char* _pp, char* _oo)
{

	if ( kvs_get(s_store_str_id, _ss) == NULL)
	{
		kvs_put(s_store_str_id, _ss, s_counter);
		char* sdup = strdup(_ss);
		int * ints = malloc(sizeof(int));
		ints = s_counter;
		kvs_put(s_store_id_str, ints ,sdup);
		
		s_counter++;
	}


        
        if (kvs_get(p_store_str_id, _pp) == NULL )
        {
                kvs_put(p_store_str_id, _pp, p_counter);
	        char* pdup = strdup(_pp);
                int * intp = malloc(sizeof(int));
                intp = p_counter;
                kvs_put(p_store_id_str, intp ,pdup);

                p_counter++;

        }


        
        if (kvs_get(o_store_str_id, _oo) == NULL)
        {
                kvs_put(o_store_str_id, _oo, o_counter);
     		char* odup = strdup(_oo);
                int * into = malloc(sizeof(int));
                into = o_counter;
                kvs_put(o_store_id_str, into ,odup);

                o_counter++;

        }

	int slk = kvs_get(s_store_str_id, _ss);
	int plk = kvs_get(p_store_str_id, _pp);
	int olk = kvs_get(o_store_str_id, _oo);
	
	___bdd_construct(slk, plk, olk);
	


}



















FILE *fp = NULL;

void main_kb (char *argv)
{
	  char subject[st_size], tr_predicate[st_size], object[st_size];
	  int readfile = 0;
	  //size_t line_number=0;
	  bool flag_bnode = false;
  	  char *check_ws;
  	  char gt = '>';
  	  long fpos;
  

	
	  
  	  fp = fopen(argv, "r");
  	  if (fp == NULL)
  	  {
  		  printf ("Error opening the file\n\n");
  		  exit(EXIT_FAILURE);
  	  } else
  	  {
		  while (readfile != EOF)
		  {
			  line_number++;

			  fpos = ftell(fp);
  			  readfile = fscanf(fp, "%s %s %[^\n]\n", subject, tr_predicate, object);
  
			  if (subject[0] == '_' && subject[1]==':')
  				  flag_bnode = true;
  			  else
  				  check_ws = strstr(subject,">");
  
			  if (!flag_bnode && check_ws == NULL)
  			  {
				fseek(fp, fpos, SEEK_SET);
				readfile = fscanf(fp, "%[^>]> %s %[^\n]\n", subject, tr_predicate, object);
				strncat(subject, &gt, 1);
			
			  }
  			  flag_bnode = false;

			  object[(strlen(object)-1)]='\0';

			  if (readfile == 3)
			  {
				    #if (DEBUG == 1)  
				  {
  				
					  printf("line:%zu \n s: %s \n  p:%s \n o:%s \n \n\n\n ", line_number, subject, tr_predicate, object);
					  printf("s_l: %d \t  p_l:%d \t o_l:%d \n ", strlen(subject), strlen(tr_predicate), strlen(object));

				
				  }
				    #endif

				  char* sdup = strdup(subject);
				  char* pdup = strdup(tr_predicate);
				  char* odup = strdup(object);

				  triple_construct(sdup , pdup, odup);


				  
				 
				  


			  }


		  }
	  
	  
	  }




}




/**************************************************************************
  Example of allsat print handler.
**************************************************************************/
bool flag= false;

void allsatHandlerPrint(char* varset, int size)
{


  for (int v=0; v<size ; ++v)
  {
	  if (varset[v] < 0)
		  printf("%c",'X');
	  else
		  printf("%c",(char)('0' + varset[v]));

  }
  //if (flag)
  //	  getchar();
  printf("\n");
  //flag = true;
  
}



/**************************************************************************
  allsat handler for checking that all assignments are detected.
**************************************************************************/

static bdd allsatBDD;
static bdd allsatSumBDD;

void allsatHandler(char* varset, int size)
{
  bdd x = bddtrue;
  for (int v=0 ; v<size ; ++v)
    if (varset[v] == 0)
      x = bdd_apply(x,bdd_nithvar(v),bddop_and);
    else if (varset[v] == 1)
      x = bdd_apply(x,bdd_ithvar(v),bddop_and);


  // Sum up all assignments
  allsatSumBDD = bdd_apply(x,allsatSumBDD,bddop_or);


  // Remove assignment from initial set
  allsatBDD = bdd_addref(bdd_apply( allsatBDD, x ,bddop_diff));


}



void allsatSPO()
{

 
       	char* bin_q;
        bdd alpha = bddtrue;
        bdd beta = kg;

        for (long k=1; k < bin_SP_counter  ; k++ )
        {
                bin_q = kvs_get(store_SP_bin, k  );


                //printf("\n bin:%s length:%d \n", bin_q, strlen(bin_q));

                for (int v=0; v < bit_object_s ; v++)
                {
                        if (bin_q[v] == '0')
                                alpha = bdd_addref(bdd_apply( bdd_nithvar(v) , alpha , bddop_and ));    
                        else
                                alpha = bdd_addref(bdd_apply( bdd_ithvar(v)  , alpha , bddop_and ));
                }

                bdd RES = bdd_addref( bdd_restrict(beta, alpha));
                //flag = false;

                bdd_allsat(RES, allsatHandlerPrint);

                //flag = false;

                alpha = bddtrue;
                beta = kg;
                //printf("\n");

                //getchar();

        }



	


}








void allsatSOP()
{


        char* bin_q;
        bdd alpha = bddtrue;
        bdd beta = kg;

        for (long k=1; k < bin_SO_counter  ; k++ )
        {
                bin_q = kvs_get(store_SO_bin, k  );


                //printf("\n bin:%s length:%d \n", bin_q, strlen(bin_q));

                for (int v=0; v <= bit_object_e ; v++)
                {
			if (bin_q[v] != '2')
			{
				if (bin_q[v] == '0')
					alpha = bdd_addref(bdd_apply( bdd_nithvar(v) , alpha , bddop_and ));
				else
					alpha = bdd_addref(bdd_apply( bdd_ithvar(v)  , alpha , bddop_and ));
			}
                }

                bdd RES = bdd_addref( bdd_restrict(beta, alpha));
                //flag = false;

                bdd_allsat(RES, allsatHandlerPrint);

                //flag = false;

                alpha = bddtrue;
                beta = kg;
                //printf("\n");

                //getchar();

        }






}


void test1_check(bdd x)
{
  //double anum = bdd_satcount(x);
 
  //printf("Checking bdd with\t%f\t assignments:\n",anum);

    
  //allsatBDD = x;
  //allsatSumBDD = bddfalse;

  // Calculate whole set of asignments and remove all assignments
  // from original set

  //bdd_allsat(x, allsatHandler);

  bdd_allsat(x, allsatSPO);



  /*

  //bdd_allsat(x, allsatHandlerPrint  );
  

  // Now the summed set should be equal to the original set
  if (allsatSumBDD == x)
    printf( "  Sum-OK. ");
  else
    printf( "  Sum-ERROR. ");

  // The subtracted set should be empty
  if (allsatBDD == bddfalse)
    printf("Sub-OK.\n");
  else
    printf("Sub-ERROR.\n");

    */


}













int main(void)
{

	
	size_t model_count;

	//bdd_init(10000000000,100000000); //100 000 000 000 killed 64G

	//bdd_init(10000,1000);

	bdd_init(1000000,100000); 

	//bdd_init(1000000000,100000000);
	//
	//
	bdd_setcacheratio(64);
     	bdd_setmaxincrease(5000000);



	//
	bdd_setvarnum(varnum);

	kg = bddfalse;

	s_store_str_id = kvs_create(strcmp);
	p_store_str_id = kvs_create(strcmp);
	o_store_str_id = kvs_create(strcmp);

	s_store_id_str = kvs_create(NULL);
	p_store_id_str = kvs_create(NULL);
	o_store_id_str = kvs_create(NULL);

	store_SP_bin = kvs_create(NULL);

        store_SO_bin = kvs_create(NULL);


	//main_kb("/home/ubuntu/ds_compare/limitedBsbm.nt");
	//main_kb("/home/ubuntu/ds_compare/testds.nt");
	//main_kb("/home/ubuntu/ds_compare/bsbm.nt");


	//main_kb("/home/ubuntu/bsbm-dataset/bsbm86m-sorted.nt");
	//
	//main_kb("/home/ubuntu/icwe2021datasets/bsbm/bsbm100k_sorted.nt");
	//
	//main_kb("/home/ubuntu/icwe2021datasets/lubm/lubm100k_sorted.nt");
	//main_kb("/home/ubuntu/icwe2021datasets/sp2b/sp2b100k_sorted.nt");
	//
	//

   	//main_kb("/home/ubuntu/icwe2021datasets/bsbm/bsbm1m_sorted.nt");
        //main_kb("/home/ubuntu/icwe2021datasets/lubm/lubm1m_sorted.nt");
        //main_kb("/home/ubuntu/icwe2021datasets/sp2b/sp2b1m_sorted.nt");

        //main_kb("/home/ubuntu/icwe2021datasets/bsbm/bsbm10m_sorted.nt");
        //main_kb("/home/ubuntu/icwe2021datasets/lubm/lubm10m_sorted.nt");
        main_kb("/home/ubuntu/icwe2021datasets/sp2b/sp2b10m_sorted.nt");





	
	//int nDigits = floor(log10(abs(p_counter))) + 1;
	long long SP = ((s_counter-1) * (pow(10,N_DIGITS_P))) + (p_counter-1);
 


	 printf("#s:%d \t #p:%d \t #o:%d \n", (s_counter-1), (p_counter-1), (o_counter-1));
         printf("\n last SP:%d \n", SP);
	 //getchar();



	//test1_check(kg);
	
	 

	double time_spent = 0.0;
    	clock_t begin = clock();


	time_t wc_begin = time(NULL);

	//bdd_allsat( kg , allsatHandlerPrint  );
	//bdd_allsat(kg, allsatHandler);
	//test1_check(kg);
	//



	//bdd_allsat(kg, allsatHandlerPrint);



	allsatSPO();
	//
	//allsatSOP();



	time_t wc_end = time(NULL);

	printf("WALL CLOCK - Time elpased is %d secondsi\n", (wc_end - wc_begin));


	clock_t end = clock();
    	// calculate elapsed time by finding difference (end - begin) and
	// dividing the difference by CLOCKS_PER_SEC to convert to seconds
    
	time_spent += (double)(end - begin) / CLOCKS_PER_SEC;
    	printf("CLOCK OS - Time elpased is %f seconds\n", time_spent);



	
	//for (long i=1; i< s_counter; i++)
	//	printf("\n subject %i: %s\n", i, kvs_get(s_store_id_str, i));
	




	kvs_destroy(s_store_str_id);
	kvs_destroy(p_store_str_id);
	kvs_destroy(o_store_str_id);

	kvs_destroy(s_store_id_str);
	kvs_destroy(p_store_id_str);
	kvs_destroy(o_store_id_str);
	return 0;
}








































































