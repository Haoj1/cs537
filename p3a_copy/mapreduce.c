#include <stdio.h>
#include <stdlib.h>
#include "mapreduce.h"
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>

#define TABLE_SIZE  1400

struct table** tables; 
Partitioner partitioner;
int partition_number;
pthread_mutex_t filelock=PTHREAD_MUTEX_INITIALIZER;
int fileNumber;
int current_file;
int current_partition;
pthread_mutex_t *partitionlock;
struct node** reducelist;
Mapper mapper;
Reducer reducer;

//key list
struct node{
    char* key;
    struct node *next;
    struct valnode *head;
    struct valnode *current;
    pthread_mutex_t listlock;
    int size;
};

//Main Hash Table
struct table{
    int size;
    struct node **list;
    struct node **linkedlist;
    pthread_mutex_t *locks;
    pthread_mutex_t keylock;
    long long nodesize;
};


//value node
struct valnode{
    char* val;
    struct valnode *next;
};

//Function used to instantiate the table
struct table *init_table(int size){
    struct table *t = (struct table*)malloc(sizeof(struct table));
    t->size = size;
    t->list = calloc(size, sizeof(struct node*));
    t->linkedlist = calloc(size, sizeof(struct node*));
    t->locks = malloc(sizeof(pthread_mutex_t) * size);
    pthread_mutex_init(&t->keylock ,NULL);
    t->nodesize=0;
    int i;
    for(i = 0; i < size; i++){
        // t->list[i] = NULL;
        t->linkedlist[i] = (struct node*)malloc(sizeof(struct node));
        int rc = pthread_mutex_init(&(t->linkedlist[i]->listlock), NULL);
        assert(rc == 0); 
        rc = pthread_mutex_init(&(t->locks[i]), NULL);
        assert(rc == 0); 
    }
    return t;
}

 unsigned long long
 rehash(char *str)
 {
     unsigned long long hash = 5381;
     int c;
 
     while((c = *str++)!= '\0'){
         hash = hash * 33 + c; /* hash * 33 + c */
     }
     hash = hash % TABLE_SIZE;
     return hash;
 }

//Insert into the hash map.
void add_to_list(struct table *t,char* key,char* val){  
    long long index = rehash(key);
    pthread_mutex_t *lock = &t->linkedlist[index]->listlock;
    struct valnode *newValNode = malloc(sizeof(struct valnode));
    newValNode->val = strdup(val);
    newValNode->next = NULL;
    pthread_mutex_lock(lock);
    struct node *list = t->list[index];
    struct node *curnode = list;
    while(curnode){
        if(strcmp(curnode->key, key)==0){
            //insert the newnode to the head
            struct valnode *sublist = curnode->head;
            newValNode->next = sublist;
            curnode->head = newValNode;
            curnode->current = newValNode;
            curnode->size++;
            pthread_mutex_unlock(lock);
            return;
        }
        curnode = curnode->next;
    }
    struct node *newNode = malloc(sizeof(struct node));
    newNode->key = strdup(key);
    newNode->head = newValNode;
    newNode->current = newValNode;  
    newNode->next = list;
    t->list[index] = newNode;
    pthread_mutex_lock(&t->keylock);
    t->nodesize++;
    pthread_mutex_unlock(&t->keylock);
    pthread_mutex_unlock(lock);
}


 int cmp(const void *key1, const void *key2)
 {
    struct node **n1 = (struct node **)key1;
    struct node **n2 = (struct node **)key2;
    if(*n1==NULL && *n2==NULL) {
         return 0;
    } 
    if(*n1==NULL) {
         return -1;
     } 
    if(*n2==NULL) {
         return 1;
    } 
    return strcmp((*n1)->key,(*n2)->key);    
 }


//Getter function that returns the next key in a subnode list.
char* get_func(char* key, int partition_num)
{
    struct node *curnode = reducelist[partition_num];
    struct valnode *val = curnode->current;
    if(val == NULL){
        return NULL;
      }
     curnode->current = val->next;
     return val->val;
}
       
void* reduce_call(){
    for( ; ; ) {
        pthread_mutex_lock(&filelock);
        int cur_partition;
        if(current_partition >= partition_number){
            pthread_mutex_unlock(&filelock);
            return NULL;
        }
        if(current_partition < partition_number){
            cur_partition = current_partition;
            current_partition++;
        }
        pthread_mutex_unlock(&filelock);
        if(tables[cur_partition]==NULL)
            break;
        struct table* tempTable=tables[cur_partition];
        struct node *list[tables[cur_partition]->nodesize];
        int reduce_times = 0;
        for(int j = 0; j < TABLE_SIZE; j++){
            if(tempTable->list[j] == NULL)
                continue;
            struct node* tempNode = tempTable->list[j];
            while(tempNode != NULL){
                list[reduce_times]=tempNode;
                reduce_times++;
                tempNode=tempNode->next;
            }
        }
        qsort(list, tables[cur_partition]->nodesize, sizeof(struct node *), cmp);
        for(int i = 0; i < reduce_times; i++){
            reducelist[cur_partition] = list[i];
            reducer(list[i]->key, get_func, cur_partition);
        }
    }
    return NULL;
}

void *get_map(void *files)
{    
    char **arguments = (char**) files;
    while(1){
        pthread_mutex_lock(&filelock);
        int file_index;
        if(current_file > fileNumber){
            pthread_mutex_unlock(&filelock); 
            return NULL;
        }
        if(current_file <= fileNumber){
            file_index = current_file;
            current_file++;
        }
        pthread_mutex_unlock(&filelock); 
        mapper(arguments[file_index]);  
    }
    return NULL;
}


//Default Hash partitioner
unsigned long MR_DefaultHashPartition(char *key, int num_partitions) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++) != '\0')
        hash = hash * 33 + c;
    return hash % num_partitions;
    //return 1;
}

//
void MR_Emit(char *key, char *value){ //Key -> tocket, Value -> 1
    long partitionNumber;
    //printf("In MR_EMIT\n");
    if(partitioner != NULL){
        partitionNumber = partitioner(key, partition_number);
    }
    else{
        partitionNumber = MR_DefaultHashPartition(key, partition_number);
    } 
    add_to_list(tables[partitionNumber],key,value);    
}


void free_table(struct table *t)
{
    for(int i=0;i<t->size;i++)
    {
	    struct node *curnode = t->list[i];
        pthread_mutex_t *l=&(t->locks[i]);
        while(curnode)
        { 	
            struct node *tempNode = curnode;
            struct valnode *temp = tempNode->head;
            while(temp){
                struct valnode *sublist=temp;
                temp = temp->next; 
		        free(sublist->val);	
                free(sublist);
            }
            curnode = curnode->next;
		    free(tempNode->key);
            free(tempNode); 
	    }
	    pthread_mutex_destroy(l);
    }
    free(t->locks);
    free(t->list);
    free(t);
}

void mem_free() {
     //Memory clean-up
    for(int i=0;i < partition_number;i++)
	{	
  	 	free_table(tables[i]);
	}
    free(partitionlock);
    free(reducelist);
    free(tables);
}


void MR_Run(int argc, char *argv[], Mapper map, int num_mappers, Reducer reduce, int num_reducers, 
	        Partitioner partition)
{   
    current_partition = 0;
    current_file = 1;
    partitioner = partition;
    mapper = map;
    reducer = reduce;
    partition_number = num_reducers;
    fileNumber = argc-1;
    tables = malloc(sizeof(struct table *)*num_reducers);
    //Create Partitions
    for(int i=0; i < partition_number; i++){
        struct table *table = init_table(TABLE_SIZE);
        tables[i]=table;
    }
    
    //Start Mapping Process
    pthread_t mappers[num_mappers];
    pthread_t reducer[num_reducers];
    for(int i=0;i<num_mappers || i==argc-1;i++){
         pthread_create(&mappers[i], NULL, get_map, (void*) argv);
    } 
    //Join the Mappers       
    for(int i=0;i<num_mappers || i==argc-1;i++)//Start joining all the threads
    {
            pthread_join(mappers[i], NULL);
    }

    //Start Reducing Process
    reducelist = malloc(sizeof(struct node *) * num_reducers);
    for(int i=0;i < num_reducers; i++){
        pthread_create(&reducer[i], NULL, reduce_call, NULL);
    }
    
    //Join the Reducers
    for(int i = 0; i < num_reducers; i++){
        pthread_join(reducer[i], NULL);
    }

    mem_free();
}    
    