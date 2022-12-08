
#include<stdio.h>
#include<stdlib.h>
#include "mapreduce.h"
#include<string.h>
#include<pthread.h>
#include <unistd.h>

/////////////////////////////////////////////////////////////////////////////////
//Global Variables
/////////////////////////////////////////////////////////////////////////////////

#define TABLE_SIZE  1400

struct table** p; 
Partitioner partitioner;
Mapper mapper;
Reducer reducer;
int partition_number;
pthread_mutex_t filelock=PTHREAD_MUTEX_INITIALIZER;
int fileNumber;
int current_file;
int current_partition;
pthread_mutex_t *partitionlock;
struct node** reducelist;

////////////////////////////////////////////////////////////////////////////////
//HASH STUFF
///////////////////////////////////////////////////////////////////////////////

//Bucket Element
//key list
struct node{
    char* key;
    struct node *next;
    struct valnode *head;
    struct valnode *current;
    int size;
};

//Main Hash Table
struct table{
    int size;
    struct node **list;
    pthread_mutex_t *locks;
    long long nodesize;
    pthread_mutex_t keylock;

};


//value node
struct valnode{
    char* val;
    struct valnode  *next;
};

//Function used to instantiate the table
struct table *init_table(int size){
    struct table *t = (struct table*)malloc(sizeof(struct table));
    t->size = size;
    t->list = malloc(sizeof(struct node*)*size);
    t->locks=malloc(sizeof(pthread_mutex_t)*size);
    pthread_mutex_init(&t->keylock,NULL);
    t->nodesize=0;
    int i;
    for(i=0;i<size;i++){
        t->list[i] = NULL;
        if(pthread_mutex_init(&(t->locks[i]), NULL)!=0)
            printf("Locks init failed\n");
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
     hash=hash % TABLE_SIZE;
     return hash;
 }

//Insert into the hash map.
void add_to_list(struct table *t,char* key,char* val){  
    long long pos = rehash(key);
    pthread_mutex_t *lock = t->locks + pos;
    struct valnode *newValNode=malloc(sizeof(struct valnode));
    newValNode->val=strdup(val);
    newValNode->next=NULL;
    pthread_mutex_lock(lock);
    struct node *list = t->list[pos];
    struct node *curnode = list;
    while(curnode){
        if(strcmp(curnode->key,key)==0){

            //insert the newnode to the head
            struct valnode *sublist = curnode->head;
            newValNode->next=sublist;
            curnode->head = newValNode;
            curnode->current = newValNode;
            curnode->size++;
            pthread_mutex_unlock(lock);
            return;
        }
        curnode = curnode->next;
    }
    pthread_mutex_lock(&t->keylock);
    t->nodesize++;
    pthread_mutex_unlock(&t->keylock);
    struct node *newNode = malloc(sizeof(struct node));
    newNode->key = strdup(key);
    newNode->head = newValNode;
    newNode->current = newValNode;  
    newNode->next = list;
    t->list[pos] = newNode;
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
    struct node *curnode= reducelist[partition_num];
    struct valnode *addr = curnode->current;
    if(addr==NULL){
        return NULL;
      }
     curnode->current=addr->next;
     return addr->val;
}


void* callMap(char *fileName){
    mapper(fileName);
    return NULL;
}

void reduceHelper(int i){
    if(p[i]==NULL)
        return;
    struct table* tempTable=p[i];
    struct node *list[p[i]->nodesize];
    long long x=0;
    for(int j=0;j<TABLE_SIZE;j++){
        if(tempTable->list[j] ==NULL)
            continue;
        struct node* tempNode=tempTable->list[j];
        while(tempNode){
            list[x]=tempNode;
            x++;
            tempNode=tempNode->next;
        }
    }
    qsort(list,p[i]->nodesize,sizeof(struct node *),cmp);
    for(int k=0;k<x;k++){
        reducelist[i]=list[k];
        reducer(list[k]->key, get_func, i);
    }
}
        
         
void* callReduce(){
    while(1){
        pthread_mutex_lock(&filelock);
        int x;
        if(current_partition>=partition_number){
            pthread_mutex_unlock(&filelock);
            return NULL;
        }
        if(current_partition<partition_number){
            x=current_partition;
            current_partition++;
        }
        pthread_mutex_unlock(&filelock);
	    reduceHelper(x);
    }    
}

void *findFile(void *files)
{    
    char **arguments = (char**) files;
    while(1){
        pthread_mutex_lock(&filelock);
        int x;
        if(current_file>fileNumber){
            pthread_mutex_unlock(&filelock); 
            return NULL;
        }
        if(current_file<=fileNumber){
            x=current_file;
            current_file++;
        }
        pthread_mutex_unlock(&filelock); 
        callMap(arguments[x]);  
    }
    return NULL;
}

//////////////////////////////////////////////////////////////////////////
// Master
//////////////////////////////////////////////////////////////////////////

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
    if(partitioner!=NULL){
        partitionNumber = partitioner(key,partition_number);
    }
    else{
        partitionNumber= MR_DefaultHashPartition(key,partition_number);
    } 
    add_to_list(p[partitionNumber],key,value);    
}

////////////////////////////////////////////////////////////////////////////
// Memory Cleanup
////////////////////////////////////////////////////////////////////////////

void mem_free(struct table *t)
{
    for(int i=0;i<t->size;i++)
    {
	    struct node *list1=t->list[i];
        struct node *temp2=list1;
        pthread_mutex_t *l=&(t->locks[i]);
        while(temp2)
        { 	
            struct node *tempNode=temp2;
            struct valnode *temp=tempNode->head;
            while(temp){
                struct valnode *sublist=temp;
                temp=temp->next; 
		        free(sublist->val);	
                free(sublist);
            }
            temp2=temp2->next;
		    free(tempNode->key);
            free(tempNode); 
	    }
	    pthread_mutex_destroy(l);
    }
    free(t->locks);
    free(t->list);
    free(t);
}

////////////////////////////////////////////////////////////////////////////
/* Main */
///////////////////////////////////////////////////////////////////////////

void MR_Run(int argc, char *argv[], Mapper map, int num_mappers, Reducer reduce, int num_reducers, 
	        Partitioner partition)
{   
    //printf("IN MR_RUN\n");
    current_partition=0;
    current_file=1;
    partitioner=partition;
    mapper=map;
    reducer=reduce;
    partition_number=num_reducers; //Number of partitions
    fileNumber=argc-1;
    p=malloc(sizeof(struct table *)*num_reducers);
    
    //Create Partitions
    for(int i=0;i<partition_number;i++){
        struct table *table = init_table(TABLE_SIZE);
        p[i]=table;
    }
    
    //Start Mapping Process
    pthread_t mappers[num_mappers];
    for(int i=0;i<num_mappers || i==argc-1;i++){
         pthread_create(&mappers[i], NULL,findFile,(void*) argv);
    } 
    //Join the Mappers       
    for(int i=0;i<num_mappers || i==argc-1;i++)//Start joining all the threads
    {
            pthread_join(mappers[i], NULL);
    }

    //Start Reducing Process
    pthread_t reducer[num_reducers];
    reducelist = malloc(sizeof(struct node *) * num_reducers);
    for(int i=0;i<num_reducers;i++){
        pthread_create(&reducer[i],NULL,callReduce,NULL);
    }
    
    //Join the Reducers
    for(int i=0;i<num_reducers;i++){
        pthread_join(reducer[i],NULL);
    }
    
    //Memory clean-up
    for(int i=0;i<partition_number;i++)
	{	
  	 	mem_free(p[i]);
	}
    free(partitionlock);
    free(reducelist);
    free(p);
}    
    